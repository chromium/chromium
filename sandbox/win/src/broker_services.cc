// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/broker_services.h"

#include <aclapi.h>

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/startup_information.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/app_container_profile.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sandbox_policy_diagnostic.h"
#include "sandbox/win/src/target_process.h"
#include "sandbox/win/src/win2k_threadpool.h"
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

// Utility function to do the cleanup necessary when something goes wrong
// while in SpawnTarget and we must terminate the target process.
sandbox::ResultCode SpawnCleanup(sandbox::TargetProcess* target) {
  target->Terminate();
  delete target;
  return sandbox::SBOX_ERROR_GENERIC;
}

// the different commands that you can send to the worker thread that
// executes TargetEventsThread().
enum {
  THREAD_CTRL_NONE,
  THREAD_CTRL_NEW_JOB_TRACKER,
  THREAD_CTRL_NEW_PROCESS_TRACKER,
  THREAD_CTRL_PROCESS_SIGNALLED,
  THREAD_CTRL_GET_POLICY_INFO,
  THREAD_CTRL_QUIT,
  THREAD_CTRL_LAST,
};

// Helper structure that allows the Broker to associate a job notification
// with a job object and with a policy.
struct JobTracker {
  JobTracker(base::win::ScopedHandle job,
             scoped_refptr<sandbox::PolicyBase> policy,
             DWORD process_id)
      : job(std::move(job)), policy(policy), process_id(process_id) {}
  ~JobTracker() { FreeResources(); }

  // Releases the Job and notifies the associated Policy object to release its
  // resources as well.
  void FreeResources();

  base::win::ScopedHandle job;
  scoped_refptr<sandbox::PolicyBase> policy;
  DWORD process_id;
};

void JobTracker::FreeResources() {
  if (policy) {
    bool res = ::TerminateJobObject(job.Get(), sandbox::SBOX_ALL_OK);
    DCHECK(res);
    // Closing the job causes the target process to be destroyed so this needs
    // to happen before calling OnJobEmpty().
    HANDLE stale_job_handle = job.Get();
    job.Close();

    // In OnJobEmpty() we don't actually use the job handle directly.
    policy->OnJobEmpty(stale_job_handle);
    policy = nullptr;
  }
}

// tracks processes that are not in jobs
struct ProcessTracker {
  ProcessTracker(scoped_refptr<sandbox::PolicyBase> policy,
                 DWORD process_id,
                 base::win::ScopedHandle process)
      : policy(policy), process_id(process_id), process(std::move(process)) {}
  ~ProcessTracker() { FreeResources(); }

  void FreeResources();

  scoped_refptr<sandbox::PolicyBase> policy;
  DWORD process_id;
  base::win::ScopedHandle process;
  // Used to UnregisterWait. Not a real handle so cannot CloseHandle().
  HANDLE wait_handle;
  // IOCP that is tracking this non-job process
  HANDLE iocp;
};

void ProcessTracker::FreeResources() {
  if (policy) {
    policy->OnJobEmpty(nullptr);
    policy = nullptr;
  }
}

// Helper redispatches process events to tracker thread.
void WINAPI ProcessEventCallback(PVOID param, BOOLEAN ignored) {
  // This callback should do very little, and must be threadpool safe.
  ProcessTracker* tracker = reinterpret_cast<ProcessTracker*>(param);
  // If this fails we can do nothing... we will leak the policy.
  ::PostQueuedCompletionStatus(tracker->iocp, 0, THREAD_CTRL_PROCESS_SIGNALLED,
                               reinterpret_cast<LPOVERLAPPED>(tracker));
}

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

}  // namespace

namespace sandbox {

BrokerServicesBase::BrokerServicesBase() {}

// The broker uses a dedicated worker thread that services the job completion
// port to perform policy notifications and associated cleanup tasks.
ResultCode BrokerServicesBase::Init() {
  if (job_port_.IsValid() || thread_pool_)
    return SBOX_ERROR_UNEXPECTED_CALL;

  job_port_.Set(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0));
  if (!job_port_.IsValid())
    return SBOX_ERROR_CANNOT_INIT_BROKERSERVICES;

