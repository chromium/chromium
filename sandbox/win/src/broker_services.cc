// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/broker_services.h"

#include <stddef.h>

#include <optional>
#include <utility>
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/threading/platform_thread.h"
#include "base/win/access_token.h"
#include "base/win/current_module.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sandbox_policy_diagnostic.h"
#include "sandbox/win/src/startup_information_helper.h"
#include "sandbox/win/src/target_process.h"
#include "sandbox/win/src/threadpool.h"
#include "sandbox/win/src/win_utils.h"

namespace {

// Utility function to associate a completion port to a job object.
bool AssociateCompletionPort(HANDLE job, HANDLE port, void* key) {
  JOBOBJECT_ASSOCIATE_COMPLETION_PORT job_acp = {key, port};
  return ::SetInformationJobObject(job,
                                   JobObjectAssociateCompletionPortInformation,
                                   &job_acp, sizeof(job_acp))
             ? true
             : false;
}

// Commands that can be sent to the completion port serviced by
// TargetEventsThread().
enum {
  THREAD_CTRL_NONE,
  THREAD_CTRL_NEW_JOB_TRACKER,
  THREAD_CTRL_GET_POLICY_INFO,
  THREAD_CTRL_QUIT,
  THREAD_CTRL_LAST,
};

// Transfers parameters to the target events thread during Init().
struct TargetEventsThreadParams {
  TargetEventsThreadParams(
      HANDLE iocp,
      std::unique_ptr<sandbox::BrokerServicesTargetTracker> target_tracker,
      std::unique_ptr<sandbox::ThreadPool> thread_pool)
      : iocp(iocp),
        target_tracker_(std::move(target_tracker)),
        thread_pool(std::move(thread_pool)) {}
  ~TargetEventsThreadParams() {}
  // IOCP that job notifications and commands are sent to.
  // Handle is closed when BrokerServices is destroyed.
  HANDLE iocp;
  // Used in tests to keep track of how many processes are in jobs. Should be
  // nullptr in production.
  std::unique_ptr<sandbox::BrokerServicesTargetTracker> target_tracker_;
  // Thread pool used to mediate sandbox IPC, owned by the target
  // events thread but accessed by BrokerServices and TargetProcesses.
  // Destroyed when TargetEventsThread ends.
  std::unique_ptr<sandbox::ThreadPool> thread_pool;
};

// Helper structure that allows the Broker to associate a job notification
// with a job object and with a policy.
struct JobTracker {
  JobTracker(std::unique_ptr<sandbox::PolicyBase> policy, DWORD process_id)
      : policy(std::move(policy)), process_id(process_id) {}
  ~JobTracker() {
    // As if TerminateProcess() was called for all associated processes.
    // Handles are still valid.
    ::TerminateJobObject(policy->GetJobHandle(), sandbox::SBOX_ALL_OK);
  }

  std::unique_ptr<sandbox::PolicyBase> policy;
  DWORD process_id;
};

// Helper class to send policy lists
class PolicyDiagnosticList final : public sandbox::PolicyList {
 public:
  PolicyDiagnosticList() {}
  ~PolicyDiagnosticList() override {}
  void push_back(std::unique_ptr<sandbox::PolicyInfo> info) {
    internal_list_.push_back(std::move(info));
  }
  std::vector<std::unique_ptr<sandbox::PolicyInfo>>::iterator begin() override {
    return internal_list_.begin();
  }
  std::vector<std::unique_ptr<sandbox::PolicyInfo>>::iterator end() override {
    return internal_list_.end();
  }
  size_t size() const override { return internal_list_.size(); }

