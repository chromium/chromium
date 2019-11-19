// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sharedmem_ipc_client.h"

#include <stddef.h>
#include <string.h>

#include "base/logging.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/crosscall_params.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_types.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace sandbox {

SANDBOX_INTERCEPT NtExports g_nt;

namespace {

DWORD SignalObjectAndWaitWrapper(HANDLE object_to_signal,
                                 HANDLE object_to_wait_on,
                                 DWORD millis,
                                 BOOL alertable) {
  // Not running in a sandboxed process so can call directly.
  if (!g_nt.SignalAndWaitForSingleObject)
    return SignalObjectAndWait(object_to_signal, object_to_wait_on, millis,
                               alertable);
  // Don't support alertable.
  CHECK_NT(!alertable);
  LARGE_INTEGER timeout;
  timeout.QuadPart = millis * -10000LL;
  NTSTATUS status = g_nt.SignalAndWaitForSingleObject(
      object_to_signal, object_to_wait_on, alertable,
      millis == INFINITE ? nullptr : &timeout);
  if (!NT_SUCCESS(status))
    return WAIT_FAILED;
  return status;
}

DWORD WaitForSingleObjectWrapper(HANDLE handle, DWORD millis) {
  // Not running in a sandboxed process so can call directly.
  if (!g_nt.WaitForSingleObject)
    return WaitForSingleObject(handle, millis);
  LARGE_INTEGER timeout;
  timeout.QuadPart = millis * -10000LL;
  NTSTATUS status = g_nt.WaitForSingleObject(
      handle, FALSE, millis == INFINITE ? nullptr : &timeout);
  if (!NT_SUCCESS(status))
    return WAIT_FAILED;
  return status;
}

}  // namespace

// Get the base of the data buffer of the channel; this is where the input
// parameters get serialized. Since they get serialized directly into the
// channel we avoid one copy.
void* SharedMemIPCClient::GetBuffer() {
  bool failure = false;
  size_t ix = LockFreeChannel(&failure);
  if (failure)
    return nullptr;
  return reinterpret_cast<char*>(control_) +
         control_->channels[ix].channel_base;
}

// If we need to cancel an IPC before issuing DoCall
// our client should call FreeBuffer with the same pointer
// returned by GetBuffer.
void SharedMemIPCClient::FreeBuffer(void* buffer) {
  size_t num = ChannelIndexFromBuffer(buffer);
  ChannelControl* channel = control_->channels;
  LONG result = ::InterlockedExchange(&channel[num].state, kFreeChannel);
  DCHECK_NE(kFreeChannel, static_cast<ChannelState>(result));
}

// The constructor simply casts the shared memory to the internal
// structures. This is a cheap step that is why this IPC object can
// and should be constructed per call.
SharedMemIPCClient::SharedMemIPCClient(void* shared_mem)
    : control_(reinterpret_cast<IPCControl*>(shared_mem)) {
  first_base_ =
      reinterpret_cast<char*>(shared_mem) + control_->channels[0].channel_base;
  // There must be at least one channel.
  DCHECK(0 != control_->channels_count);
}

// Do the IPC. At this point the channel should have already been
// filled with the serialized input parameters.
// We follow the pattern explained in the header file.
ResultCode SharedMemIPCClient::DoCall(CrossCallParams* params,
                                      CrossCallReturn* answer) {
  if (!control_->server_alive)
    return SBOX_ERROR_CHANNEL_ERROR;

  size_t num = ChannelIndexFromBuffer(params->GetBuffer());
  ChannelControl* channel = control_->channels;
  // Note that the IPC tag goes outside the buffer as well inside
  // the buffer. This should enable the server to prioritize based on
  // IPC tags without having to de-serialize the entire message.
  channel[num].ipc_tag = params->GetTag();

  // Wait for the server to service this IPC call. After kIPCWaitTimeOut1
  // we check if the server_alive mutex was abandoned which will indicate
  // that the server has died.

  // While the atomic signaling and waiting is not a requirement, it
  // is nice because we save a trip to kernel.
  DWORD wait = SignalObjectAndWaitWrapper(channel[num].ping_event,
                                          channel[num].pong_event,
                                          kIPCWaitTimeOut1, false);
  if (WAIT_TIMEOUT == wait) {
    // The server is taking too long. Enter a loop were we check if the
    // server_alive mutex has been abandoned which would signal a server crash
    // or else we keep waiting for a response.
    while (true) {
      wait = WaitForSingleObjectWrapper(control_->server_alive, 0);
      if (WAIT_TIMEOUT == wait) {
        // Server seems still alive. We already signaled so here we just wait.
        wait = WaitForSingleObjectWrapper(channel[num].pong_event,
                                          kIPCWaitTimeOut1);
        if (WAIT_OBJECT_0 == wait) {
          // The server took a long time but responded.
          break;
        } else if (WAIT_TIMEOUT == wait) {
          continue;
        } else {
          return SBOX_ERROR_CHANNEL_ERROR;
        }
      } else {
        // The server has crashed and windows has signaled the mutex as
        // abandoned.
        ::InterlockedExchange(&channel[num].state, kAbandonedChannel);
        control_->server_alive = 0;
        return SBOX_ERROR_CHANNEL_ERROR;
      }
    }
  } else if (WAIT_OBJECT_0 != wait) {
    // Probably the server crashed before the kIPCWaitTimeOut1 occurred.
    return SBOX_ERROR_CHANNEL_ERROR;
  }

  // The server has returned an answer, copy it and free the channel.
  memcpy_wrapper(answer, params->GetCallReturn(), sizeof(CrossCallReturn));

  // Return the IPC state It can indicate that while the IPC has
  // completed some error in the Broker has caused to not return valid
  // results.
  return answer->call_outcome;
}

// Locking a channel is a simple as looping over all the channels
// looking for one that is has state = kFreeChannel and atomically
// swapping it to kBusyChannel.
// If there is no free channel, then we must back off so some other
// thread makes progress and frees a channel. To back off we sleep.
size_t SharedMemIPCClient::LockFreeChannel(bool* severe_failure) {
  if (0 == control_->channels_count) {
    *severe_failure = true;
    return 0;
  }
  ChannelControl* channel = control_->channels;
  do {
    for (size_t ix = 0; ix != control_->channels_count; ++ix) {
      if (kFreeChannel == ::InterlockedCompareExchange(
                              &channel[ix].state, kBusyChannel, kFreeChannel)) {
        *severe_failure = false;
        return ix;
      }
    }
    // We did not find any available channel, maybe the server is dead.
    DWORD wait =
        WaitForSingleObjectWrapper(control_->server_alive, kIPCWaitTimeOut2);
    if (WAIT_TIMEOUT != wait) {
      // The server is dead and we outlive it enough to get in trouble.
      *severe_failure = true;
      return 0;
    }
  } while (true);
}

// Find out which channel we are from the pointer returned by GetBuffer.
size_t SharedMemIPCClient::ChannelIndexFromBuffer(const void* buffer) {
  ptrdiff_t d = reinterpret_cast<const char*>(buffer) - first_base_;
  size_t num = d / kIPCChannelSize;
  DCHECK_LT(num, control_->channels_count);
  return (num);
}

}  // namespace sandbox
