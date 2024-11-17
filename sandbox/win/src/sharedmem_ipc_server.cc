// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/sharedmem_ipc_server.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "sandbox/win/src/crosscall_params.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/ipc_args.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/threadpool.h"

namespace {
// This handle must not be closed.
volatile HANDLE g_alive_mutex = nullptr;
}  // namespace

namespace sandbox {

SharedMemIPCServer::ServerControl::ServerControl() {}

SharedMemIPCServer::ServerControl::~ServerControl() {}

SharedMemIPCServer::SharedMemIPCServer(HANDLE target_process,
                                       DWORD target_process_id,
                                       ThreadPool* thread_pool,
                                       Dispatcher* dispatcher)
    : client_control_(nullptr),
      thread_pool_(thread_pool),
      target_process_(target_process),
      target_process_id_(target_process_id),
      call_dispatcher_(dispatcher) {
  // We create a initially owned mutex. If the server dies unexpectedly,
  // the thread that owns it will fail to release the lock and windows will
  // report to the target (when it tries to acquire it) that the wait was
  // abandoned. Note: We purposely leak the local handle because we want it to
  // be closed by Windows itself so it is properly marked as abandoned if the
  // server dies.
  if (!g_alive_mutex) {
    HANDLE mutex = ::CreateMutexW(nullptr, true, nullptr);
    if (::InterlockedCompareExchangePointer(&g_alive_mutex, mutex, nullptr)) {
      // We lost the race to create the mutex.
      ::CloseHandle(mutex);
    }
  }
}

SharedMemIPCServer::~SharedMemIPCServer() {
  // Free the wait handles associated with the thread pool.
  if (!thread_pool_->UnRegisterWaits(this)) {
    // Better to leak than to crash.
    return;
  }
  server_contexts_.clear();

  if (client_control_)
    ::UnmapViewOfFile(client_control_);
}

bool SharedMemIPCServer::Init(void* shared_mem,
                              uint32_t shared_size,
                              uint32_t channel_size) {
  // The shared memory needs to be at least as big as a channel.
  if (shared_size < channel_size) {
    return false;
  }
  // The channel size should be aligned.
  if (0 != (channel_size % 32)) {
    return false;
  }

  // Calculate how many channels we can fit in the shared memory.
  shared_size -= offsetof(IPCControl, channels);
  size_t channel_count = shared_size / (sizeof(ChannelControl) + channel_size);

  // If we cannot fit even one channel we bail out.
  if (0 == channel_count) {
    return false;
  }
  // Calculate the start of the first channel.
  size_t base_start =
      (sizeof(ChannelControl) * channel_count) + offsetof(IPCControl, channels);

  client_control_ = reinterpret_cast<IPCControl*>(shared_mem);
  client_control_->channels_count = 0;

  // This is the initialization that we do per-channel. Basically:
  // 1) make two events (ping & pong)
  // 2) create handles to the events for the client and the server.
  // 3) initialize the channel (client_context) with the state.
  // 4) initialize the server side of the channel (service_context).
  // 5) call the thread provider RegisterWait to register the ping events.
  for (size_t ix = 0; ix != channel_count; ++ix) {
    ChannelControl* client_context = &client_control_->channels[ix];
    ServerControl* service_context = new ServerControl;
    server_contexts_.push_back(base::WrapUnique(service_context));

    if (!MakeEvents(&service_context->ping_event, &service_context->pong_event,
                    &client_context->ping_event, &client_context->pong_event)) {
      return false;
    }

    client_context->channel_base = base_start;
    client_context->state = kFreeChannel;

    // Note that some of these values are available as members of this object
    // but we put them again into the service_context because we will be called
    // on a static method (ThreadPingEventReady). In particular, target_process_
    // is a raw handle that is not owned by this object (it's owned by the
    // owner of this object), and we are storing it in multiple places.
    service_context->shared_base = reinterpret_cast<char*>(shared_mem);
    service_context->channel_size = channel_size;
    service_context->channel = client_context;
    service_context->channel_buffer =
        service_context->shared_base + client_context->channel_base;
    service_context->dispatcher = call_dispatcher_;
    service_context->target_info.process = target_process_;
    service_context->target_info.process_id = target_process_id_;
    // Advance to the next channel.
    base_start += channel_size;
    // Register the ping event with the threadpool.
    thread_pool_->RegisterWait(this, service_context->ping_event.get(),
                               ThreadPingEventReady, service_context);
  }
  if (!::DuplicateHandle(::GetCurrentProcess(), g_alive_mutex, target_process_,
                         &client_control_->server_alive,
                         SYNCHRONIZE | EVENT_MODIFY_STATE, false, 0)) {
    return false;
  }
  // This last setting indicates to the client all is setup.
  client_control_->channels_count = channel_count;
  return true;
}

bool SharedMemIPCServer::InvokeCallback(const ServerControl* service_context,
                                        void* ipc_buffer,
                                        CrossCallReturn* call_result) {
  // Set the default error code;
  SetCallError(SBOX_ERROR_INVALID_IPC, call_result);
  uint32_t output_size = 0;
  // Parse, verify and copy the message. The handler operates on a copy
  // of the message so the client cannot play dirty tricks by changing the
  // data in the channel while the IPC is being processed.
  std::unique_ptr<CrossCallParamsEx> params(CrossCallParamsEx::CreateFromBuffer(
      ipc_buffer, service_context->channel_size, &output_size));
  if (!params.get())
    return false;

  IpcTag tag = params->GetTag();
  static_assert(0 == INVALID_TYPE, "incorrect type enum");
  IPCParams ipc_params = {tag};

  void* args[kMaxIpcParams];
  if (!GetArgs(params.get(), &ipc_params, args))
    return false;

  IPCInfo ipc_info = {tag};
  ipc_info.client_info = &service_context->target_info;
  Dispatcher* dispatcher = service_context->dispatcher;
  DCHECK(dispatcher);
  bool error = true;
  Dispatcher* handler = nullptr;

  Dispatcher::CallbackGeneric callback_generic;
  handler = dispatcher->OnMessageReady(&ipc_params, &callback_generic);
  if (handler) {
    switch (params->GetParamsCount()) {
      case 0: {
        // Ask the IPC dispatcher if it can service this IPC.
        Dispatcher::Callback0 callback =
            reinterpret_cast<Dispatcher::Callback0>(callback_generic);
        if (!(handler->*callback)(&ipc_info))
          break;
        error = false;
        break;
      }
      case 1: {
        Dispatcher::Callback1 callback =
            reinterpret_cast<Dispatcher::Callback1>(callback_generic);
        if (!(handler->*callback)(&ipc_info, args[0]))
          break;
        error = false;
        break;
      }
      case 2: {
        Dispatcher::Callback2 callback =
            reinterpret_cast<Dispatcher::Callback2>(callback_generic);
        if (!(handler->*callback)(&ipc_info, args[0], args[1]))
          break;
        error = false;
        break;
      }
      case 3: {
        Dispatcher::Callback3 callback =
            reinterpret_cast<Dispatcher::Callback3>(callback_generic);
        if (!(handler->*callback)(&ipc_info, args[0], args[1], args[2]))
          break;
        error = false;
        break;
      }
      case 4: {
        Dispatcher::Callback4 callback =
            reinterpret_cast<Dispatcher::Callback4>(callback_generic);
        if (!(handler->*callback)(&ipc_info, args[0], args[1], args[2],
                                  args[3]))
          break;
        error = false;
        break;
      }
      case 5: {
        Dispatcher::Callback5 callback =
            reinterpret_cast<Dispatcher::Callback5>(callback_generic);
        if (!(handler->*callback)(&ipc_info, args[0], args[1], args[2], args[3],
                                  args[4]))
          break;
        error = false;
        break;
      }
      case 6: {
        Dispatcher::Callback6 callback =
            reinterpret_cast<Dispatcher::Callback6>(callback_generic);
        if (!(handler->*callback)(&ipc_info, args[0], args[1], args[2], args[3],
                                  args[4], args[5]))
          break;
        error = false;
        break;
      }
      case 7: {
        Dispatcher::Callback7 callback =
            reinterpret_cast<Dispatcher::Callback7>(callback_generic);
        if (!(handler->*callback)(&ipc_info, args[0], args[1], args[2], args[3],
                                  args[4], args[5], args[6]))
          break;
        error = false;
        break;
      }
      case 8: {
        Dispatcher::Callback8 callback =
            reinterpret_cast<Dispatcher::Callback8>(callback_generic);
        if (!(handler->*callback)(&ipc_info, args[0], args[1], args[2], args[3],
                                  args[4], args[5], args[6], args[7]))
          break;
        error = false;
        break;
      }
      case 9: {
        Dispatcher::Callback9 callback =
            reinterpret_cast<Dispatcher::Callback9>(callback_generic);
        if (!(handler->*callback)(&ipc_info, args[0], args[1], args[2], args[3],
                                  args[4], args[5], args[6], args[7], args[8]))
          break;
        error = false;
        break;
      }
      default: {
        NOTREACHED();
      }
    }
  }

  if (error) {
    if (handler)
      SetCallError(SBOX_ERROR_FAILED_IPC, call_result);
  } else {
    memcpy(call_result, &ipc_info.return_info, sizeof(*call_result));
    SetCallSuccess(call_result);
    if (params->IsInOut()) {
      // Maybe the params got changed by the broker. We need to upadte the
      // memory section.
      memcpy(ipc_buffer, params.get(), output_size);
    }
  }

  ReleaseArgs(&ipc_params, args);

  return !error;
}

// This function gets called by a thread from the thread pool when a
// ping event fires. The context is the same as passed in the RegisterWait()
// call above.
void __stdcall SharedMemIPCServer::ThreadPingEventReady(void* context,
                                                        unsigned char) {
  if (!context) {
    DCHECK(false);
    return;
  }
  ServerControl* service_context = reinterpret_cast<ServerControl*>(context);
  // Since the event fired, the channel *must* be busy. Change to kAckChannel
  // while we service it.
  LONG last_state = ::InterlockedCompareExchange(
      &service_context->channel->state, kAckChannel, kBusyChannel);
  if (kBusyChannel != last_state) {
    DCHECK(false);
    return;
  }

  // Prepare the result structure. At this point we will return some result
  // even if the IPC is invalid, malformed or has no handler.
  CrossCallReturn call_result = {0};
  void* buffer = service_context->channel_buffer;

  InvokeCallback(service_context, buffer, &call_result);

  // Copy the answer back into the channel and signal the pong event. This
  // should wake up the client so it can finish the ipc cycle.
  CrossCallParams* call_params = reinterpret_cast<CrossCallParams*>(buffer);
  memcpy(call_params->GetCallReturn(), &call_result, sizeof(call_result));
  ::InterlockedExchange(&service_context->channel->state, kAckChannel);
  ::SetEvent(service_context->pong_event.get());
}

bool SharedMemIPCServer::MakeEvents(base::win::ScopedHandle* server_ping,
                                    base::win::ScopedHandle* server_pong,
                                    HANDLE* client_ping,
                                    HANDLE* client_pong) {
  // Note that the IPC client has no right to delete the events. That would
  // cause problems. The server *owns* the events.
  const DWORD kDesiredAccess = SYNCHRONIZE | EVENT_MODIFY_STATE;

  // The events are auto reset, and start not signaled.
  server_ping->Set(::CreateEventW(nullptr, false, false, nullptr));
  if (!::DuplicateHandle(::GetCurrentProcess(), server_ping->get(),
                         target_process_, client_ping, kDesiredAccess, false,
                         0)) {
    return false;
  }

  server_pong->Set(::CreateEventW(nullptr, false, false, nullptr));
  if (!::DuplicateHandle(::GetCurrentProcess(), server_pong->get(),
                         target_process_, client_pong, kDesiredAccess, false,
                         0)) {
    return false;
  }
  return true;
}

}  // namespace sandbox