 private:
  std::vector<std::unique_ptr<sandbox::PolicyInfo>> internal_list_;
};

// The worker thread stays in a loop waiting for asynchronous notifications
// from the job objects. Right now we only care about knowing when the last
// process on a job terminates, but in general this is the place to tell
// the policy about events.
DWORD WINAPI TargetEventsThread(PVOID param) {
  if (!param)
    return 1;

  base::PlatformThread::SetName("BrokerEvent");

  // Take ownership of params so that it is deleted on thread exit.
  std::unique_ptr<TargetEventsThreadParams> params(
      reinterpret_cast<TargetEventsThreadParams*>(param));

  std::list<std::unique_ptr<JobTracker>> jobs;

  while (true) {
    DWORD event = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED ovl = nullptr;

    if (!::GetQueuedCompletionStatus(params->iocp, &event, &key, &ovl,
                                     INFINITE)) {
      // This call fails if the port has been closed before we have a
      // chance to service the last packet which is 'exit' anyway so
      // this is not an error.
      return 1;
    }

    if (key > THREAD_CTRL_LAST) {
      // The notification comes from a job object. There are nine notifications
      // that jobs can send and some of them depend on the job attributes set.
      JobTracker* tracker = reinterpret_cast<JobTracker*>(key);

      // Processes may be added to a job after the process count has reached
      // zero, leading us to manipulate a freed JobTracker object or job handle
      // (as the key is no longer valid). We therefore check if the tracker has
      // already been deleted. Note that Windows may emit notifications after
      // 'job finished' (active process zero), so not every case is unexpected.
      if (!base::Contains(jobs, tracker, &std::unique_ptr<JobTracker>::get)) {
        // CHECK if job already deleted.
        CHECK_NE(static_cast<int>(event), JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO);
        // Continue to next notification otherwise.
        continue;
      }

      switch (event) {
        case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO: {
          // The job object has signaled that the last process associated
          // with it has terminated. It is safe to free the tracker
          // and release its reference to the associated policy object
          // which will Close the job handle.
          jobs.erase(std::remove_if(
                         jobs.begin(), jobs.end(),
                         [&](auto&& p) -> bool { return p.get() == tracker; }),
                     jobs.end());
          break;
        }

        case JOB_OBJECT_MSG_NEW_PROCESS: {
          // Child process created from sandboxed process.
          if (params->target_tracker_) {
            params->target_tracker_->OnTargetAdded();
          }
          break;
        }

        case JOB_OBJECT_MSG_EXIT_PROCESS:
        case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS: {
          if (params->target_tracker_) {
            params->target_tracker_->OnTargetRemoved();
          }
          break;
        }

        case JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT: {
          // A child process attempted and failed to create a child process.
          // Counters must increment here as Windows will also send us a
          // JOB_OBJECT_MSG_EXIT_PROCESS notification for the failed-to-start
          // process.
          // Windows does not reveal the process id.
          if (params->target_tracker_) {
            params->target_tracker_->OnTargetAdded();
          }
          break;
        }

        case JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT: {
          bool res = ::TerminateJobObject(tracker->policy->GetJobHandle(),
                                          sandbox::SBOX_FATAL_MEMORY_EXCEEDED);
          DCHECK(res);
          // We also get the ACTIVE_PROCESS_ZERO event which reaps the job.
          if (params->target_tracker_) {
            params->target_tracker_->OnTargetRemoved();
          }
          break;
        }

        default: {
          NOTREACHED();
        }
      }
    } else if (THREAD_CTRL_NEW_JOB_TRACKER == key) {
      std::unique_ptr<JobTracker> tracker;
      tracker.reset(reinterpret_cast<JobTracker*>(ovl));
      DCHECK(tracker->policy->HasJob());

      jobs.push_back(std::move(tracker));
    } else if (THREAD_CTRL_GET_POLICY_INFO == key) {
      // Clone the policies for sandbox diagnostics.
      std::unique_ptr<sandbox::PolicyDiagnosticsReceiver> receiver;
      receiver.reset(static_cast<sandbox::PolicyDiagnosticsReceiver*>(
          reinterpret_cast<void*>(ovl)));
      // The PollicyInfo ctor copies essential information from the trackers.
      auto policy_list = std::make_unique<PolicyDiagnosticList>();
      for (auto&& job_tracker : jobs) {
        if (job_tracker->policy) {
          policy_list->push_back(std::make_unique<sandbox::PolicyDiagnostic>(
              job_tracker->policy.get()));
        }
      }
      // Receiver should return quickly.
      receiver->ReceiveDiagnostics(std::move(policy_list));

    } else if (THREAD_CTRL_QUIT == key) {
      // After this point, so further calls to ProcessEventCallback can
      // occur. Other tracked objects are destroyed as this thread ends.
      return 0;
    } else {
      // We have not implemented more commands.
      NOTREACHED();
    }
  }

  NOTREACHED();
}

}  // namespace

