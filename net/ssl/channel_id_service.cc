// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/channel_id_service.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "crypto/ec_private_key.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "url/gurl.h"

namespace net {

namespace {

base::AtomicSequenceNumber g_next_id;

// On success, returns a ChannelID object and sets |*error| to OK.
// Otherwise, returns NULL, and |*error| will be set to a net error code.
// |serial_number| is passed in because base::RandInt cannot be called from an
// unjoined thread, due to relying on a non-leaked LazyInstance
std::unique_ptr<ChannelIDStore::ChannelID> GenerateChannelID(
    const std::string& server_identifier,
    int* error) {
  std::unique_ptr<ChannelIDStore::ChannelID> result;

  base::Time creation_time = base::Time::Now();
  std::unique_ptr<crypto::ECPrivateKey> key(crypto::ECPrivateKey::Create());

  if (!key) {
    DLOG(ERROR) << "Unable to create channel ID key pair";
    *error = ERR_KEY_GENERATION_FAILED;
    return result;
  }

  result.reset(new ChannelIDStore::ChannelID(server_identifier, creation_time,
                                             std::move(key)));
  *error = OK;
  return result;
}

}  // namespace

// ChannelIDServiceWorker takes care of the blocking process of performing key
// generation. Will take care of deleting itself once Start() is called.
class ChannelIDServiceWorker {
 public:
  typedef base::OnceCallback<
      void(const std::string&, int, std::unique_ptr<ChannelIDStore::ChannelID>)>
      WorkerDoneCallback;

  ChannelIDServiceWorker(const std::string& server_identifier,
                         WorkerDoneCallback callback)
      : server_identifier_(server_identifier),
        origin_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        callback_(std::move(callback)) {}

  // Starts the worker asynchronously.
  void Start(const scoped_refptr<base::TaskRunner>& task_runner) {
    DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

    auto callback =
        base::BindOnce(&ChannelIDServiceWorker::Run, base::Owned(this));

    if (task_runner) {
      task_runner->PostTask(FROM_HERE, std::move(callback));
    } else {
      base::PostTaskWithTraits(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          std::move(callback));
    }
  }

 private:
  void Run() {
    // Runs on a worker thread.
    int error = ERR_FAILED;
    std::unique_ptr<ChannelIDStore::ChannelID> channel_id =
        GenerateChannelID(server_identifier_, &error);
    origin_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), server_identifier_,
                                  error, base::Passed(&channel_id)));
  }

  const std::string server_identifier_;
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  WorkerDoneCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ChannelIDServiceWorker);
};

// A ChannelIDServiceJob is a one-to-one counterpart of an
// ChannelIDServiceWorker. It lives only on the ChannelIDService's
// origin task runner's thread.
class ChannelIDServiceJob {
 public:
  ChannelIDServiceJob(bool create_if_missing)
      : create_if_missing_(create_if_missing) {
  }

  ~ChannelIDServiceJob() { DCHECK(requests_.empty()); }

  void AddRequest(ChannelIDService::Request* request,
                  bool create_if_missing = false) {
    create_if_missing_ |= create_if_missing;
    requests_.push_back(request);
  }

  void HandleResult(int error, std::unique_ptr<crypto::ECPrivateKey> key) {
    PostAll(error, std::move(key));
  }

  bool CreateIfMissing() const { return create_if_missing_; }

  void CancelRequest(ChannelIDService::Request* req) {
    auto it = std::find(requests_.begin(), requests_.end(), req);
    if (it != requests_.end())
      requests_.erase(it);
  }

 private:
  void PostAll(int error, std::unique_ptr<crypto::ECPrivateKey> key) {
    std::vector<ChannelIDService::Request*> requests;
    requests_.swap(requests);

    for (auto i = requests.begin(); i != requests.end(); i++) {
      std::unique_ptr<crypto::ECPrivateKey> key_copy;
      if (key)
        key_copy = key->Copy();
      (*i)->Post(error, std::move(key_copy));
    }
  }

  std::vector<ChannelIDService::Request*> requests_;
  bool create_if_missing_;
};

