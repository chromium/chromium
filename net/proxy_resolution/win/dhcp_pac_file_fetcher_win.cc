// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/dhcp_pac_file_fetcher_win.h"

#include <winsock2.h>

#include <iphlpapi.h>

#include <memory>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/free_deleter.h"
#include "base/synchronization/lock.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/proxy_resolution/win/dhcp_pac_file_adapter_fetcher_win.h"

namespace net {

namespace {

// Returns true if |adapter| should be considered when probing for WPAD via
// DHCP.
bool IsDhcpCapableAdapter(IP_ADAPTER_ADDRESSES* adapter) {
  if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
    return false;
  if ((adapter->Flags & IP_ADAPTER_DHCP_ENABLED) == 0)
    return false;

  // Don't probe interfaces which are not up and ready to pass packets.
  //
  // This is a speculative fix for https://crbug.com/770201, in case calling
  // dhcpsvc!DhcpRequestParams on interfaces that aren't ready yet blocks for
  // a long time.
  //
  // Since ConfiguredProxyResolutionService restarts WPAD probes in response to
  // other network level changes, this will likely get called again once the
  // interface is up.
  if (adapter->OperStatus != IfOperStatusUp)
    return false;

  return true;
}

}  // namespace

// This struct contains logging information describing how
// GetCandidateAdapterNames() performed, for output to NetLog.
struct DhcpAdapterNamesLoggingInfo {
  DhcpAdapterNamesLoggingInfo() = default;

  DhcpAdapterNamesLoggingInfo(const DhcpAdapterNamesLoggingInfo&) = delete;
  DhcpAdapterNamesLoggingInfo& operator=(const DhcpAdapterNamesLoggingInfo&) =
      delete;

  ~DhcpAdapterNamesLoggingInfo() = default;

  // The error that iphlpapi!GetAdaptersAddresses returned.
  ULONG error;

  // The adapters list that iphlpapi!GetAdaptersAddresses returned.
  std::unique_ptr<IP_ADAPTER_ADDRESSES, base::FreeDeleter> adapters;

  // The time immediately before GetCandidateAdapterNames was posted to a worker
  // thread from the origin thread.
  base::TimeTicks origin_thread_start_time;

  // The time when GetCandidateAdapterNames began running on the worker thread.
  base::TimeTicks worker_thread_start_time;

  // The time when GetCandidateAdapterNames completed running on the worker
  // thread.
  base::TimeTicks worker_thread_end_time;

  // The time when control returned to the origin thread
  // (OnGetCandidateAdapterNamesDone)
  base::TimeTicks origin_thread_end_time;
};

namespace {

// Maximum number of DHCP lookup tasks running concurrently. This is chosen
// based on the following UMA data:
// - When OnWaitTimer fires, ~99.8% of users have 6 or fewer network
//   adapters enabled for DHCP in total.
// - At the same measurement point, ~99.7% of users have 3 or fewer pending
//   DHCP adapter lookups.
// - There is however a very long and thin tail of users who have
//   systems reporting up to 100+ adapters (this must be some very weird
//   OS bug (?), probably the cause of http://crbug.com/240034).
//
// Th value is chosen such that DHCP lookup tasks don't prevent other tasks from
// running even on systems that report a huge number of network adapters, while
// giving a good chance of getting back results for any responsive adapters.
constexpr int kMaxConcurrentDhcpLookupTasks = 12;

// How long to wait at maximum after we get results (a PAC file or
// knowledge that no PAC file is configured) from whichever network
// adapter finishes first.
constexpr base::TimeDelta kMaxWaitAfterFirstResult = base::Milliseconds(400);

// A TaskRunner that never schedules more than |kMaxConcurrentDhcpLookupTasks|
// tasks concurrently.
class TaskRunnerWithCap : public base::TaskRunner {
 public:
  TaskRunnerWithCap() = default;

  TaskRunnerWithCap(const TaskRunnerWithCap&) = delete;
  TaskRunnerWithCap& operator=(const TaskRunnerWithCap&) = delete;

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    // Delayed tasks are not supported.
    DCHECK(delay.is_zero());