namespace sandbox {

BrokerServicesBase::BrokerServicesBase() {}

// The broker uses a dedicated worker thread that services the job completion
// port to perform policy notifications and associated cleanup tasks.
ResultCode BrokerServicesBase::InitInternal(
    std::unique_ptr<BrokerServicesDelegate> delegate,
    std::unique_ptr<BrokerServicesTargetTracker> target_tracker) {
  broker_services_delegate_ = std::move(delegate);

  if (job_port_.is_valid() || thread_pool_) {
    return SBOX_ERROR_UNEXPECTED_CALL;
  }

  job_port_.Set(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0));
  if (!job_port_.is_valid()) {
    return SBOX_ERROR_CANNOT_INIT_BROKERSERVICES;
  }

  // We transfer ownership of this memory to the thread.
  auto params = std::make_unique<TargetEventsThreadParams>(
      job_port_.get(), std::move(target_tracker),
      std::make_unique<ThreadPool>());

  // We keep the thread alive until our destructor so we can use a raw
  // pointer to the thread pool.
  thread_pool_ = params->thread_pool.get();

#if defined(ARCH_CPU_32_BITS)
  // Conserve address space in 32-bit Chrome. This thread uses a small and
  // consistent amount and doesn't need the default of 1.5 MiB.
  constexpr unsigned flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
  constexpr size_t stack_size = 128 * 1024;
#else
  constexpr unsigned int flags = 0;
  constexpr size_t stack_size = 0;
#endif
  job_thread_.Set(::CreateThread(nullptr, stack_size,  // Default security.
                                 TargetEventsThread, params.get(), flags,
                                 nullptr));
  if (!job_thread_.is_valid()) {
    thread_pool_ = nullptr;
    // Returning cleans up params.
    return SBOX_ERROR_CANNOT_INIT_BROKERSERVICES;
  }

  params.release();
  return SBOX_ALL_OK;
}

ResultCode BrokerServicesBase::Init(
    std::unique_ptr<BrokerServicesDelegate> delegate) {
  return BrokerServicesBase::InitInternal(std::move(delegate), nullptr);
}

// Only called in test code.
ResultCode BrokerServicesBase::InitForTesting(
    std::unique_ptr<BrokerServicesDelegate> delegate,
    std::unique_ptr<BrokerServicesTargetTracker> target_tracker) {
  return BrokerServicesBase::InitInternal(std::move(delegate),
                                          std::move(target_tracker));
}

// The destructor should only be called when the Broker process is terminating.
// Since BrokerServicesBase is a singleton, this is called from the CRT
// termination handlers, if this code lives on a DLL it is called during
// DLL_PROCESS_DETACH in other words, holding the loader lock, so we cannot
// wait for threads here.
BrokerServicesBase::~BrokerServicesBase() {
  // If there is no port Init() was never called successfully.
  if (!job_port_.is_valid()) {
    return;
  }

  // Closing the port causes, that no more Job notifications are delivered to
  // the worker thread and also causes the thread to exit. This is what we
  // want to do since we are going to close all outstanding Jobs and notifying
  // the policy objects ourselves.
  ::PostQueuedCompletionStatus(job_port_.get(), 0, THREAD_CTRL_QUIT, nullptr);

  if (job_thread_.is_valid() &&
      WAIT_TIMEOUT == ::WaitForSingleObject(job_thread_.get(), 5000)) {
    // Cannot clean broker services, continuing past here will lead to crashes
    // if any sandbox IPCs are outstanding, and crashing isn't valuable, so
    // terminate the process.
    ::TerminateProcess(GetCurrentProcess(), SBOX_FATAL_BROKER_SHUTDOWN_HUNG);
    // Should never happen but tells the compiler this block cannot return.
    NOTREACHED();
  }
}

