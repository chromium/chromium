// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_service.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_header_parser.h"
#include "net/reporting/reporting_uploader.h"
#include "url/gurl.h"

namespace net {

namespace {

constexpr int kMaxJsonSize = 16 * 1024;
constexpr int kMaxJsonDepth = 5;

// If constructed with a PersistentReportingStore, the first call to any of
// QueueReport(), ProcessHeader(), RemoveBrowsingData(), or
// RemoveAllBrowsingData() on a valid input will trigger a load from the store.
// Tasks are queued pending completion of loading from the store.
class ReportingServiceImpl : public ReportingService {
 public:
  ReportingServiceImpl(std::unique_ptr<ReportingContext> context)
      : context_(std::move(context)),
        shut_down_(false),
        started_loading_from_store_(false),
        initialized_(false) {
    if (!context_->IsClientDataPersisted())
      initialized_ = true;
  }

  // ReportingService implementation:

  ~ReportingServiceImpl() override {
    if (initialized_)
      context_->cache()->Flush();
  }

  void QueueReport(const GURL& url,
                   const NetworkIsolationKey& network_isolation_key,
                   const std::string& user_agent,
                   const std::string& group,
                   const std::string& type,
                   std::unique_ptr<const base::Value> body,
                   int depth) override {
    DCHECK(context_);
    DCHECK(context_->delegate());

    if (!context_->delegate()->CanQueueReport(url::Origin::Create(url)))
      return;

    // Strip username, password, and ref fragment from the URL.
    GURL sanitized_url = url.GetAsReferrer();
    if (!sanitized_url.is_valid())
      return;

    base::TimeTicks queued_ticks = context_->tick_clock().NowTicks();

    // base::Unretained is safe because the callback is stored in
    // |task_backlog_| which will not outlive |this|.
    DoOrBacklogTask(base::BindOnce(
        &ReportingServiceImpl::DoQueueReport, base::Unretained(this),
        FixupNetworkIsolationKey(network_isolation_key),
        std::move(sanitized_url), user_agent, group, type, std::move(body),
        depth, queued_ticks));
  }

  void ProcessHeader(const GURL& url,
                     const NetworkIsolationKey& network_isolation_key,
                     const std::string& header_string) override {
    if (header_string.size() > kMaxJsonSize)
      return;

    std::unique_ptr<base::Value> header_value =
        base::JSONReader::ReadDeprecated("[" + header_string + "]",
                                         base::JSON_PARSE_RFC, kMaxJsonDepth);
    if (!header_value)
      return;

    DVLOG(1) << "Received Reporting policy for " << url.GetOrigin();
    DoOrBacklogTask(base::BindOnce(
        &ReportingServiceImpl::DoProcessHeader, base::Unretained(this),
        FixupNetworkIsolationKey(network_isolation_key), url,
        std::move(header_value)));
  }

  void RemoveBrowsingData(uint64_t data_type_mask,
                          const base::RepeatingCallback<bool(const GURL&)>&
                              origin_filter) override {
    DoOrBacklogTask(base::BindOnce(&ReportingServiceImpl::DoRemoveBrowsingData,
                                   base::Unretained(this), data_type_mask,
                                   origin_filter));
  }

  void RemoveAllBrowsingData(uint64_t data_type_mask) override {
    DoOrBacklogTask(
        base::BindOnce(&ReportingServiceImpl::DoRemoveAllBrowsingData,
                       base::Unretained(this), data_type_mask));
  }

  void OnShutdown() override {
    shut_down_ = true;
    context_->OnShutdown();
  }

  const ReportingPolicy& GetPolicy() const override {
    return context_->policy();
  }

  base::Value StatusAsValue() const override {
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetKey("reportingEnabled", base::Value(true));
    dict.SetKey("clients", context_->cache()->GetClientsAsValue());
    dict.SetKey("reports", context_->cache()->GetReportsAsValue());
    return dict;
  }

  ReportingContext* GetContextForTesting() const override {
    return context_.get();
  }

 private:
  void DoOrBacklogTask(base::OnceClosure task) {
    if (shut_down_)
      return;

    FetchAllClientsFromStoreIfNecessary();

    if (!initialized_) {
      task_backlog_.push_back(std::move(task));
      return;
    }

    std::move(task).Run();
  }