  no_targets_.Set(::CreateEventW(nullptr, true, false, nullptr));

  job_thread_.Set(::CreateThread(nullptr, 0,  // Default security and stack.
                                 TargetEventsThread, this, 0, nullptr));
  if (!job_thread_.IsValid())
    return SBOX_ERROR_CANNOT_INIT_BROKERSERVICES;

  return SBOX_ALL_OK;
}

// The destructor should only be called when the Broker process is terminating.
// Since BrokerServicesBase is a singleton, this is called from the CRT
// termination handlers, if this code lives on a DLL it is called during
// DLL_PROCESS_DETACH in other words, holding the loader lock, so we cannot
// wait for threads here.
BrokerServicesBase::~BrokerServicesBase() {
  // If there is no port Init() was never called successfully.
  if (!job_port_.IsValid())
    return;

  // Closing the port causes, that no more Job notifications are delivered to
  // the worker thread and also causes the thread to exit. This is what we
  // want to do since we are going to close all outstanding Jobs and notifying
  // the policy objects ourselves.
  ::PostQueuedCompletionStatus(job_port_.Get(), 0, THREAD_CTRL_QUIT, nullptr);

  if (job_thread_.IsValid() &&
      WAIT_TIMEOUT == ::WaitForSingleObject(job_thread_.Get(), 1000)) {
    // Cannot clean broker services.
    NOTREACHED();
    return;
  }
  thread_pool_.reset();
}

scoped_refptr<TargetPolicy> BrokerServicesBase::CreatePolicy() {
  // If you change the type of the object being created here you must also
  // change the downcast to it in SpawnTarget().
  scoped_refptr<TargetPolicy> policy(new PolicyBase);
  // PolicyBase starts with refcount 1.
  policy->Release();
  return policy;
}