    // Wrap the task in a callback that runs |task|, then tries to schedule a
    // task from |pending_tasks_|.
    base::OnceClosure wrapped_task =
        base::BindOnce(&TaskRunnerWithCap::RunTaskAndSchedulePendingTask, this,
                       std::move(task));

    {
      base::AutoLock auto_lock(lock_);

      // If |kMaxConcurrentDhcpLookupTasks| tasks are scheduled, move the task
      // to |pending_tasks_|.
      DCHECK_LE(num_scheduled_tasks_, kMaxConcurrentDhcpLookupTasks);
      if (num_scheduled_tasks_ == kMaxConcurrentDhcpLookupTasks) {
        pending_tasks_.emplace(from_here, std::move(wrapped_task));
        return true;
      }

      // If less than |kMaxConcurrentDhcpLookupTasks| tasks are scheduled,
      // increment |num_scheduled_tasks_| and schedule the task.
      ++num_scheduled_tasks_;
    }

    task_runner_->PostTask(from_here, std::move(wrapped_task));
    return true;
  }

 private:
  struct LocationAndTask {
    LocationAndTask() = default;
    LocationAndTask(const base::Location& from_here, base::OnceClosure task)
        : from_here(from_here), task(std::move(task)) {}
    base::Location from_here;
    base::OnceClosure task;
  };

  ~TaskRunnerWithCap() override = default;

  void RunTaskAndSchedulePendingTask(base::OnceClosure task) {
    // Run |task|.
    std::move(task).Run();

    // If |pending_tasks_| is non-empty, schedule a task from it. Otherwise,
    // decrement |num_scheduled_tasks_|.
    LocationAndTask task_to_schedule;

    {
      base::AutoLock auto_lock(lock_);

      DCHECK_GT(num_scheduled_tasks_, 0);
      if (pending_tasks_.empty()) {
        --num_scheduled_tasks_;
        return;
      }

      task_to_schedule = std::move(pending_tasks_.front());
      pending_tasks_.pop();
    }

    DCHECK(task_to_schedule.task);
    task_runner_->PostTask(task_to_schedule.from_here,
                           std::move(task_to_schedule.task));
  }

  const scoped_refptr<base::TaskRunner> task_runner_ =
      base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
           base::TaskPriority::USER_VISIBLE});

  // Synchronizes access to members below.
  base::Lock lock_;

  // Number of tasks that are currently scheduled.
  int num_scheduled_tasks_ = 0;

  // Tasks that are waiting to be scheduled.
  base::queue<LocationAndTask> pending_tasks_;
};

base::Value::Dict NetLogGetAdaptersDoneParams(
    DhcpAdapterNamesLoggingInfo* info) {
  base::Value::Dict result;

  // Add information on each of the adapters enumerated (including those that
  // were subsequently skipped).
  base::Value::List adapters_list;
  for (IP_ADAPTER_ADDRESSES* adapter = info->adapters.get(); adapter;
       adapter = adapter->Next) {
    base::Value::Dict adapter_value;

    adapter_value.Set("AdapterName", adapter->AdapterName);
    adapter_value.Set("IfType", static_cast<int>(adapter->IfType));
    adapter_value.Set("Flags", static_cast<int>(adapter->Flags));
    adapter_value.Set("OperStatus", static_cast<int>(adapter->OperStatus));
    adapter_value.Set("TunnelType", static_cast<int>(adapter->TunnelType));

    // "skipped" means the adapter was not ultimately chosen as a candidate for
    // testing WPAD.
    bool skipped = !IsDhcpCapableAdapter(adapter);
    adapter_value.Set("skipped", base::Value(skipped));

    adapters_list.Append(std::move(adapter_value));
  }
  result.Set("adapters", std::move(adapters_list));

  result.Set("origin_to_worker_thread_hop_dt",
             static_cast<int>((info->worker_thread_start_time -
                               info->origin_thread_start_time)
                                  .InMilliseconds()));
  result.Set("worker_to_origin_thread_hop_dt",
             static_cast<int>(
                 (info->origin_thread_end_time - info->worker_thread_end_time)
                     .InMilliseconds()));
  result.Set("worker_dt", static_cast<int>((info->worker_thread_end_time -
                                            info->worker_thread_start_time)
                                               .InMilliseconds()));

  if (info->error != ERROR_SUCCESS)
    result.Set("error", static_cast<int>(info->error));

  return result;
}

