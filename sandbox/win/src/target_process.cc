// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/target_process.h"

#include <windows.h>

#include <processenv.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/free_deleter.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/win/access_token.h"
#include "base/win/current_module.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_util.h"
#include "base/win/startup_information.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/sharedmem_ipc_server.h"
#include "sandbox/win/src/startup_information_helper.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {

// Parses a null-terminated input string of an environment block. The key is
// placed into the given string, and the total length of the line, including
// the terminating null, is returned.
size_t ParseEnvLine(const wchar_t* input, std::wstring* key) {
  // Skip to the equals or end of the string, this is the key.
  size_t cur = 0;
  while (input[cur] && input[cur] != '=') {
    cur++;
  }
  *key = std::wstring(&input[0], cur);

  // Now just skip to the end of the string.
  while (input[cur]) {
    cur++;
  }
  return cur + 1;
}

void CopyPolicyToTarget(base::span<const uint8_t> source, void* dest) {
  if (!source.size()) {
    return;
  }
  memcpy(dest, source.data(), source.size());
  sandbox::PolicyGlobal* policy =
      reinterpret_cast<sandbox::PolicyGlobal*>(dest);

  size_t offset = reinterpret_cast<size_t>(source.data());

  for (size_t i = 0; i < sandbox::kSandboxIpcCount; i++) {
    size_t buffer = reinterpret_cast<size_t>(policy->entry[i]);
    if (buffer) {
      buffer -= offset;
      policy->entry[i] = reinterpret_cast<sandbox::PolicyBuffer*>(buffer);
    }
  }
}

// Checks that the impersonation token was applied successfully and hasn't been
// reverted to an identification level token.
bool CheckImpersonationToken(HANDLE thread) {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromThread(thread);
  if (!token.has_value()) {
    return false;
  }
  return !token->IsIdentification();
}

}  // namespace

// 'SAND'
SANDBOX_INTERCEPT DWORD g_sentinel_value_start = 0x53414E44;
SANDBOX_INTERCEPT HANDLE g_shared_section;
SANDBOX_INTERCEPT size_t g_shared_IPC_size;
// The following may be zero if not needed in the child.
SANDBOX_INTERCEPT size_t g_shared_policy_size;
SANDBOX_INTERCEPT size_t g_delegate_data_size;
// 'BOXY'
SANDBOX_INTERCEPT DWORD g_sentinel_value_end = 0x424F5859;

TargetProcess::TargetProcess(base::win::AccessToken initial_token,
                             base::win::AccessToken lockdown_token,
                             ThreadPool* thread_pool)
    // This object owns everything initialized here except thread_pool.
    : lockdown_token_(std::move(lockdown_token)),
      initial_token_(std::move(initial_token)),
      thread_pool_(thread_pool),
      base_address_(nullptr) {}

TargetProcess::~TargetProcess() {
  // Give a chance to the process to die. In most cases the JOB_KILL_ON_CLOSE
  // will take effect only when the context changes. As far as the testing went,
  // this wait was enough to switch context and kill the processes in the job.
  // If this process is already dead, the function will return without waiting.
  // For now, this wait is there only to do a best effort to prevent some leaks
  // from showing up in purify.
  if (sandbox_process_info_.IsValid()) {
    ::WaitForSingleObject(sandbox_process_info_.process_handle(), 50);
    // Terminate the process if it's still alive, as its IPC server is going
    // away. 1 is RESULT_CODE_KILLED.
    ::TerminateProcess(sandbox_process_info_.process_handle(), 1);
  }

  // ipc_server_ references our process handle, so make sure the former is shut
  // down before the latter is closed (by ScopedProcessInformation).
  ipc_server_.reset();
}