  void DoQueueReport(const NetworkIsolationKey& network_isolation_key,
                     GURL sanitized_url,
                     const std::string& user_agent,
                     const std::string& group,
                     const std::string& type,
                     std::unique_ptr<const base::Value> body,
                     int depth,
                     base::TimeTicks queued_ticks) {
    DCHECK(initialized_);
    context_->cache()->AddReport(network_isolation_key, sanitized_url,
                                 user_agent, group, type, std::move(body),
                                 depth, queued_ticks, 0 /* attempts */);
  }

  void DoProcessHeader(const NetworkIsolationKey& network_isolation_key,
                       const GURL& url,
                       std::unique_ptr<base::Value> header_value) {
    DCHECK(initialized_);
    ReportingHeaderParser::ParseHeader(context_.get(), network_isolation_key,
                                       url, std::move(header_value));
  }

  void DoRemoveBrowsingData(
      uint64_t data_type_mask,
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
    DCHECK(initialized_);
    ReportingBrowsingDataRemover::RemoveBrowsingData(
        context_->cache(), data_type_mask, origin_filter);
  }

  void DoRemoveAllBrowsingData(uint64_t data_type_mask) {
    DCHECK(initialized_);
    ReportingBrowsingDataRemover::RemoveAllBrowsingData(context_->cache(),
                                                        data_type_mask);
  }

  void ExecuteBacklog() {
    DCHECK(initialized_);
    DCHECK(context_);

    if (shut_down_)
      return;

    for (base::OnceClosure& task : task_backlog_) {
      std::move(task).Run();
    }
    task_backlog_.clear();
  }

  void FetchAllClientsFromStoreIfNecessary() {
    if (!context_->IsClientDataPersisted() || started_loading_from_store_)
      return;

    started_loading_from_store_ = true;
    FetchAllClientsFromStore();
  }

  void FetchAllClientsFromStore() {
    DCHECK(context_->IsClientDataPersisted());
    DCHECK(!initialized_);

    context_->store()->LoadReportingClients(base::BindOnce(
        &ReportingServiceImpl::OnClientsLoaded, weak_factory_.GetWeakPtr()));
  }

  void OnClientsLoaded(
      std::vector<ReportingEndpoint> loaded_endpoints,
      std::vector<CachedReportingEndpointGroup> loaded_endpoint_groups) {
    initialized_ = true;
    context_->cache()->AddClientsLoadedFromStore(
        std::move(loaded_endpoints), std::move(loaded_endpoint_groups));
    ExecuteBacklog();
  }

  // Returns either |network_isolation_key| or an empty NetworkIsolationKey,
  // based on |respect_network_isolation_key_|. Should be used on all
  // NetworkIsolationKeys passed in through public API calls.
  const NetworkIsolationKey& FixupNetworkIsolationKey(
      const NetworkIsolationKey& network_isolation_key) {
    if (respect_network_isolation_key_)
      return network_isolation_key;
    return empty_nik_;
  }

  std::unique_ptr<ReportingContext> context_;
  bool shut_down_;
  bool started_loading_from_store_;
  bool initialized_;
  std::vector<base::OnceClosure> task_backlog_;

  bool respect_network_isolation_key_ = base::FeatureList::IsEnabled(
      features::kPartitionNelAndReportingByNetworkIsolationKey);

  // Allows returning a NetworkIsolationKey by reference when
  // |respect_network_isolation_key_| is false.
  NetworkIsolationKey empty_nik_;

  base::WeakPtrFactory<ReportingServiceImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ReportingServiceImpl);
};

}  // namespace

ReportingService::~ReportingService() = default;

// static
std::unique_ptr<ReportingService> ReportingService::Create(
    const ReportingPolicy& policy,
    URLRequestContext* request_context,
    ReportingCache::PersistentReportingStore* store) {
  return std::make_unique<ReportingServiceImpl>(
      ReportingContext::Create(policy, request_context, store));
}

// static
std::unique_ptr<ReportingService> ReportingService::CreateForTesting(
    std::unique_ptr<ReportingContext> reporting_context) {
  return std::make_unique<ReportingServiceImpl>(std::move(reporting_context));
}

base::Value ReportingService::StatusAsValue() const {
  NOTIMPLEMENTED();
  return base::Value();
}

}  // namespace net