base::Value::Dict NetLogFetcherDoneParams(int fetcher_index, int net_error) {
  base::Value::Dict result;

  result.Set("fetcher_index", fetcher_index);
  result.Set("net_error", net_error);

  return result;
}

}  // namespace

DhcpPacFileFetcherWin::DhcpPacFileFetcherWin(
    URLRequestContext* url_request_context)
    : url_request_context_(url_request_context),
      task_runner_(base::MakeRefCounted<TaskRunnerWithCap>()) {
  DCHECK(url_request_context_);
}

DhcpPacFileFetcherWin::~DhcpPacFileFetcherWin() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Count as user-initiated if we are not yet in STATE_DONE.
  Cancel();
}

int DhcpPacFileFetcherWin::Fetch(
    std::u16string* utf16_text,
    CompletionOnceCallback callback,
    const NetLogWithSource& net_log,
    const NetworkTrafficAnnotationTag traffic_annotation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (state_ != STATE_START && state_ != STATE_DONE) {
    NOTREACHED_IN_MIGRATION();
    return ERR_UNEXPECTED;
  }

  net_log_ = net_log;

  if (!url_request_context_)
    return ERR_CONTEXT_SHUT_DOWN;

  state_ = STATE_WAIT_ADAPTERS;
  callback_ = std::move(callback);
  destination_string_ = utf16_text;

  net_log.BeginEvent(NetLogEventType::WPAD_DHCP_WIN_FETCH);

  // TODO(eroman): This event is not ended in the case of cancellation.
  net_log.BeginEvent(NetLogEventType::WPAD_DHCP_WIN_GET_ADAPTERS);

  last_query_ = ImplCreateAdapterQuery();
  last_query_->logging_info()->origin_thread_start_time =
      base::TimeTicks::Now();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &DhcpPacFileFetcherWin::AdapterQuery::GetCandidateAdapterNames,
          last_query_.get()),
      base::BindOnce(&DhcpPacFileFetcherWin::OnGetCandidateAdapterNamesDone,
                     weak_ptr_factory_.GetWeakPtr(), last_query_,
                     traffic_annotation));

  return ERR_IO_PENDING;
}

void DhcpPacFileFetcherWin::Cancel() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  CancelImpl();
}

void DhcpPacFileFetcherWin::OnShutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Cancel current request, if there is one.
  CancelImpl();

  // Prevent future network requests.
  url_request_context_ = nullptr;
}

void DhcpPacFileFetcherWin::CancelImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state_ != STATE_DONE) {
    callback_.Reset();
    wait_timer_.Stop();
    state_ = STATE_DONE;

    for (FetcherVector::iterator it = fetchers_.begin();
         it != fetchers_.end();
         ++it) {
      (*it)->Cancel();
    }

    fetchers_.clear();
  }
  destination_string_ = nullptr;
}

void DhcpPacFileFetcherWin::OnGetCandidateAdapterNamesDone(
    scoped_refptr<AdapterQuery> query,
    const NetworkTrafficAnnotationTag traffic_annotation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This can happen if this object is reused for multiple queries,
  // and a previous query was cancelled before it completed.
  if (query.get() != last_query_.get())
    return;
  last_query_ = nullptr;

  DhcpAdapterNamesLoggingInfo* logging_info = query->logging_info();
  logging_info->origin_thread_end_time = base::TimeTicks::Now();

  net_log_.EndEvent(NetLogEventType::WPAD_DHCP_WIN_GET_ADAPTERS,
                    [&] { return NetLogGetAdaptersDoneParams(logging_info); });

  // Enable unit tests to wait for this to happen; in production this function
  // call is a no-op.
  ImplOnGetCandidateAdapterNamesDone();

  // We may have been cancelled.
  if (state_ != STATE_WAIT_ADAPTERS)
    return;

  state_ = STATE_NO_RESULTS;

  const std::set<std::string>& adapter_names = query->adapter_names();

  if (adapter_names.empty()) {
    TransitionToDone();
    return;
  }

  for (const std::string& adapter_name : adapter_names) {
    std::unique_ptr<DhcpPacFileAdapterFetcher> fetcher(
        ImplCreateAdapterFetcher());
    size_t fetcher_index = fetchers_.size();
    fetcher->Fetch(adapter_name,
                   base::BindOnce(&DhcpPacFileFetcherWin::OnFetcherDone,
                                  base::Unretained(this), fetcher_index),
                   traffic_annotation);
    fetchers_.push_back(std::move(fetcher));
  }
  num_pending_fetchers_ = fetchers_.size();
}

