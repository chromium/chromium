// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/histogram_synchronizer.h"

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_delta_serialization.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/histogram_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/common/content_constants.h"

namespace content {

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace {

// Negative numbers are never used as sequence numbers.  We explicitly pick a
// negative number that is "so negative" that even when we add one (as is done
// when we generated the next sequence number) that it will still be negative.
// We have code that handles wrapping around on an overflow into negative
// territory.
static const int kNeverUsableSequenceNumber = -2;

}  // anonymous namespace

// The "RequestContext" structure describes an individual request received from
// the UI. All methods are accessible on UI thread.
class HistogramSynchronizer::RequestContext {
 public:
  // A map from sequence_number_ to the actual RequestContexts.
  typedef std::map<int, RequestContext*> RequestContextMap;

  RequestContext(base::OnceClosure callback, int sequence_number)
      : callback_(std::move(callback)),
        sequence_number_(sequence_number),
        received_process_group_count_(0),
        processes_pending_(0) {}
  ~RequestContext() {}

  void SetReceivedProcessGroupCount(bool done) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    received_process_group_count_ = done;
  }

  // Methods for book keeping of processes_pending_.
  void AddProcessesPending(int processes_pending) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    processes_pending_ += processes_pending;
  }

  void DecrementProcessesPending() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    --processes_pending_;
  }

  // Records that we are waiting for one less histogram data from a process for
  // the given sequence number. If |received_process_group_count_| and
  // |processes_pending_| are zero, then delete the current object by calling
  // Unregister.
  void DeleteIfAllDone() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (processes_pending_ <= 0 && received_process_group_count_)
      RequestContext::Unregister(sequence_number_);
  }

  // Register |callback| in |outstanding_requests_| map for the given
  // |sequence_number|.
  static void Register(base::OnceClosure callback, int sequence_number) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    RequestContext* request =
        new RequestContext(std::move(callback), sequence_number);
    outstanding_requests_.Get()[sequence_number] = request;
  }

  // Find the |RequestContext| in |outstanding_requests_| map for the given
  // |sequence_number|.
  static RequestContext* GetRequestContext(int sequence_number) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    auto it = outstanding_requests_.Get().find(sequence_number);
    if (it == outstanding_requests_.Get().end())
      return nullptr;

    RequestContext* request = it->second;
    DCHECK_EQ(sequence_number, request->sequence_number_);
    return request;
  }

  // Delete the entry for the given |sequence_number| from
  // |outstanding_requests_| map. This method is called when all changes have
  // been acquired, or when the wait time expires (whichever is sooner).
  static void Unregister(int sequence_number) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    auto it = outstanding_requests_.Get().find(sequence_number);
    if (it == outstanding_requests_.Get().end())
      return;

    RequestContext* request = it->second;
    DCHECK_EQ(sequence_number, request->sequence_number_);
    bool received_process_group_count = request->received_process_group_count_;
    int unresponsive_processes = request->processes_pending_;

    std::move(request->callback_).Run();

    delete request;
    outstanding_requests_.Get().erase(it);

    UMA_HISTOGRAM_BOOLEAN("Histogram.ReceivedProcessGroupCount",
                          received_process_group_count);
    UMA_HISTOGRAM_COUNTS_1M("Histogram.PendingProcessNotResponding",
                            unresponsive_processes);
  }

  // Delete all the entries in |outstanding_requests_| map.
  static void OnShutdown() {
    // Just in case we have any pending tasks, clear them out.
    while (!outstanding_requests_.Get().empty()) {
      auto it = outstanding_requests_.Get().begin();
      delete it->second;
      outstanding_requests_.Get().erase(it);
    }
  }

  // Requests are made to asynchronously send data to the |callback_|.
  base::OnceClosure callback_;

  // The sequence number used by the most recent update request to contact all
  // processes.
  int sequence_number_;

  // Indicates if we have received all pending processes count.
  bool received_process_group_count_;

  // The number of pending processes (all renderer processes and browser child
  // processes) that have not yet responded to requests.
  int processes_pending_;

  // Map of all outstanding RequestContexts, from sequence_number_ to
  // RequestContext.
  static base::LazyInstance<RequestContextMap>::Leaky outstanding_requests_;
};

// static
base::LazyInstance
    <HistogramSynchronizer::RequestContext::RequestContextMap>::Leaky
        HistogramSynchronizer::RequestContext::outstanding_requests_ =
            LAZY_INSTANCE_INITIALIZER;

HistogramSynchronizer::HistogramSynchronizer()
    : lock_(),
      last_used_sequence_number_(kNeverUsableSequenceNumber),
      async_sequence_number_(kNeverUsableSequenceNumber) {
  HistogramController::GetInstance()->Register(this);
}

HistogramSynchronizer::~HistogramSynchronizer() {
  RequestContext::OnShutdown();

  // Just in case we have any pending tasks, clear them out.
  SetTaskRunnerAndCallback(nullptr, base::Closure());
}

HistogramSynchronizer* HistogramSynchronizer::GetInstance() {
  return base::Singleton<
      HistogramSynchronizer,
      base::LeakySingletonTraits<HistogramSynchronizer>>::get();
}

