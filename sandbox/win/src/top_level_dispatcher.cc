// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/top_level_dispatcher.h"

#include <stdint.h>
#include <string.h>

#include "base/logging.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/filesystem_dispatcher.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/named_pipe_dispatcher.h"
#include "sandbox/win/src/process_mitigations_win32k_dispatcher.h"
#include "sandbox/win/src/process_thread_dispatcher.h"
#include "sandbox/win/src/registry_dispatcher.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/signed_dispatcher.h"
#include "sandbox/win/src/sync_dispatcher.h"

namespace sandbox {

TopLevelDispatcher::TopLevelDispatcher(PolicyBase* policy) : policy_(policy) {
  // Initialize the IPC dispatcher array.
  memset(ipc_targets_, 0, sizeof(ipc_targets_));
  Dispatcher* dispatcher;

  dispatcher = new FilesystemDispatcher(policy_);
  ipc_targets_[static_cast<size_t>(IpcTag::NTCREATEFILE)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::NTOPENFILE)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::NTSETINFO_RENAME)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::NTQUERYATTRIBUTESFILE)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::NTQUERYFULLATTRIBUTESFILE)] =
      dispatcher;
  filesystem_dispatcher_.reset(dispatcher);

  dispatcher = new NamedPipeDispatcher(policy_);
  ipc_targets_[static_cast<size_t>(IpcTag::CREATENAMEDPIPEW)] = dispatcher;
  named_pipe_dispatcher_.reset(dispatcher);

  dispatcher = new ThreadProcessDispatcher(policy_);
  ipc_targets_[static_cast<size_t>(IpcTag::NTOPENTHREAD)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::NTOPENPROCESS)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::CREATEPROCESSW)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::NTOPENPROCESSTOKEN)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::NTOPENPROCESSTOKENEX)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::CREATETHREAD)] = dispatcher;
  thread_process_dispatcher_.reset(dispatcher);

  dispatcher = new SyncDispatcher(policy_);
  ipc_targets_[static_cast<size_t>(IpcTag::CREATEEVENT)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::OPENEVENT)] = dispatcher;
  sync_dispatcher_.reset(dispatcher);

  dispatcher = new RegistryDispatcher(policy_);
  ipc_targets_[static_cast<size_t>(IpcTag::NTCREATEKEY)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::NTOPENKEY)] = dispatcher;
  registry_dispatcher_.reset(dispatcher);

  dispatcher = new ProcessMitigationsWin32KDispatcher(policy_);
  ipc_targets_[static_cast<size_t>(IpcTag::GDI_GDIDLLINITIALIZE)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::GDI_GETSTOCKOBJECT)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::USER_REGISTERCLASSW)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::USER_ENUMDISPLAYMONITORS)] =
      dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::USER_ENUMDISPLAYDEVICES)] =
      dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::USER_GETMONITORINFO)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::GDI_CREATEOPMPROTECTEDOUTPUTS)] =
      dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::GDI_GETCERTIFICATE)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::GDI_GETCERTIFICATESIZE)] =
      dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::GDI_DESTROYOPMPROTECTEDOUTPUT)] =
      dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::GDI_CONFIGUREOPMPROTECTEDOUTPUT)] =
      dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::GDI_GETOPMINFORMATION)] = dispatcher;
  ipc_targets_[static_cast<size_t>(IpcTag::GDI_GETOPMRANDOMNUMBER)] =
      dispatcher;
  ipc_targets_[static_cast<size_t>(
      IpcTag::GDI_GETSUGGESTEDOPMPROTECTEDOUTPUTARRAYSIZE)] = dispatcher;
  ipc_targets_[static_cast<size_t>(
      IpcTag::GDI_SETOPMSIGNINGKEYANDSEQUENCENUMBERS)] = dispatcher;
  process_mitigations_win32k_dispatcher_.reset(dispatcher);

  dispatcher = new SignedDispatcher(policy_);
  ipc_targets_[static_cast<size_t>(IpcTag::NTCREATESECTION)] = dispatcher;
  signed_dispatcher_.reset(dispatcher);
}

TopLevelDispatcher::~TopLevelDispatcher() {}

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
    return nullptr;
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
    return false;
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
  if (ipc_tag >= IpcTag::LAST || ipc_tag <= IpcTag::UNUSED)
    return nullptr;

  return ipc_targets_[static_cast<size_t>(ipc_tag)];
}

}  // namespace sandbox