std::string DhcpPacFileFetcherWin::GetFetcherName() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return "win";
}

const GURL& DhcpPacFileFetcherWin::GetPacURL() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, STATE_DONE);

  return pac_url_;
}

void DhcpPacFileFetcherWin::OnFetcherDone(size_t fetcher_index,
                                          int result) {
  DCHECK(state_ == STATE_NO_RESULTS || state_ == STATE_SOME_RESULTS);

  net_log_.AddEvent(NetLogEventType::WPAD_DHCP_WIN_ON_FETCHER_DONE, [&] {
    return NetLogFetcherDoneParams(fetcher_index, result);
  });

  if (--num_pending_fetchers_ == 0) {
    TransitionToDone();
    return;
  }

  // If the only pending adapters are those less preferred than one
  // with a valid PAC script, we do not need to wait any longer.
  for (FetcherVector::iterator it = fetchers_.begin();
       it != fetchers_.end();
       ++it) {
    bool did_finish = (*it)->DidFinish();
    int fetch_result = (*it)->GetResult();
    if (did_finish && fetch_result == OK) {
      TransitionToDone();
      return;
    }
    if (!did_finish || fetch_result != ERR_PAC_NOT_IN_DHCP) {
      break;
    }
  }

  // Once we have a single result, we set a maximum on how long to wait
  // for the rest of the results.
  if (state_ == STATE_NO_RESULTS) {
    state_ = STATE_SOME_RESULTS;
    net_log_.AddEvent(NetLogEventType::WPAD_DHCP_WIN_START_WAIT_TIMER);
    wait_timer_.Start(FROM_HERE,
        ImplGetMaxWait(), this, &DhcpPacFileFetcherWin::OnWaitTimer);
  }
}

void DhcpPacFileFetcherWin::OnWaitTimer() {
  DCHECK_EQ(state_, STATE_SOME_RESULTS);

  net_log_.AddEvent(NetLogEventType::WPAD_DHCP_WIN_ON_WAIT_TIMER);
  TransitionToDone();
}

void DhcpPacFileFetcherWin::TransitionToDone() {
  DCHECK(state_ == STATE_NO_RESULTS || state_ == STATE_SOME_RESULTS);

  int used_fetcher_index = -1;
  int result = ERR_PAC_NOT_IN_DHCP;  // Default if no fetchers.
  if (!fetchers_.empty()) {
    // Scan twice for the result; once through the whole list for success,
    // then if no success, return result for most preferred network adapter,
    // preferring "real" network errors to the ERR_PAC_NOT_IN_DHCP error.
    // Default to ERR_ABORTED if no fetcher completed.
    result = ERR_ABORTED;
    for (size_t i = 0; i < fetchers_.size(); ++i) {
      const auto& fetcher = fetchers_[i];
      if (fetcher->DidFinish() && fetcher->GetResult() == OK) {
        result = OK;
        *destination_string_ = fetcher->GetPacScript();
        pac_url_ = fetcher->GetPacURL();
        used_fetcher_index = i;
        break;
      }
    }
    if (result != OK) {
      destination_string_->clear();
      for (size_t i = 0; i < fetchers_.size(); ++i) {
        const auto& fetcher = fetchers_[i];
        if (fetcher->DidFinish()) {
          result = fetcher->GetResult();
          used_fetcher_index = i;
          if (result != ERR_PAC_NOT_IN_DHCP) {
            break;
          }
        }
      }
    }
  }

  CompletionOnceCallback callback = std::move(callback_);
  CancelImpl();
  DCHECK_EQ(state_, STATE_DONE);
  DCHECK(fetchers_.empty());

  net_log_.EndEvent(NetLogEventType::WPAD_DHCP_WIN_FETCH, [&] {
    return NetLogFetcherDoneParams(used_fetcher_index, result);
  });

  // We may be deleted re-entrantly within this outcall.
  std::move(callback).Run(result);
}