ChannelIDService::Request::Request() : service_(NULL) {
}

ChannelIDService::Request::~Request() {
  Cancel();
}

void ChannelIDService::Request::Cancel() {
  if (service_) {
    callback_.Reset();
    job_->CancelRequest(this);

    service_ = NULL;
  }
}

void ChannelIDService::Request::RequestStarted(
    ChannelIDService* service,
    CompletionOnceCallback callback,
    std::unique_ptr<crypto::ECPrivateKey>* key,
    ChannelIDServiceJob* job) {
  DCHECK(service_ == NULL);
  service_ = service;
  callback_ = std::move(callback);
  key_ = key;
  job_ = job;
}

void ChannelIDService::Request::Post(
    int error,
    std::unique_ptr<crypto::ECPrivateKey> key) {
  service_ = NULL;
  DCHECK(!callback_.is_null());
  if (key)
    *key_ = std::move(key);
  // Running the callback might delete |this| (e.g. the callback cleans up
  // resources created for the request), so we can't touch any of our
  // members afterwards. Reset callback_ first.
  base::ResetAndReturn(&callback_).Run(error);
}

ChannelIDService::ChannelIDService(ChannelIDStore* channel_id_store)
    : channel_id_store_(channel_id_store),
      id_(g_next_id.GetNext()),
      requests_(0),
      key_store_hits_(0),
      inflight_joins_(0),
      workers_created_(0),
      weak_ptr_factory_(this) {}

ChannelIDService::~ChannelIDService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// static
std::string ChannelIDService::GetDomainForHost(const std::string& host) {
  std::string domain =
      registry_controlled_domains::GetDomainAndRegistry(
          host, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty())
    return host;
  return domain;
}

int ChannelIDService::GetOrCreateChannelID(
    const std::string& host,
    std::unique_ptr<crypto::ECPrivateKey>* key,
    CompletionOnceCallback callback,
    Request* out_req) {
  DVLOG(1) << __func__ << " " << host;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (callback.is_null() || !key || host.empty()) {
    return ERR_INVALID_ARGUMENT;
  }

  std::string domain = GetDomainForHost(host);
  if (domain.empty()) {
    return ERR_INVALID_ARGUMENT;
  }

  requests_++;

  // See if a request for the same domain is currently in flight.
  bool create_if_missing = true;
  if (JoinToInFlightRequest(domain, key, create_if_missing, &callback,
                            out_req)) {
    return ERR_IO_PENDING;
  }

  int err = LookupChannelID(domain, key, create_if_missing, &callback, out_req);
  if (err == ERR_FILE_NOT_FOUND) {
    // Sync lookup did not find a valid channel ID.  Start generating a new one.
    workers_created_++;
    ChannelIDServiceWorker* worker = new ChannelIDServiceWorker(
        domain, base::BindOnce(&ChannelIDService::GeneratedChannelID,
                               weak_ptr_factory_.GetWeakPtr()));
    worker->Start(task_runner_);

    // We are waiting for key generation.  Create a job & request to track it.
    ChannelIDServiceJob* job = new ChannelIDServiceJob(create_if_missing);
    inflight_[domain] = base::WrapUnique(job);

    job->AddRequest(out_req);
    out_req->RequestStarted(this, std::move(callback), key, job);
    return ERR_IO_PENDING;
  }

  return err;
}

int ChannelIDService::GetChannelID(const std::string& host,
                                   std::unique_ptr<crypto::ECPrivateKey>* key,
                                   CompletionOnceCallback callback,
                                   Request* out_req) {
  DVLOG(1) << __func__ << " " << host;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (callback.is_null() || !key || host.empty()) {
    return ERR_INVALID_ARGUMENT;
  }

  std::string domain = GetDomainForHost(host);
  if (domain.empty()) {
    return ERR_INVALID_ARGUMENT;
  }

  requests_++;

  // See if a request for the same domain currently in flight.
  bool create_if_missing = false;
  if (JoinToInFlightRequest(domain, key, create_if_missing, &callback,
                            out_req)) {
    return ERR_IO_PENDING;
  }

  int err = LookupChannelID(domain, key, create_if_missing, &callback, out_req);
  return err;
}

