// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/chromoting_module.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/win/scoped_handle.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/typed_buffer.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/win/rdp_desktop_session.h"

namespace remoting {

namespace {

// Holds a reference to the task runner used by the module.
base::LazyInstance<scoped_refptr<AutoThreadTaskRunner>>::DestructorAtExit
    g_module_task_runner = LAZY_INSTANCE_INITIALIZER;

// Lowers the process integrity level such that it does not exceed |max_level|.
// |max_level| is expected to be one of SECURITY_MANDATORY_XXX constants.
bool LowerProcessIntegrityLevel(DWORD max_level) {
  HANDLE temp_handle;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_WRITE,
                        &temp_handle)) {
    PLOG(ERROR) << "OpenProcessToken() failed";
    return false;
  }
  base::win::ScopedHandle token(temp_handle);

  TypedBuffer<TOKEN_MANDATORY_LABEL> mandatory_label;
  DWORD length = 0;

  // Get the size of the buffer needed to hold the mandatory label.
  BOOL result = GetTokenInformation(token.Get(), TokenIntegrityLevel,
                                    mandatory_label.get(), length, &length);
  if (!result && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    // Allocate a buffer that is large enough.
    TypedBuffer<TOKEN_MANDATORY_LABEL> buffer(length);
    mandatory_label.Swap(buffer);

    // Get the the mandatory label.
    result = GetTokenInformation(token.Get(), TokenIntegrityLevel,
                                 mandatory_label.get(), length, &length);
  }
  if (!result) {
    PLOG(ERROR) << "Failed to get the mandatory label";
    return false;
  }

  // Read the current integrity level.
  DWORD sub_authority_count =
      *GetSidSubAuthorityCount(mandatory_label->Label.Sid);
  DWORD* current_level =
      GetSidSubAuthority(mandatory_label->Label.Sid, sub_authority_count - 1);

  // Set the integrity level to |max_level| if needed.
  if (*current_level > max_level) {
    *current_level = max_level;
    if (!SetTokenInformation(token.Get(), TokenIntegrityLevel,
                             mandatory_label.get(), length)) {
      PLOG(ERROR) << "Failed to set the mandatory label";
      return false;
    }
  }

  return true;
}

}  // namespace

ChromotingModule::ChromotingModule(ATL::_ATL_OBJMAP_ENTRY* classes,
                                   ATL::_ATL_OBJMAP_ENTRY* classes_end)
    : classes_(classes), classes_end_(classes_end) {
  // Don't do anything if COM initialization failed.
  if (!com_initializer_.Succeeded()) {
    return;
  }

  ATL::_AtlComModule.ExecuteObjectMain(true);
}

ChromotingModule::~ChromotingModule() {
  // Don't do anything if COM initialization failed.
  if (!com_initializer_.Succeeded()) {
    return;
  }

  Term();
  ATL::_AtlComModule.ExecuteObjectMain(false);
}

// static
scoped_refptr<AutoThreadTaskRunner> ChromotingModule::task_runner() {
  return g_module_task_runner.Get();
}

bool ChromotingModule::Run() {
  // Don't do anything if COM initialization failed.
  if (!com_initializer_.Succeeded()) {
    return false;
  }

  // Register class objects.
  HRESULT result = RegisterClassObjects(CLSCTX_LOCAL_SERVER,
                                        REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to register class objects, result=0x" << std::hex
               << result << std::dec << ".";
    return false;
  }

  // Arrange to run |main_task_executor| until no components depend on it.
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::RunLoop run_loop;
  g_module_task_runner.Get() = new AutoThreadTaskRunner(
      main_task_executor.task_runner(), run_loop.QuitClosure());

  // Start accepting activations.
  result = CoResumeClassObjects();
  if (FAILED(result)) {
    LOG(ERROR) << "CoResumeClassObjects() failed, result=0x" << std::hex
               << result << std::dec << ".";
    return false;
  }

  // Run the loop until the module lock counter reaches zero.
  run_loop.Run();

  // Unregister class objects.
  result = RevokeClassObjects();
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to unregister class objects, result=0x" << std::hex
               << result << std::dec << ".";
    return false;
  }

  return true;
}

LONG ChromotingModule::Unlock() {
  LONG count = ATL::CAtlModuleT<ChromotingModule>::Unlock();

  if (!count) {
    // Stop accepting activations.
    HRESULT hr = CoSuspendClassObjects();
    CHECK(SUCCEEDED(hr));

    // Release the message loop reference, causing the message loop to exit.
    g_module_task_runner.Get() = nullptr;
  }

  return count;
}

HRESULT ChromotingModule::RegisterClassObjects(DWORD class_context,
                                               DWORD flags) {
  for (ATL::_ATL_OBJMAP_ENTRY* i = classes_; i != classes_end_; ++i) {
    HRESULT result = i->RegisterClassObject(class_context, flags);
    if (FAILED(result)) {
      return result;
    }
  }

  return S_OK;
}

HRESULT ChromotingModule::RevokeClassObjects() {
  for (ATL::_ATL_OBJMAP_ENTRY* i = classes_; i != classes_end_; ++i) {
    HRESULT result = i->RevokeClassObject();
    if (FAILED(result)) {
      return result;
    }
  }

  return S_OK;
}

// RdpClient entry point.
int RdpDesktopSessionMain() {
  // Lower the integrity level to medium, which is the lowest level at which
  // the RDP ActiveX control can run.
  if (!LowerProcessIntegrityLevel(SECURITY_MANDATORY_MEDIUM_RID)) {
    return kInitializationFailed;
  }

  ATL::_ATL_OBJMAP_ENTRY rdp_client_entry[] = {
      OBJECT_ENTRY(__uuidof(RdpDesktopSession), RdpDesktopSession)};

  ChromotingModule module(rdp_client_entry, rdp_client_entry + 1);
  return module.Run() ? kSuccessExitCode : kInitializationFailed;
}

}  // namespace remoting