// The worker thread stays in a loop waiting for asynchronous notifications
// from the job objects. Right now we only care about knowing when the last
// process on a job terminates, but in general this is the place to tell
// the policy about events.
DWORD WINAPI BrokerServicesBase::TargetEventsThread(PVOID param) {
  if (!param)
    return 1;

  base::PlatformThread::SetName("BrokerEvent");

  BrokerServicesBase* broker = reinterpret_cast<BrokerServicesBase*>(param);
  HANDLE port = broker->job_port_.Get();
  HANDLE no_targets = broker->no_targets_.Get();

  std::set<DWORD> child_process_ids;
  std::list<std::unique_ptr<JobTracker>> jobs;
  std::list<std::unique_ptr<ProcessTracker>> processes;
  int target_counter = 0;
  int untracked_target_counter = 0;
  ::ResetEvent(no_targets);

  while (true) {
    DWORD events = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED ovl = nullptr;

    if (!::GetQueuedCompletionStatus(port, &events, &key, &ovl, INFINITE)) {
      // this call fails if the port has been closed before we have a
      // chance to service the last packet which is 'exit' anyway so
      // this is not an error.
      return 1;
    }

    if (key > THREAD_CTRL_LAST) {
      // The notification comes from a job object. There are nine notifications
      // that jobs can send and some of them depend on the job attributes set.
      JobTracker* tracker = reinterpret_cast<JobTracker*>(key);

      switch (events) {
        case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO: {
          // The job object has signaled that the last process associated
          // with it has terminated. It is safe to free the tracker
          // and release its reference to the associated policy object
          // which will Close the job handle.
          HANDLE job_handle = tracker->job.Get();

          // Erase by comparing with the job handle.
          jobs.erase(std::remove_if(
              jobs.begin(), jobs.end(),
              [&](auto&& p) -> bool { return p->job.Get() == job_handle; }));
          break;
        }

        case JOB_OBJECT_MSG_NEW_PROCESS: {
          // Child process created from sandboxed process.
          DWORD process_id =
              static_cast<DWORD>(reinterpret_cast<uintptr_t>(ovl));
          size_t count = child_process_ids.count(process_id);
          if (count == 0)
            untracked_target_counter++;
          ++target_counter;
          if (1 == target_counter) {
            ::ResetEvent(no_targets);
          }
          break;
        }

        case JOB_OBJECT_MSG_EXIT_PROCESS:
        case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS: {
          size_t erase_result = child_process_ids.erase(
              static_cast<DWORD>(reinterpret_cast<uintptr_t>(ovl)));
          if (erase_result != 1U) {
            // The process was untracked e.g. a child process of the target.
            --untracked_target_counter;
            DCHECK(untracked_target_counter >= 0);
          }
          --target_counter;
          if (0 == target_counter)
            ::SetEvent(no_targets);

          DCHECK(target_counter >= 0);
          break;
        }

        case JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT: {
          // A child process attempted and failed to create a child process.
          // Windows does not reveal the process id.
          untracked_target_counter++;
          target_counter++;
          break;
        }

        case JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT: {
          bool res = ::TerminateJobObject(tracker->job.Get(),
                                          SBOX_FATAL_MEMORY_EXCEEDED);
          DCHECK(res);
          break;
        }

        default: {
          NOTREACHED();
          break;
        }
      }
    } else if (THREAD_CTRL_NEW_JOB_TRACKER == key) {
      std::unique_ptr<JobTracker> tracker;
      tracker.reset(reinterpret_cast<JobTracker*>(ovl));
      DCHECK(tracker->job.IsValid());

      child_process_ids.insert(tracker->process_id);
      jobs.push_back(std::move(tracker));

    } else if (THREAD_CTRL_NEW_PROCESS_TRACKER == key) {
      std::unique_ptr<ProcessTracker> tracker;
      tracker.reset(reinterpret_cast<ProcessTracker*>(ovl));

      if (child_process_ids.empty()) {
        ::SetEvent(broker->no_targets_.Get());
      }

      tracker->iocp = port;
      if (!::RegisterWaitForSingleObject(&(tracker->wait_handle),
                                         tracker->process.Get(),
                                         ProcessEventCallback, tracker.get(),
                                         INFINITE, WT_EXECUTEONLYONCE)) {
        // Failed. Invalidate the wait_handle and store anyway.
        tracker->wait_handle = INVALID_HANDLE_VALUE;
      }
      processes.push_back(std::move(tracker));

    } else if (THREAD_CTRL_PROCESS_SIGNALLED == key) {
      ProcessTracker* tracker =
          static_cast<ProcessTracker*>(reinterpret_cast<void*>(ovl));

      ::UnregisterWait(tracker->wait_handle);
      tracker->wait_handle = INVALID_HANDLE_VALUE;

      // PID is unique until the process handle is closed in dtor.
      processes.erase(std::remove_if(
          processes.begin(), processes.end(), [&](auto&& p) -> bool {
            return p->process_id == tracker->process_id;
          }));

    } else if (THREAD_CTRL_GET_POLICY_INFO == key) {
      // Clone the policies for sandbox diagnostics.
      std::unique_ptr<PolicyDiagnosticsReceiver> receiver;
      receiver.reset(static_cast<PolicyDiagnosticsReceiver*>(
          reinterpret_cast<void*>(ovl)));
      // The PollicyInfo ctor copies essential information from the trackers.
      auto policy_list = std::make_unique<PolicyDiagnosticList>();
      for (auto&& process_tracker : processes) {
        if (process_tracker->policy) {
          policy_list->push_back(std::make_unique<PolicyDiagnostic>(
              process_tracker->policy.get()));
        }
      }
      for (auto&& job_tracker : jobs) {
        if (job_tracker->policy) {
          policy_list->push_back(
              std::make_unique<PolicyDiagnostic>(job_tracker->policy.get()));
        }
      }
      // Receiver should return quickly.
      receiver->ReceiveDiagnostics(std::move(policy_list));

    } else if (THREAD_CTRL_QUIT == key) {
      // The broker object is being destroyed so the thread needs to exit.
      for (auto&& tracker : processes) {
        ::UnregisterWait(tracker->wait_handle);
        tracker->wait_handle = INVALID_HANDLE_VALUE;
      }
      // After this point, so further calls to ProcessEventCallback can
      // occur. Other tracked objects are destroyed as this thread ends.
      return 0;
    } else {
      // We have not implemented more commands.
      NOTREACHED();
    }
  }

  NOTREACHED();
  return 0;
}