std::unique_ptr<TargetPolicy> BrokerServicesBase::CreatePolicy() {
  return CreatePolicy("");
}

std::unique_ptr<TargetPolicy> BrokerServicesBase::CreatePolicy(
    std::string_view tag) {
  // If you change the type of the object being created here you must also
  // change the downcast to it in SpawnTarget().
  auto policy = std::make_unique<PolicyBase>(tag);
  // Empty key implies we will not use the store. The policy will need
  // to look after its config.
  if (!tag.empty()) {
    // Otherwise the broker owns the memory, not the policy.
    auto found = config_cache_.find(tag);
    ConfigBase* shared_config = nullptr;
    if (found == config_cache_.end()) {
      auto new_config = std::make_unique<ConfigBase>();
      shared_config = new_config.get();
      config_cache_[std::string(tag)] = std::move(new_config);
      policy->SetConfig(shared_config);
    } else {
      policy->SetConfig(found->second.get());
    }
  }
  return policy;
}

ResultCode BrokerServicesBase::SpawnTarget(const wchar_t* exe_path,
                                           const wchar_t* command_line,
                                           std::unique_ptr<TargetPolicy> policy,
                                           DWORD* last_error,
                                           PROCESS_INFORMATION* target_info) {
  *last_error = 0;
  *target_info = {};
  ResultCode result = SBOX_ERROR_GENERIC;
  bool callback_called = false;

  // With parallel launching disabled, it is safe to capture local references
  // because SpawnTargetAsyncImpl is guaranteed to run the callback before
  // returning.
  // The `policy` downcast is safe as long as we control CreatePolicy().
  SpawnTargetAsyncImpl(
      exe_path, command_line,
      base::WrapUnique(static_cast<PolicyBase*>(policy.release())),
      base::BindOnce(
          [](DWORD* last_error, PROCESS_INFORMATION* target_info,
             ResultCode* result, bool* callback_called,
             base::win::ScopedProcessInformation result_target_info,
             DWORD result_last_error, ResultCode result_code) {
            *target_info = result_target_info.Take();
            *last_error = result_last_error;
            *result = result_code;
            *callback_called = true;
          },
          last_error, target_info, &result, &callback_called),
      /*allow_parallel_launch=*/false);

  CHECK(callback_called);
  return result;
}

void BrokerServicesBase::SpawnTargetAsync(const wchar_t* exe_path,
                                          const wchar_t* command_line,
                                          std::unique_ptr<TargetPolicy> policy,
                                          SpawnTargetCallback result_callback) {
  // The `policy` downcast is safe as long as we control CreatePolicy().
  SpawnTargetAsyncImpl(
      exe_path, command_line,
      base::WrapUnique(static_cast<PolicyBase*>(policy.release())),
      std::move(result_callback),
      /*allow_parallel_launch=*/true);
}