// static
void HistogramSynchronizer::FetchHistograms() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&HistogramSynchronizer::FetchHistograms));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  HistogramSynchronizer* current_synchronizer =
      HistogramSynchronizer::GetInstance();
  if (current_synchronizer == nullptr)
    return;

  current_synchronizer->RegisterAndNotifyAllProcesses(
      HistogramSynchronizer::UNKNOWN,
      base::TimeDelta::FromMinutes(1));
}

void FetchHistogramsAsynchronously(scoped_refptr<base::TaskRunner> task_runner,
                                   const base::Closure& callback,
                                   base::TimeDelta wait_time) {
  HistogramSynchronizer::FetchHistogramsAsynchronously(std::move(task_runner),
                                                       callback, wait_time);
}

// static
void HistogramSynchronizer::FetchHistogramsAsynchronously(
    scoped_refptr<base::TaskRunner> task_runner,
    const base::Closure& callback,
    base::TimeDelta wait_time) {
  DCHECK(task_runner);
  DCHECK(!callback.is_null());

  HistogramSynchronizer* current_synchronizer =
      HistogramSynchronizer::GetInstance();
  current_synchronizer->SetTaskRunnerAndCallback(std::move(task_runner),
                                                 callback);

  current_synchronizer->RegisterAndNotifyAllProcesses(
      HistogramSynchronizer::ASYNC_HISTOGRAMS, wait_time);
}

void HistogramSynchronizer::RegisterAndNotifyAllProcesses(
    ProcessHistogramRequester requester,
    base::TimeDelta wait_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int sequence_number = GetNextAvailableSequenceNumber(requester);

  base::OnceClosure callback = base::BindOnce(
      &HistogramSynchronizer::ForceHistogramSynchronizationDoneCallback,
      base::Unretained(this), sequence_number);

  RequestContext::Register(std::move(callback), sequence_number);

  // Get histogram data from renderer and browser child processes.
  HistogramController::GetInstance()->GetHistogramData(sequence_number);

  // Post a task that would be called after waiting for wait_time.  This acts
  // as a watchdog, to cancel the requests for non-responsive processes.
  base::PostDelayedTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&RequestContext::Unregister, sequence_number), wait_time);
}

void HistogramSynchronizer::OnPendingProcesses(int sequence_number,
                                               int pending_processes,
                                               bool end) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RequestContext* request = RequestContext::GetRequestContext(sequence_number);
  if (!request)
    return;
  request->AddProcessesPending(pending_processes);
  request->SetReceivedProcessGroupCount(end);
  request->DeleteIfAllDone();
}

void HistogramSynchronizer::OnHistogramDataCollected(
    int sequence_number,
    const std::vector<std::string>& pickled_histograms) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::HistogramDeltaSerialization::DeserializeAndAddSamples(
      pickled_histograms);

  RequestContext* request = RequestContext::GetRequestContext(sequence_number);
  if (!request)
    return;

  // Delete request if we have heard back from all child processes.
  request->DecrementProcessesPending();
  request->DeleteIfAllDone();
}

void HistogramSynchronizer::SetTaskRunnerAndCallback(
    scoped_refptr<base::TaskRunner> task_runner,
    const base::Closure& callback) {
  base::Closure old_callback;
  scoped_refptr<base::TaskRunner> old_task_runner;
  {
    base::AutoLock auto_lock(lock_);
    old_callback = callback_;
    callback_ = callback;
    old_task_runner = std::move(callback_task_runner_);
    callback_task_runner_ = std::move(task_runner);
    // Prevent premature calling of our new callbacks.
    async_sequence_number_ = kNeverUsableSequenceNumber;
  }
  // Just in case there was a task pending....
  InternalPostTask(std::move(old_task_runner), std::move(old_callback));
}

void HistogramSynchronizer::ForceHistogramSynchronizationDoneCallback(
    int sequence_number) {
  base::Closure callback;
  scoped_refptr<base::TaskRunner> task_runner;
  {
    base::AutoLock lock(lock_);
    if (sequence_number != async_sequence_number_)
      return;
    callback = callback_;
    task_runner = std::move(callback_task_runner_);
    callback_.Reset();
  }
  InternalPostTask(std::move(task_runner), std::move(callback));
}

void HistogramSynchronizer::InternalPostTask(
    scoped_refptr<base::TaskRunner> task_runner,
    const base::Closure& callback) {
  if (callback.is_null() || !task_runner)
    return;
  task_runner->PostTask(FROM_HERE, callback);
}

int HistogramSynchronizer::GetNextAvailableSequenceNumber(
    ProcessHistogramRequester requester) {
  base::AutoLock auto_lock(lock_);
  ++last_used_sequence_number_;
  // Watch out for wrapping to a negative number.
  if (last_used_sequence_number_ < 0) {
    // Bypass the reserved number, which is used when a renderer spontaneously
    // decides to send some histogram data.
    last_used_sequence_number_ =
        kHistogramSynchronizerReservedSequenceNumber + 1;
  }
  DCHECK_NE(last_used_sequence_number_,
            kHistogramSynchronizerReservedSequenceNumber);
  if (requester == ASYNC_HISTOGRAMS)
    async_sequence_number_ = last_used_sequence_number_;
  return last_used_sequence_number_;
}

}  // namespace content
