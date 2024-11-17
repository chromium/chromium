// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/top_level_dispatcher.h"

#include <stdint.h>
#include <string.h>

#include "base/check.h"
#include "base/notreached.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/filesystem_dispatcher.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/process_mitigations_win32k_dispatcher.h"
#include "sandbox/win/src/process_thread_dispatcher.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/signed_dispatcher.h"

namespace sandbox {

TopLevelDispatcher::TopLevelDispatcher(PolicyBase* policy) : policy_(policy) {
  // Initialize the IPC dispatcher array.
  memset(ipc_targets_, 0, sizeof(ipc_targets_));

  ConfigBase* config = policy_->config();
  CHECK(config->IsConfigured());

  for (IpcTag service :
       {IpcTag::NTCREATEFILE, IpcTag::NTOPENFILE, IpcTag::NTSETINFO_RENAME,
        IpcTag::NTQUERYATTRIBUTESFILE, IpcTag::NTQUERYFULLATTRIBUTESFILE}) {
    if (config->NeedsIpc(service)) {
      if (!filesystem_dispatcher_) {
        filesystem_dispatcher_ =
            std::make_unique<FilesystemDispatcher>(policy_);
      }
      ipc_targets_[static_cast<size_t>(service)] = filesystem_dispatcher_.get();
    }
  }

  for (IpcTag service : {IpcTag::NTOPENTHREAD, IpcTag::NTOPENPROCESSTOKENEX,
                         IpcTag::CREATETHREAD}) {
    if (config->NeedsIpc(service)) {
      if (!thread_process_dispatcher_) {
        thread_process_dispatcher_ =
            std::make_unique<ThreadProcessDispatcher>();
      }
      ipc_targets_[static_cast<size_t>(service)] =
          thread_process_dispatcher_.get();
    }
  }

  for (IpcTag service :
       {IpcTag::GDI_GDIDLLINITIALIZE, IpcTag::GDI_GETSTOCKOBJECT,
        IpcTag::USER_REGISTERCLASSW}) {
    if (config->NeedsIpc(service)) {
      if (!process_mitigations_win32k_dispatcher_) {
        process_mitigations_win32k_dispatcher_ =
            std::make_unique<ProcessMitigationsWin32KDispatcher>(policy_);
      }
      // Technically we don't need to register for IPCs but we do need this
      // here to write the intercepts in SetupService.
      ipc_targets_[static_cast<size_t>(service)] =
          process_mitigations_win32k_dispatcher_.get();
    }
  }

  if (config->NeedsIpc(IpcTag::NTCREATESECTION)) {
    signed_dispatcher_ = std::make_unique<SignedDispatcher>(policy_);
    ipc_targets_[static_cast<size_t>(IpcTag::NTCREATESECTION)] =
        signed_dispatcher_.get();
  }
}

TopLevelDispatcher::~TopLevelDispatcher() {}

std::vector<IpcTag> TopLevelDispatcher::ipc_targets() {
  std::vector<IpcTag> results = {IpcTag::PING1, IpcTag::PING2};
  for (size_t ipc = 0; ipc < kSandboxIpcCount; ipc++) {
    if (ipc_targets_[ipc]) {
      results.push_back(static_cast<IpcTag>(ipc));
    }
  }
  return results;
}

// When an IPC is ready in any of the targets we get called. We manage an array
// of IPC dispatchers which are keyed on the IPC tag so we normally delegate
// to the appropriate dispatcher unless we can handle the IPC call ourselves.
Dispatcher* TopLevelDispatcher::OnMessageReady(IPCParams* ipc,
                                               CallbackGeneric* callback) {
  DCHECK(callback);
  static const IPCParams ping1 = {IpcTag::PING1, {UINT32_TYPE}};
  static const IPCParams ping2 = {IpcTag::PING2, {INOUTPTR_TYPE}};

  if (ping1.Matches(ipc) || ping2.Matches(ipc)) {
    *callback = reinterpret_cast<CallbackGeneric>(
        static_cast<Callback1>(&TopLevelDispatcher::Ping));
    return this;
  }

  Dispatcher* dispatcher = GetDispatcher(ipc->ipc_tag);
  if (!dispatcher) {
    NOTREACHED();
  }
  return dispatcher->OnMessageReady(ipc, callback);
}

// Delegate to the appropriate dispatcher.
bool TopLevelDispatcher::SetupService(InterceptionManager* manager,
                                      IpcTag service) {
  if (IpcTag::PING1 == service || IpcTag::PING2 == service)
    return true;

  Dispatcher* dispatcher = GetDispatcher(service);
  if (!dispatcher) {
    NOTREACHED();
  }
  return dispatcher->SetupService(manager, service);
}

// We service PING message which is a way to test a round trip of the
// IPC subsystem. We receive a integer cookie and we are expected to return the
// cookie times two (or three) and the current tick count.
bool TopLevelDispatcher::Ping(IPCInfo* ipc, void* arg1) {
  switch (ipc->ipc_tag) {
    case IpcTag::PING1: {
      IPCInt ipc_int(arg1);
      uint32_t cookie = ipc_int.As32Bit();
      ipc->return_info.extended_count = 2;
      ipc->return_info.extended[0].unsigned_int = ::GetTickCount();
      ipc->return_info.extended[1].unsigned_int = 2 * cookie;
      return true;
    }
    case IpcTag::PING2: {
      CountedBuffer* io_buffer = reinterpret_cast<CountedBuffer*>(arg1);
      if (sizeof(uint32_t) != io_buffer->Size())
        return false;

      uint32_t* cookie = reinterpret_cast<uint32_t*>(io_buffer->Buffer());
      *cookie = (*cookie) * 3;
      return true;
    }
    default:
      return false;
  }
}

Dispatcher* TopLevelDispatcher::GetDispatcher(IpcTag ipc_tag) {
  if (ipc_tag > IpcTag::kMaxValue || ipc_tag == IpcTag::UNUSED) {
    return nullptr;
  }

  return ipc_targets_[static_cast<size_t>(ipc_tag)];
}

}  // namespace sandbox
