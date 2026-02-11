// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/top_level_dispatcher.h"

#include <stdint.h>

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

TopLevelDispatcher::TopLevelDispatcher(PolicyBase* policy) {
  ConfigBase* config = policy->config();
  CHECK(config->IsConfigured());

  for (IpcTag service :
       {IpcTag::NTCREATEFILE, IpcTag::NTOPENFILE, IpcTag::NTSETINFO_RENAME,
        IpcTag::NTQUERYATTRIBUTESFILE, IpcTag::NTQUERYFULLATTRIBUTESFILE}) {
    if (config->NeedsIpc(service)) {
      if (!filesystem_dispatcher_) {
        filesystem_dispatcher_ = std::make_unique<FilesystemDispatcher>(policy);
      }
      ipc_targets_[service] = filesystem_dispatcher_.get();
    }
  }

  for (IpcTag service : {IpcTag::NTOPENTHREAD, IpcTag::NTOPENPROCESSTOKENEX,
                         IpcTag::CREATETHREAD}) {
    if (config->NeedsIpc(service)) {
      if (!thread_process_dispatcher_) {
        thread_process_dispatcher_ =
            std::make_unique<ThreadProcessDispatcher>();
      }
      ipc_targets_[service] = thread_process_dispatcher_.get();
    }
  }

  for (IpcTag service :
       {IpcTag::GDI_GDIDLLINITIALIZE, IpcTag::GDI_GETSTOCKOBJECT,
        IpcTag::USER_REGISTERCLASSW}) {
    if (config->NeedsIpc(service)) {
      if (!process_mitigations_win32k_dispatcher_) {
        process_mitigations_win32k_dispatcher_ =
            std::make_unique<ProcessMitigationsWin32KDispatcher>(policy);
      }
      // Technically we don't need to register for IPCs but we do need this
      // here to write the intercepts in SetupService.
      ipc_targets_[service] = process_mitigations_win32k_dispatcher_.get();
    }
  }

  if (config->NeedsIpc(IpcTag::NTCREATESECTION)) {
    signed_dispatcher_ = std::make_unique<SignedDispatcher>(policy);
    ipc_targets_[IpcTag::NTCREATESECTION] = signed_dispatcher_.get();
  }

  ipc_calls_[IpcTag::PING1] = {
      {UINT32_TYPE},
      reinterpret_cast<CallbackGeneric>(&TopLevelDispatcher::Ping1)};
  ipc_calls_[IpcTag::PING2] = {
      {INOUTPTR_TYPE},
      reinterpret_cast<CallbackGeneric>(&TopLevelDispatcher::Ping2)};
}

TopLevelDispatcher::~TopLevelDispatcher() {}

std::vector<IpcTag> TopLevelDispatcher::ipc_targets() {
  std::vector<IpcTag> results;
  for (const auto& pair : ipc_calls_) {
    results.push_back(pair.first);
  }
  for (const auto& pair : ipc_targets_) {
    results.push_back(pair.first);
  }
  return results;
}

// When an IPC is ready in any of the targets we get called. We manage an array
// of IPC dispatchers which are keyed on the IPC tag so we normally delegate
// to the appropriate dispatcher unless we can handle the IPC call ourselves.
Dispatcher* TopLevelDispatcher::OnMessageReady(IpcTag ipc_tag,
                                               const IPCParamTypes& types,
                                               CallbackGeneric* callback) {
  DCHECK(callback);
  Dispatcher* dispatcher = Dispatcher::OnMessageReady(ipc_tag, types, callback);
  if (!dispatcher) {
    dispatcher =
        GetDispatcher(ipc_tag)->OnMessageReady(ipc_tag, types, callback);
  }
  return dispatcher;
}

// Delegate to the appropriate dispatcher.
bool TopLevelDispatcher::SetupService(InterceptionManager* manager,
                                      IpcTag service) {
  if (IpcTag::PING1 == service || IpcTag::PING2 == service) {
    return true;
  }
  return GetDispatcher(service)->SetupService(manager, service);
}

// We service PING message which is a way to test a round trip of the
// IPC subsystem. We receive a integer cookie and we are expected to return the
// cookie times two (or three) and the current tick count.
bool TopLevelDispatcher::Ping1(IPCInfo* ipc, uint32_t cookie) {
  ipc->return_info.extended_count = 2;
  ipc->return_info.extended[0].unsigned_int = ::GetTickCount();
  ipc->return_info.extended[1].unsigned_int = 2 * cookie;
  return true;
}

bool TopLevelDispatcher::Ping2(IPCInfo* ipc, CountedBuffer* io_buffer) {
  if (sizeof(uint32_t) != io_buffer->size()) {
    return false;
  }
  uint32_t* cookie = reinterpret_cast<uint32_t*>(io_buffer->data());
  *cookie = (*cookie) * 3;
  return true;
}

Dispatcher* TopLevelDispatcher::GetDispatcher(IpcTag ipc_tag) {
  CHECK(ipc_targets_.contains(ipc_tag));
  return ipc_targets_[ipc_tag];
}

}  // namespace sandbox