void ChannelIDService::GotChannelID(int err,
                                    const std::string& server_identifier,
                                    std::unique_ptr<crypto::ECPrivateKey> key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto j = inflight_.find(server_identifier);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }

  if (err == OK) {
    // Async DB lookup found a valid channel ID.
    key_store_hits_++;
    // ChannelIDService::Request::Post will do the histograms and stuff.
    HandleResult(OK, server_identifier, std::move(key));
    return;
  }
  // Async lookup failed or the channel ID was missing. Return the error
  // directly, unless the channel ID was missing and a request asked to create
  // one.
  if (err != ERR_FILE_NOT_FOUND || !j->second->CreateIfMissing()) {
    HandleResult(err, server_identifier, std::move(key));
    return;
  }
  // At least one request asked to create a channel ID => start generating a new
  // one.
  workers_created_++;
  ChannelIDServiceWorker* worker = new ChannelIDServiceWorker(
      server_identifier, base::BindOnce(&ChannelIDService::GeneratedChannelID,
                                        weak_ptr_factory_.GetWeakPtr()));
  worker->Start(task_runner_);
}

ChannelIDStore* ChannelIDService::GetChannelIDStore() {
  return channel_id_store_.get();
}

void ChannelIDService::GeneratedChannelID(
    const std::string& server_identifier,
    int error,
    std::unique_ptr<ChannelIDStore::ChannelID> channel_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<crypto::ECPrivateKey> key;
  if (error == OK) {
    key = channel_id->key()->Copy();
    channel_id_store_->SetChannelID(std::move(channel_id));
  }
  HandleResult(error, server_identifier, std::move(key));
}

void ChannelIDService::HandleResult(int error,
                                    const std::string& server_identifier,
                                    std::unique_ptr<crypto::ECPrivateKey> key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto j = inflight_.find(server_identifier);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }
  std::unique_ptr<ChannelIDServiceJob> job = std::move(j->second);
  inflight_.erase(j);

  job->HandleResult(error, std::move(key));
}

bool ChannelIDService::JoinToInFlightRequest(
    const std::string& domain,
    std::unique_ptr<crypto::ECPrivateKey>* key,
    bool create_if_missing,
    CompletionOnceCallback* callback,
    Request* out_req) {
  auto j = inflight_.find(domain);
  if (j == inflight_.end())
    return false;

  // A request for the same domain is in flight already. We'll attach our
  // callback, but we'll also mark it as requiring a channel ID if one's mising.
  ChannelIDServiceJob* job = j->second.get();
  inflight_joins_++;

  job->AddRequest(out_req, create_if_missing);
  out_req->RequestStarted(this, std::move(*callback), key, job);
  return true;
}

int ChannelIDService::LookupChannelID(
    const std::string& domain,
    std::unique_ptr<crypto::ECPrivateKey>* key,
    bool create_if_missing,
    CompletionOnceCallback* callback,
    Request* out_req) {
  // Check if a channel ID key already exists for this domain.
  int err = channel_id_store_->GetChannelID(
      domain, key,
      base::BindOnce(&ChannelIDService::GotChannelID,
                     weak_ptr_factory_.GetWeakPtr()));

  if (err == OK) {
    // Sync lookup found a valid channel ID.
    DVLOG(1) << "Channel ID store had valid key for " << domain;
    key_store_hits_++;
    return OK;
  }

  if (err == ERR_IO_PENDING) {
    // We are waiting for async DB lookup.  Create a job & request to track it.
    ChannelIDServiceJob* job = new ChannelIDServiceJob(create_if_missing);
    inflight_[domain] = base::WrapUnique(job);

    job->AddRequest(out_req);
    out_req->RequestStarted(this, std::move(*callback), key, job);
    return ERR_IO_PENDING;
  }

  return err;
}

int ChannelIDService::channel_id_count() {
  return channel_id_store_->GetChannelIDCount();
}

}  // namespace net