ResultCode BrokerServicesBase::PreSpawnTarget(
    const wchar_t* exe_path,
    PolicyBase* policy_base,
    StartupInformationHelper* startup_info,
    std::unique_ptr<TargetProcess>& target) {
  if (!exe_path)
    return SBOX_ERROR_BAD_PARAMS;

  // This code should only be called from the exe, ensure that this is always
  // the case.
  HMODULE exe_module = nullptr;
  CHECK(::GetModuleHandleEx(
      /*dwFlags=*/GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, nullptr,
      &exe_module));
  if (CURRENT_MODULE() != exe_module)
    return SBOX_ERROR_INVALID_LINK_STATE;

  if (!policy_base) {
    return SBOX_ERROR_BAD_PARAMS;
  }

  ConfigBase* config_base = static_cast<ConfigBase*>(policy_base->GetConfig());
  if (!config_base->IsConfigured()) {
    if (!config_base->Freeze())
      return SBOX_ERROR_FAILED_TO_FREEZE_CONFIG;
  }

  // Even though the resources touched by SpawnTarget can be accessed in
  // multiple threads, the method itself cannot be called from more than one
  // thread. This is to protect the global variables used while setting up the
  // child process, and to make sure launcher thread mitigations are applied
  // correctly.
  static DWORD thread_id = ::GetCurrentThreadId();
  DCHECK(thread_id == ::GetCurrentThreadId());

  // Launcher thread only needs to be opted out of ACG once. Do this on the
  // first child process being spawned.
  static bool launcher_thread_opted_out = false;

  if (!launcher_thread_opted_out) {
    // Soft fail this call. It will fail if ACG is not enabled for this process.
    sandbox::ApplyMitigationsToCurrentThread(
        sandbox::MITIGATION_DYNAMIC_CODE_OPT_OUT_THIS_THREAD);
    launcher_thread_opted_out = true;
  }

  // Construct the tokens and the job object that we are going to associate
  // with the soon to be created target process.
  std::optional<base::win::AccessToken> initial_token;
  std::optional<base::win::AccessToken> lockdown_token;
  ResultCode result = SBOX_ALL_OK;

  result = policy_base->MakeTokens(initial_token, lockdown_token);
  if (SBOX_ALL_OK != result)
    return result;

  result = UpdateDesktopIntegrity(config_base->desktop(),
                                  config_base->integrity_level());
  if (result != SBOX_ALL_OK)
    return result;

  result = policy_base->InitJob();
  if (SBOX_ALL_OK != result)
    return result;

  // Initialize the startup information from the policy.
  // We don't want any child processes causing the IDC_APPSTARTING cursor.
  startup_info->UpdateFlags(STARTF_FORCEOFFFEEDBACK);
  startup_info->SetDesktop(GetDesktopName(config_base->desktop()));
  startup_info->SetMitigations(config_base->GetProcessMitigations());
  startup_info->SetFilterEnvironment(config_base->GetEnvironmentFiltered());

  if (base::win::GetVersion() >= base::win::Version::WIN10_TH2 &&
      config_base->GetJobLevel() <= JobLevel::kLimitedUser) {
    startup_info->SetRestrictChildProcessCreation(true);
  }

  // Shares std handles if they are valid.
  startup_info->SetStdHandles(policy_base->GetStdoutHandle(),
                              policy_base->GetStderrHandle());
  // Add any additional handles that were requested.
  const auto& policy_handle_list = policy_base->GetHandlesBeingShared();
  for (HANDLE handle : policy_handle_list)
    startup_info->AddInheritedHandle(handle);

  AppContainer* container = config_base->GetAppContainer();
  if (container) {
    CHECK(config_base->is_csrss_connected() ||
          config_base->GetLockdownTokenLevel() == USER_LOCKDOWN)
        << "CSRSS must be connected to use a privileged AppContainer sandbox.";
    startup_info->SetAppContainer(container);
  }

  startup_info->AddJobToAssociate(policy_base->GetJobHandle());

  if (!startup_info->BuildStartupInformation())
    return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;

  // Create the TargetProcess object. Note that Brokerservices does not own the
  // target object. It is owned by the Policy.
  target = std::make_unique<TargetProcess>(
      std::move(*initial_token), std::move(*lockdown_token), thread_pool_);

  return SBOX_ALL_OK;
}