int DhcpPacFileFetcherWin::num_pending_fetchers() const {
  return num_pending_fetchers_;
}

URLRequestContext* DhcpPacFileFetcherWin::url_request_context() const {
  return url_request_context_;
}

scoped_refptr<base::TaskRunner> DhcpPacFileFetcherWin::GetTaskRunner() {
  return task_runner_;
}

std::unique_ptr<DhcpPacFileAdapterFetcher>
DhcpPacFileFetcherWin::ImplCreateAdapterFetcher() {
  return std::make_unique<DhcpPacFileAdapterFetcher>(url_request_context_,
                                                     task_runner_);
}

scoped_refptr<DhcpPacFileFetcherWin::AdapterQuery>
DhcpPacFileFetcherWin::ImplCreateAdapterQuery() {
  return base::MakeRefCounted<AdapterQuery>();
}

base::TimeDelta DhcpPacFileFetcherWin::ImplGetMaxWait() {
  return kMaxWaitAfterFirstResult;
}

bool DhcpPacFileFetcherWin::GetCandidateAdapterNames(
    std::set<std::string>* adapter_names,
    DhcpAdapterNamesLoggingInfo* info) {
  DCHECK(adapter_names);
  adapter_names->clear();

  // The GetAdaptersAddresses MSDN page recommends using a size of 15000 to
  // avoid reallocation.
  ULONG adapters_size = 15000;
  std::unique_ptr<IP_ADAPTER_ADDRESSES, base::FreeDeleter> adapters;
  ULONG error = ERROR_SUCCESS;
  int num_tries = 0;

  do {
    adapters.reset(static_cast<IP_ADAPTER_ADDRESSES*>(malloc(adapters_size)));
    // Return only unicast addresses, and skip information we do not need.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    error = GetAdaptersAddresses(
        AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
            GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME,
        nullptr, adapters.get(), &adapters_size);
    ++num_tries;
  } while (error == ERROR_BUFFER_OVERFLOW && num_tries <= 3);

  if (info)
    info->error = error;

  if (error == ERROR_NO_DATA) {
    // There are no adapters that we care about.
    return true;
  }

  if (error != ERROR_SUCCESS) {
    LOG(WARNING) << "Unexpected error retrieving WPAD configuration from DHCP.";
    return false;
  }

  IP_ADAPTER_ADDRESSES* adapter = nullptr;
  for (adapter = adapters.get(); adapter; adapter = adapter->Next) {
    if (IsDhcpCapableAdapter(adapter)) {
      DCHECK(adapter->AdapterName);
      adapter_names->insert(adapter->AdapterName);
    }
  }

  // Transfer the buffer containing the adapters, so it can be used later for
  // emitting NetLog parameters from the origin thread.
  if (info)
    info->adapters = std::move(adapters);
  return true;
}

DhcpPacFileFetcherWin::AdapterQuery::AdapterQuery()
    : logging_info_(std::make_unique<DhcpAdapterNamesLoggingInfo>()) {}

void DhcpPacFileFetcherWin::AdapterQuery::GetCandidateAdapterNames() {
  logging_info_->error = ERROR_NO_DATA;
  logging_info_->adapters.reset();
  logging_info_->worker_thread_start_time = base::TimeTicks::Now();

  ImplGetCandidateAdapterNames(&adapter_names_, logging_info_.get());

  logging_info_->worker_thread_end_time = base::TimeTicks::Now();
}

const std::set<std::string>&
DhcpPacFileFetcherWin::AdapterQuery::adapter_names() const {
  return adapter_names_;
}

bool DhcpPacFileFetcherWin::AdapterQuery::ImplGetCandidateAdapterNames(
    std::set<std::string>* adapter_names,
    DhcpAdapterNamesLoggingInfo* info) {
  return DhcpPacFileFetcherWin::GetCandidateAdapterNames(adapter_names,
                                                         info);
}

DhcpPacFileFetcherWin::AdapterQuery::~AdapterQuery() = default;

}  // namespace net