// SpawnTarget does all the interesting sandbox setup and creates the target
// process inside the sandbox.
ResultCode BrokerServicesBase::SpawnTarget(const wchar_t* exe_path,
                                           const wchar_t* command_line,
                                           scoped_refptr<TargetPolicy> policy,
                                           ResultCode* last_warning,
                                           DWORD* last_error,
                                           PROCESS_INFORMATION* target_info) {
  if (!exe_path)
    return SBOX_ERROR_BAD_PARAMS;

  if (!policy)
    return SBOX_ERROR_BAD_PARAMS;

  // Even though the resources touched by SpawnTarget can be accessed in
  // multiple threads, the method itself cannot be called from more than one
  // thread. This is to protect the global variables used while setting up the
  // child process, and to make sure launcher thread mitigations are applied
  // correctly.
  static DWORD thread_id = ::GetCurrentThreadId();
  DCHECK(thread_id == ::GetCurrentThreadId());
  *last_warning = SBOX_ALL_OK;

  // Launcher thread only needs to be opted out of ACG once. Do this on the
  // first child process being spawned.
  static bool launcher_thread_opted_out = false;

  if (!launcher_thread_opted_out) {
    // Soft fail this call. It will fail if ACG is not enabled for this process.
    sandbox::ApplyMitigationsToCurrentThread(
        sandbox::MITIGATION_DYNAMIC_CODE_OPT_OUT_THIS_THREAD);
    launcher_thread_opted_out = true;
  }

  // This downcast is safe as long as we control CreatePolicy()
  scoped_refptr<PolicyBase> policy_base(static_cast<PolicyBase*>(policy.get()));

  // Construct the tokens and the job object that we are going to associate
  // with the soon to be created target process.
  base::win::ScopedHandle initial_token;
  base::win::ScopedHandle lockdown_token;
  base::win::ScopedHandle lowbox_token;
  ResultCode result = SBOX_ALL_OK;

  result =
      policy_base->MakeTokens(&initial_token, &lockdown_token, &lowbox_token);
  if (SBOX_ALL_OK != result)
    return result;
  if (lowbox_token.IsValid() &&
      base::win::GetVersion() < base::win::Version::WIN8) {
    // We don't allow lowbox_token below Windows 8.
    return SBOX_ERROR_BAD_PARAMS;
  }

  base::win::ScopedHandle job;
  result = policy_base->MakeJobObject(&job);
  if (SBOX_ALL_OK != result)
    return result;

  // Initialize the startup information from the policy.
  base::win::StartupInformation startup_info;

  // We don't want any child processes causing the IDC_APPSTARTING cursor.
  startup_info.startup_info()->dwFlags |= STARTF_FORCEOFFFEEDBACK;

  // The liftime of |mitigations|, |inherit_handle_list| and
  // |child_process_creation| have to be at least as long as
  // |startup_info| because |UpdateProcThreadAttribute| requires that
  // its |lpValue| parameter persist until |DeleteProcThreadAttributeList| is
  // called; StartupInformation's destructor makes such a call.
  DWORD64 mitigations[2];
  std::vector<HANDLE> inherited_handle_list;
  DWORD child_process_creation = PROCESS_CREATION_CHILD_PROCESS_RESTRICTED;

  std::wstring desktop = policy_base->GetAlternateDesktop();
  if (!desktop.empty()) {
    startup_info.startup_info()->lpDesktop =
        const_cast<wchar_t*>(desktop.c_str());
  }

  bool inherit_handles = false;

  int attribute_count = 0;

  size_t mitigations_size;
  ConvertProcessMitigationsToPolicy(policy_base->GetProcessMitigations(),
                                    &mitigations[0], &mitigations_size);
  if (mitigations[0] || mitigations[1])
    ++attribute_count;

  bool restrict_child_process_creation = false;
  if (base::win::GetVersion() >= base::win::Version::WIN10_TH2 &&
      policy_base->GetJobLevel() <= JOB_LIMITED_USER) {
    restrict_child_process_creation = true;
    ++attribute_count;
  }

  HANDLE stdout_handle = policy_base->GetStdoutHandle();
  HANDLE stderr_handle = policy_base->GetStderrHandle();

  if (stdout_handle != INVALID_HANDLE_VALUE)
    inherited_handle_list.push_back(stdout_handle);

  // Handles in the list must be unique.
  if (stderr_handle != stdout_handle && stderr_handle != INVALID_HANDLE_VALUE)
    inherited_handle_list.push_back(stderr_handle);

  const auto& policy_handle_list = policy_base->GetHandlesBeingShared();

  for (HANDLE handle : policy_handle_list)
    inherited_handle_list.push_back(handle);

  if (inherited_handle_list.size())
    ++attribute_count;

  scoped_refptr<AppContainerProfileBase> profile =
      policy_base->GetAppContainerProfileBase();
  if (profile) {
    if (base::win::GetVersion() < base::win::Version::WIN8)
      return SBOX_ERROR_BAD_PARAMS;
    ++attribute_count;
    if (profile->GetEnableLowPrivilegeAppContainer()) {
      // LPAC first supported in RS1.
      if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
        return SBOX_ERROR_BAD_PARAMS;
      ++attribute_count;
    }
  }

  if (!startup_info.InitializeProcThreadAttributeList(attribute_count))
    return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;

  if (mitigations[0] || mitigations[1]) {
    if (!startup_info.UpdateProcThreadAttribute(
            PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &mitigations[0],
            mitigations_size)) {
      return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;
    }
  }

  if (restrict_child_process_creation) {
    if (!startup_info.UpdateProcThreadAttribute(
            PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY, &child_process_creation,
            sizeof(child_process_creation))) {
      return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;
    }
  }

  if (inherited_handle_list.size()) {
    if (!startup_info.UpdateProcThreadAttribute(
            PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &inherited_handle_list[0],
            sizeof(HANDLE) * inherited_handle_list.size())) {
      return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;
    }
    startup_info.startup_info()->dwFlags |= STARTF_USESTDHANDLES;
    startup_info.startup_info()->hStdInput = INVALID_HANDLE_VALUE;
    startup_info.startup_info()->hStdOutput = stdout_handle;
    startup_info.startup_info()->hStdError = stderr_handle;
    // Allowing inheritance of handles is only secure now that we
    // have limited which handles will be inherited.
    inherit_handles = true;
  }

  // Declared here to ensure they stay in scope until after process creation.
  std::unique_ptr<SecurityCapabilities> security_capabilities;
  DWORD all_applications_package_policy =
      PROCESS_CREATION_ALL_APPLICATION_PACKAGES_OPT_OUT;

  if (profile) {
    security_capabilities = profile->GetSecurityCapabilities();
    if (!startup_info.UpdateProcThreadAttribute(
            PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
            security_capabilities.get(), sizeof(SECURITY_CAPABILITIES))) {
      return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;
    }
    if (profile->GetEnableLowPrivilegeAppContainer()) {
      if (!startup_info.UpdateProcThreadAttribute(
              PROC_THREAD_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY,
              &all_applications_package_policy,
              sizeof(all_applications_package_policy))) {
        return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;
      }
    }
  }

  // Construct the thread pool here in case it is expensive.
  // The thread pool is shared by all the targets
  if (!thread_pool_)
    thread_pool_ = std::make_unique<Win2kThreadPool>();

  // Create the TargetProcess object and spawn the target suspended. Note that
  // Brokerservices does not own the target object. It is owned by the Policy.
  base::win::ScopedProcessInformation process_info;
  TargetProcess* target = new TargetProcess(
      std::move(initial_token), std::move(lockdown_token), job.Get(),
      thread_pool_.get(),
      profile ? profile->GetImpersonationCapabilities() : std::vector<Sid>());

  result = target->Create(exe_path, command_line, inherit_handles, startup_info,
                          &process_info, last_error);

  if (result != SBOX_ALL_OK) {
    SpawnCleanup(target);
    return result;
  }

  if (lowbox_token.IsValid()) {
    *last_warning = target->AssignLowBoxToken(lowbox_token);
    // If this fails we continue, but report the error as a warning.
    // This is due to certain configurations causing the setting of the
    // token to fail post creation, and we'd rather continue if possible.
    if (*last_warning != SBOX_ALL_OK)
      *last_error = ::GetLastError();
  }

  // Now the policy is the owner of the target.
  result = policy_base->AddTarget(target);

  if (result != SBOX_ALL_OK) {
    *last_error = ::GetLastError();
    SpawnCleanup(target);
    return result;
  }

  if (job.IsValid()) {
    JobTracker* tracker =
        new JobTracker(std::move(job), policy_base, process_info.process_id());

    // Post the tracker to the tracking thread, then associate the job with
    // the tracker. The worker thread takes ownership of these objects.
    CHECK(::PostQueuedCompletionStatus(
        job_port_.Get(), 0, THREAD_CTRL_NEW_JOB_TRACKER,
        reinterpret_cast<LPOVERLAPPED>(tracker)));
    // There is no obvious recovery after failure here. Previous version with
    // SpawnCleanup() caused deletion of TargetProcess twice. crbug.com/480639
    CHECK(
        AssociateCompletionPort(tracker->job.Get(), job_port_.Get(), tracker));
  } else {
    // Duplicate the process handle to give the tracking machinery
    // something valid to wait on in the tracking thread.
    HANDLE tmp_process_handle = INVALID_HANDLE_VALUE;
    if (!::DuplicateHandle(::GetCurrentProcess(), process_info.process_handle(),
                           ::GetCurrentProcess(), &tmp_process_handle,
                           SYNCHRONIZE, false, 0 /*no options*/)) {
      *last_error = ::GetLastError();
      // This may fail in the same way as Job associated processes.
      // crbug.com/480639.
      SpawnCleanup(target);
      return SBOX_ERROR_CANNOT_DUPLICATE_PROCESS_HANDLE;
    }
    base::win::ScopedHandle dup_process_handle(tmp_process_handle);
    ProcessTracker* tracker = new ProcessTracker(
        policy_base, process_info.process_id(), std::move(dup_process_handle));
    // The tracker and policy will leak if this call fails.
    ::PostQueuedCompletionStatus(job_port_.Get(), 0,
                                 THREAD_CTRL_NEW_PROCESS_TRACKER,
                                 reinterpret_cast<LPOVERLAPPED>(tracker));
  }

  *target_info = process_info.Take();
  return result;
}

ResultCode BrokerServicesBase::WaitForAllTargets() {
  ::WaitForSingleObject(no_targets_.Get(), INFINITE);
  return SBOX_ALL_OK;
}

ResultCode BrokerServicesBase::GetPolicyDiagnostics(
    std::unique_ptr<PolicyDiagnosticsReceiver> receiver) {
  CHECK(job_thread_.IsValid());
  // Post to the job thread.
  if (!::PostQueuedCompletionStatus(
          job_port_.Get(), 0, THREAD_CTRL_GET_POLICY_INFO,
          reinterpret_cast<LPOVERLAPPED>(receiver.get()))) {
    receiver->OnError(SBOX_ERROR_GENERIC);
    return SBOX_ERROR_GENERIC;
  }

  // Ownership has passed to tracker thread.
  receiver.release();
  return SBOX_ALL_OK;
}

}  // namespace sandbox