// SpawnTarget does all the interesting sandbox setup and creates the target
// process inside the sandbox.
void BrokerServicesBase::SpawnTargetAsyncImpl(
    const wchar_t* exe_path,
    const wchar_t* command_line,
    std::unique_ptr<PolicyBase> policy_base,
    SpawnTargetCallback result_callback,
    bool allow_parallel_launch) {
  auto startup_info = std::make_unique<StartupInformationHelper>();
  std::unique_ptr<TargetProcess> target;

  ResultCode result =
      PreSpawnTarget(exe_path, policy_base.get(), startup_info.get(), target);
  if (result != SBOX_ALL_OK) {
    std::move(result_callback)
        .Run(base::win::ScopedProcessInformation(), ::GetLastError(), result);
    return;
  }

  if (allow_parallel_launch &&
      broker_services_delegate_->ParallelLaunchEnabled()) {
    TargetProcess* target_ptr = target.get();
    broker_services_delegate_->ParallelLaunchPostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&BrokerServicesBase::CreateTarget,
                       base::Unretained(this), target_ptr,
                       std::wstring(exe_path), std::wstring(command_line),
                       std::move(startup_info)),
        base::BindOnce(&BrokerServicesBase::FinishSpawnTarget,
                       base::Unretained(this), std::move(policy_base),
                       std::move(target), std::move(result_callback)));
    return;
  }

  CreateTargetResult target_result = CreateTarget(
      target.get(), exe_path, command_line, std::move(startup_info));

  FinishSpawnTarget(std::move(policy_base), std::move(target),
                    std::move(result_callback), std::move(target_result));
}

CreateTargetResult BrokerServicesBase::CreateTarget(
    TargetProcess* target,
    const std::wstring& exe_path,
    const std::wstring& command_line,
    std::unique_ptr<StartupInformationHelper> startup_info) {
  // A trace ID for the current scope is generated from the address of a local
  // variable to ensure uniqueness across threads.
  const void* trace_id = &startup_info;
  broker_services_delegate_->BeforeTargetProcessCreateOnCreationThread(
      trace_id);

  // Spawn the target process suspended.
  CreateTargetResult result;
  result.result_code = target->Create(exe_path.c_str(), command_line.c_str(),
                                      std::move(startup_info),
                                      &result.process_info, &result.last_error);

  broker_services_delegate_->AfterTargetProcessCreateOnCreationThread(
      trace_id, result.process_info.process_id());

  return result;
}

void BrokerServicesBase::FinishSpawnTarget(
    std::unique_ptr<PolicyBase> policy_base,
    std::unique_ptr<TargetProcess> target,
    SpawnTargetCallback result_callback,
    CreateTargetResult target_result) {
  ResultCode result = FinishSpawnTargetImpl(
      target_result.result_code, std::move(policy_base), std::move(target),
      &target_result.process_info, &target_result.last_error);
  if (result != SBOX_ALL_OK) {
    target_result.process_info.Close();
  }
  std::move(result_callback)
      .Run(std::move(target_result.process_info), target_result.last_error,
           result);
}

ResultCode BrokerServicesBase::FinishSpawnTargetImpl(
    ResultCode initial_result,
    std::unique_ptr<PolicyBase> policy_base,
    std::unique_ptr<TargetProcess> target,
    base::win::ScopedProcessInformation* process_info,
    DWORD* last_error) {
  if (initial_result != SBOX_ALL_OK) {
    target->Terminate();
    return initial_result;
  }

  ConfigBase* config_base = static_cast<ConfigBase*>(policy_base->GetConfig());

  if (config_base->GetJobLevel() <= JobLevel::kLimitedUser) {
    // Restrict the job from containing any processes. Job restrictions
    // are only applied at process creation, so the target process is
    // unaffected.
    ResultCode result = policy_base->DropActiveProcessLimit();
    if (result != SBOX_ALL_OK) {
      target->Terminate();
      return result;
    }
  }

  // Now the policy is the owner of the target. TargetProcess will terminate
  // the process if it has not completed when it is destroyed.
  ResultCode result = policy_base->ApplyToTarget(std::move(target));

  if (result != SBOX_ALL_OK) {
    *last_error = ::GetLastError();
    return result;
  }

  HANDLE job_handle = policy_base->GetJobHandle();
  JobTracker* tracker =
      new JobTracker(std::move(policy_base), process_info->process_id());

  // Post the tracker to the tracking thread, then associate the job with
  // the tracker. The worker thread takes ownership of these objects.
  CHECK(::PostQueuedCompletionStatus(job_port_.get(), 0,
                                     THREAD_CTRL_NEW_JOB_TRACKER,
                                     reinterpret_cast<LPOVERLAPPED>(tracker)));
  // There is no obvious cleanup here.
  CHECK(AssociateCompletionPort(job_handle, job_port_.get(), tracker));

  return result;
}