// Creates the target (child) process suspended and assigns it to the job
// object.
ResultCode TargetProcess::Create(
    const wchar_t* exe_path,
    const wchar_t* command_line,
    std::unique_ptr<StartupInformationHelper> startup_info_helper,
    base::win::ScopedProcessInformation* target_info,
    DWORD* win_error) {
  exe_name_.reset(_wcsdup(exe_path));

  base::win::StartupInformation* startup_info =
      startup_info_helper->GetStartupInformation();

  // the command line needs to be writable by CreateProcess().
  std::unique_ptr<wchar_t, base::FreeDeleter> cmd_line(_wcsdup(command_line));

  // Start the target process suspended.
  DWORD flags =
      CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | DETACHED_PROCESS;

  if (startup_info->has_extended_startup_info())
    flags |= EXTENDED_STARTUPINFO_PRESENT;

  std::wstring new_env;

  if (startup_info_helper->IsEnvironmentFiltered()) {
    wchar_t* old_environment = ::GetEnvironmentStringsW();
    if (!old_environment) {
      return SBOX_ERROR_CANNOT_OBTAIN_ENVIRONMENT;
    }

    // Only copy a limited list of variables to the target from the broker's
    // environment. These are
    //  * "Path", "SystemDrive", "SystemRoot", "TEMP", "TMP": Needed for normal
    //    operation and tests.
    //  * "LOCALAPPDATA": Needed for App Container processes.
    //  * "CHROME_CRASHPAD_PIPE_NAME": Needed for crashpad.
    static constexpr std::wstring_view to_keep[] = {
        L"Path",
        L"SystemDrive",
        L"SystemRoot",
        L"TEMP",
        L"TMP",
        L"LOCALAPPDATA",
        L"CHROME_CRASHPAD_PIPE_NAME"};

    new_env = FilterEnvironment(old_environment, to_keep);
    ::FreeEnvironmentStringsW(old_environment);
  }

  bool inherit_handles = startup_info_helper->ShouldInheritHandles();
  PROCESS_INFORMATION temp_process_info = {};
  if (!::CreateProcessAsUserW(lockdown_token_.get(), exe_path, cmd_line.get(),
                              nullptr,  // No security attribute.
                              nullptr,  // No thread attribute.
                              inherit_handles, flags,
                              new_env.empty() ? nullptr : std::data(new_env),
                              nullptr,  // Use current directory of the caller.
                              startup_info->startup_info(),
                              &temp_process_info)) {
    *win_error = ::GetLastError();
    return SBOX_ERROR_CREATE_PROCESS;
  }
  base::win::ScopedProcessInformation process_info(temp_process_info);

  // Change the token of the main thread of the new process for the
  // impersonation token with more rights. This allows the target to start;
  // otherwise it will crash too early for us to help.
  HANDLE temp_thread = process_info.thread_handle();
  if (!::SetThreadToken(&temp_thread, initial_token_.get())) {
    *win_error = ::GetLastError();
    ::TerminateProcess(process_info.process_handle(), 0);
    return SBOX_ERROR_SET_THREAD_TOKEN;
  }
  if (!CheckImpersonationToken(process_info.thread_handle())) {
    *win_error = ERROR_BAD_IMPERSONATION_LEVEL;
    ::TerminateProcess(process_info.process_handle(), 0);
    return SBOX_ERROR_SET_THREAD_TOKEN;
  }

  if (!target_info->DuplicateFrom(process_info)) {
    *win_error = ::GetLastError();  // This may or may not be correct.
    ::TerminateProcess(process_info.process_handle(), 0);
    return SBOX_ERROR_DUPLICATE_TARGET_INFO;
  }

  base_address_ = GetProcessBaseAddress(process_info.process_handle());
  DCHECK(base_address_);
  if (!base_address_) {
    *win_error = ::GetLastError();
    ::TerminateProcess(process_info.process_handle(), 0);
    return SBOX_ERROR_CANNOT_FIND_BASE_ADDRESS;
  }

  if (base_address_ != CURRENT_MODULE()) {
    ::TerminateProcess(process_info.process_handle(), 0);
    return SBOX_ERROR_INVALID_TARGET_BASE_ADDRESS;
  }

  sandbox_process_info_.Set(process_info.Take());
  return SBOX_ALL_OK;
}

