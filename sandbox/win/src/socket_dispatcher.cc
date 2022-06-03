// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/socket_dispatcher.h"

#include <stdint.h>
#include <winsock2.h>

#include <string>

#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_params.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox.h"

namespace sandbox {

namespace {

class SocketHandleTraits {
 public:
  typedef SOCKET Handle;

  SocketHandleTraits() = delete;
  SocketHandleTraits(const SocketHandleTraits&) = delete;
  SocketHandleTraits& operator=(const SocketHandleTraits&) = delete;

  static bool CloseHandle(SOCKET handle) { return ::closesocket(handle) == 0; }
  static bool IsHandleValid(SOCKET handle) { return handle != INVALID_SOCKET; }
  static SOCKET NullHandle() { return INVALID_SOCKET; }
};

class DummySocketVerifierTraits {
 public:
  using Handle = SOCKET;

  DummySocketVerifierTraits() = delete;
  DummySocketVerifierTraits(const DummySocketVerifierTraits&) = delete;
  DummySocketVerifierTraits& operator=(const DummySocketVerifierTraits&) =
      delete;

  static void StartTracking(SOCKET handle,
                            const void* owner,
                            const void* pc1,
                            const void* pc2) {}
  static void StopTracking(SOCKET handle,
                           const void* owner,
                           const void* pc1,
                           const void* pc2) {}
};

typedef base::win::GenericScopedHandle<SocketHandleTraits,
                                       DummySocketVerifierTraits>
    ScopedSocketHandle;

}  // namespace

SocketDispatcher::SocketDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall create_params = {
      {IpcTag::WS2SOCKET,
       {UINT32_TYPE, UINT32_TYPE, UINT32_TYPE, INOUTPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(&SocketDispatcher::WS2Socket)};

  ipc_calls_.push_back(create_params);
}

bool SocketDispatcher::SetupService(InterceptionManager* manager,
                                    IpcTag service) {
  // This IPC has no interceptions.
  return true;
}

bool SocketDispatcher::WS2Socket(IPCInfo* ipc,
                                 uint32_t af,
                                 uint32_t type,
                                 uint32_t protocol,
                                 InOutCountedBuffer* protocol_info_buffer) {
  if (af != AF_INET && af != AF_INET6)
    return false;

  if (type != SOCK_STREAM && type != SOCK_DGRAM)
    return false;

  if (protocol != IPPROTO_TCP && protocol != IPPROTO_UDP)
    return false;

  if (protocol_info_buffer->Size() != sizeof(WSAPROTOCOL_INFOW))
    return false;

  CountedParameterSet<NameBased> params;
  // Policy for the IPC just needs to exist, the parameters here do not matter.
  const wchar_t* dummy_param = L"";
  params[NameBased::NAME] = ParamPickerMake(dummy_param);

  // Verify that the target process has the permission to broker sockets.
  if (policy_base_->EvalPolicy(IpcTag::WS2SOCKET, params.GetBase()) !=
      ASK_BROKER) {
    return false;
  }

  ScopedSocketHandle local_socket(
      ::WSASocketW(af, type, protocol, nullptr, 0, WSA_FLAG_OVERLAPPED));

  if (!local_socket.IsValid()) {
    ipc->return_info.extended_count = 1;
    ipc->return_info.extended[0].unsigned_int =
        static_cast<uint32_t>(::WSAGetLastError());
    return true;
  }

  WSAPROTOCOL_INFOW* protocol_info =
      reinterpret_cast<WSAPROTOCOL_INFOW*>(protocol_info_buffer->Buffer());

  if (::WSADuplicateSocketW(local_socket.Get(), ipc->client_info->process_id,
                            protocol_info)) {
    ipc->return_info.extended_count = 1;
    ipc->return_info.extended[0].unsigned_int =
        static_cast<uint32_t>(::WSAGetLastError());
    return true;
  }

  return true;
}

}  // namespace sandbox