ResultCode BrokerServicesBase::GetPolicyDiagnostics(
    std::unique_ptr<PolicyDiagnosticsReceiver> receiver) {
  CHECK(job_thread_.is_valid());
  // Post to the job thread.
  if (!::PostQueuedCompletionStatus(
          job_port_.get(), 0, THREAD_CTRL_GET_POLICY_INFO,
          reinterpret_cast<LPOVERLAPPED>(receiver.get()))) {
    receiver->OnError(SBOX_ERROR_GENERIC);
    return SBOX_ERROR_GENERIC;
  }

  // Ownership has passed to tracker thread.
  receiver.release();
  return SBOX_ALL_OK;
}

void BrokerServicesBase::SetStartingMitigations(
    sandbox::MitigationFlags starting_mitigations) {
  sandbox::SetStartingMitigations(starting_mitigations);
}

bool BrokerServicesBase::RatchetDownSecurityMitigations(
    MitigationFlags additional_flags) {
  return sandbox::RatchetDownSecurityMitigations(additional_flags);
}

std::wstring BrokerServicesBase::GetDesktopName(Desktop desktop) {
  switch (desktop) {
    case Desktop::kDefault:
      // No alternate desktop or winstation. Return an empty string.
      return std::wstring();
    case Desktop::kAlternateWinstation:
      return alt_winstation_->GetDesktopName();
    case Desktop::kAlternateDesktop:
      return alt_desktop_->GetDesktopName();
  }
}

ResultCode BrokerServicesBase::UpdateDesktopIntegrity(
    Desktop desktop,
    IntegrityLevel integrity) {
  // If we're launching on an alternate desktop we need to make sure the
  // integrity label on the object is no higher than the sandboxed process's
  // integrity level. So, we lower the label on the desktop handle if it's
  // not already low enough for our process.
  if (integrity == INTEGRITY_LEVEL_LAST)
    return SBOX_ALL_OK;
  switch (desktop) {
    case Desktop::kDefault:
      return SBOX_ALL_OK;
    case Desktop::kAlternateWinstation:
      return alt_winstation_->UpdateDesktopIntegrity(integrity);
    case Desktop::kAlternateDesktop:
      return alt_desktop_->UpdateDesktopIntegrity(integrity);
  }
}

ResultCode BrokerServicesBase::CreateAlternateDesktop(Desktop desktop) {
  switch (desktop) {
    case Desktop::kAlternateWinstation: {
      // If already populated keep going.
      if (alt_winstation_)
        return SBOX_ALL_OK;
      alt_winstation_ = std::make_unique<AlternateDesktop>();
      ResultCode result = alt_winstation_->Initialize(true);
      if (result != SBOX_ALL_OK)
        alt_winstation_.reset();
      return result;
    };
    case Desktop::kAlternateDesktop: {
      // If already populated keep going.
      if (alt_desktop_)
        return SBOX_ALL_OK;
      alt_desktop_ = std::make_unique<AlternateDesktop>();
      ResultCode result = alt_desktop_->Initialize(false);
      if (result != SBOX_ALL_OK)
        alt_desktop_.reset();
      return result;
    };
    case Desktop::kDefault:
      // The default desktop always exists.
      return SBOX_ALL_OK;
  }
}

void BrokerServicesBase::DestroyDesktops() {
  alt_winstation_.reset();
  alt_desktop_.reset();
}

void BrokerServicesBase::SetBrokerServicesDelegateForTesting(
    std::unique_ptr<BrokerServicesDelegate> delegate) {
  broker_services_delegate_ = std::move(delegate);
}

// static
void BrokerServicesBase::FreezeTargetConfigForTesting(TargetConfig* config) {
  CHECK(!config->IsConfigured());
  static_cast<ConfigBase*>(config)->Freeze();
}

}  // namespace sandbox