ResultCode TargetProcess::TransferVariable(const char* name,
                                           const void* local_address,
                                           void* target_address,
                                           size_t size) {
  if (!sandbox_process_info_.IsValid())
    return SBOX_ERROR_UNEXPECTED_CALL;

  SIZE_T written;
  if (!::WriteProcessMemory(sandbox_process_info_.process_handle(),
                            target_address, local_address, size, &written)) {
    return SBOX_ERROR_CANNOT_WRITE_VARIABLE_VALUE;
  }
  if (written != size)
    return SBOX_ERROR_INVALID_WRITE_VARIABLE_SIZE;

  return SBOX_ALL_OK;
}

// Construct the IPC server and the IPC dispatcher. When the target does
// an IPC it will eventually call the dispatcher.
ResultCode TargetProcess::Init(
    Dispatcher* ipc_dispatcher,
    std::optional<base::span<const uint8_t>> policy,
    std::optional<base::span<const uint8_t>> delegate_data,
    uint32_t shared_IPC_size,
    DWORD* win_error) {
  ResultCode ret = VerifySentinels();
  if (ret != SBOX_ALL_OK)
    return ret;

  // We need to map the shared memory on the target. This is necessary for
  // any IPC that needs to take place, even if the target has not yet hit
  // the main( ) function or even has initialized the CRT. So here we set
  // the handle to the shared section. The target on the first IPC must do
  // the rest, which boils down to calling MapViewofFile()

  // We use this single memory pool for IPC and for policy.
  size_t shared_mem_size = shared_IPC_size;
  if (policy.has_value()) {
    shared_mem_size += policy->size();
  }
  if (delegate_data.has_value()) {
    shared_mem_size += delegate_data->size();
  }

  // This region should be small, so we only pass dwMaximumSizeLow below.
  CHECK(shared_mem_size <= std::numeric_limits<DWORD>::max());

  shared_section_.Set(::CreateFileMappingW(
      INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_COMMIT, 0,
      static_cast<DWORD>(shared_mem_size), nullptr));
  if (!shared_section_.is_valid()) {
    *win_error = ::GetLastError();
    return SBOX_ERROR_CREATE_FILE_MAPPING;
  }

  void* shared_memory = ::MapViewOfFile(
      shared_section_.get(), FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0);
  if (!shared_memory) {
    *win_error = ::GetLastError();
    return SBOX_ERROR_MAP_VIEW_OF_SHARED_SECTION;
  }

  // The IPC area is just zeros so we skip over it.
  size_t current_offset = shared_IPC_size;
  // PolicyGlobal region.
  if (policy.has_value()) {
    CopyPolicyToTarget(policy.value(),
                       reinterpret_cast<char*>(shared_memory) + current_offset);
    current_offset += policy->size();
  }

  // Delegate Data region.
  if (delegate_data.has_value()) {
    memcpy(reinterpret_cast<char*>(shared_memory) + current_offset,
           delegate_data->data(), delegate_data->size());
    current_offset += delegate_data->size();
  }

  // After all regions are written we should be at the end of the allocation.
  CHECK_EQ(current_offset, shared_mem_size);

  // Set the global variables in the target. These are not used on the broker.
  size_t transfer_shared_IPC_size = shared_IPC_size;
  static_assert(sizeof(g_shared_IPC_size) == sizeof(transfer_shared_IPC_size));
  ret = TransferVariable("g_shared_IPC_size", &transfer_shared_IPC_size,
                         &g_shared_IPC_size, sizeof(g_shared_IPC_size));
  if (SBOX_ALL_OK != ret) {
    *win_error = ::GetLastError();
    return ret;
  }
  if (policy.has_value()) {
    size_t transfer_shared_policy_size = policy->size();
    static_assert(sizeof(g_shared_policy_size) ==
                  sizeof(transfer_shared_policy_size));
    ret = TransferVariable("g_shared_policy_size", &transfer_shared_policy_size,
                           &g_shared_policy_size, sizeof(g_shared_policy_size));
    if (SBOX_ALL_OK != ret) {
      *win_error = ::GetLastError();
      return ret;
    }
  }
  if (delegate_data.has_value()) {
    size_t transfer_delegate_data_size = delegate_data->size();
    static_assert(sizeof(g_delegate_data_size) ==
                  sizeof(transfer_delegate_data_size));
    ret = TransferVariable("g_delegate_data_size", &transfer_delegate_data_size,
                           &g_delegate_data_size, sizeof(g_delegate_data_size));
    if (SBOX_ALL_OK != ret) {
      *win_error = ::GetLastError();
      return ret;
    }
  }

  ipc_server_ = std::make_unique<SharedMemIPCServer>(
      sandbox_process_info_.process_handle(),
      sandbox_process_info_.process_id(), thread_pool_, ipc_dispatcher);

  if (!ipc_server_->Init(shared_memory, shared_IPC_size, kIPCChannelSize))
    return SBOX_ERROR_NO_SPACE;

  DWORD access = FILE_MAP_READ | FILE_MAP_WRITE | SECTION_QUERY;
  HANDLE target_shared_section;
  if (!::DuplicateHandle(::GetCurrentProcess(), shared_section_.get(),
                         sandbox_process_info_.process_handle(),
                         &target_shared_section, access, false, 0)) {
    *win_error = ::GetLastError();
    return SBOX_ERROR_DUPLICATE_SHARED_SECTION;
  }

  static_assert(sizeof(g_shared_section) == sizeof(target_shared_section));
  ret = TransferVariable("g_shared_section", &target_shared_section,
                         &g_shared_section, sizeof(g_shared_section));
  if (SBOX_ALL_OK != ret) {
    *win_error = ::GetLastError();
    return ret;
  }

  // After this point we cannot use this handle anymore.
  ::CloseHandle(sandbox_process_info_.TakeThreadHandle());

  return SBOX_ALL_OK;
}

void TargetProcess::Terminate() {
  if (!sandbox_process_info_.IsValid())
    return;

  ::TerminateProcess(sandbox_process_info_.process_handle(), 0);
}

ResultCode TargetProcess::VerifySentinels() {
  if (!sandbox_process_info_.IsValid())
    return SBOX_ERROR_UNEXPECTED_CALL;
  DWORD value = 0;
  SIZE_T read;

  if (!::ReadProcessMemory(sandbox_process_info_.process_handle(),
                           &g_sentinel_value_start, &value, sizeof(DWORD),
                           &read)) {
    return SBOX_ERROR_CANNOT_READ_SENTINEL_VALUE;
  }
  if (read != sizeof(DWORD))
    return SBOX_ERROR_INVALID_READ_SENTINEL_SIZE;
  if (value != g_sentinel_value_start)
    return SBOX_ERROR_MISMATCH_SENTINEL_VALUE;
  if (!::ReadProcessMemory(sandbox_process_info_.process_handle(),
                           &g_sentinel_value_end, &value, sizeof(DWORD),
                           &read)) {
    return SBOX_ERROR_CANNOT_READ_SENTINEL_VALUE;
  }
  if (read != sizeof(DWORD))
    return SBOX_ERROR_INVALID_READ_SENTINEL_SIZE;
  if (value != g_sentinel_value_end)
    return SBOX_ERROR_MISMATCH_SENTINEL_VALUE;

  return SBOX_ALL_OK;
}

// static
std::unique_ptr<TargetProcess> TargetProcess::MakeTargetProcessForTesting(
    HANDLE process,
    HMODULE base_address) {
  auto target = std::make_unique<TargetProcess>(
      base::win::AccessToken::FromCurrentProcess().value(),
      base::win::AccessToken::FromCurrentProcess().value(), nullptr);
  PROCESS_INFORMATION process_info = {};
  process_info.hProcess = process;
  target->sandbox_process_info_.Set(process_info);
  target->base_address_ = base_address;
  return target;
}

// static
std::wstring TargetProcess::FilterEnvironment(
    const wchar_t* env,
    const base::span<const std::wstring_view> to_keep) {
  std::wstring result;

  // Iterate all of the environment strings.
  const wchar_t* ptr = env;
  while (*ptr) {
    std::wstring key;
    size_t line_length = ParseEnvLine(ptr, &key);

    // Keep only values specified in the keep vector.
    if (std::find(to_keep.begin(), to_keep.end(), key) != to_keep.end()) {
      result.append(ptr, line_length);
    }
    ptr += line_length;
  }

  // Add the terminating NUL.
  result.push_back('\0');
  return result;
}

}  // namespace sandbox
