// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/audio_helper.h"

#include <memory>

#include "base/check.h"
#include "content/common/pepper_file_util.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"

using ppapi::TrackedCallback;

namespace content {

// AudioHelper -----------------------------------------------------------------

AudioHelper::AudioHelper() {}

AudioHelper::~AudioHelper() {}

int32_t AudioHelper::GetSyncSocketImpl(int* sync_socket) {
  if (socket_for_create_callback_) {
    *sync_socket = IntegerFromSyncSocketHandle(
        socket_for_create_callback_->handle());
    return PP_OK;
  }
  return PP_ERROR_FAILED;
}

int32_t AudioHelper::GetSharedMemoryImpl(base::UnsafeSharedMemoryRegion** shm) {
  if (shared_memory_for_create_callback_.IsValid()) {
    *shm = &shared_memory_for_create_callback_;
    return PP_OK;
  }
  return PP_ERROR_FAILED;
}

void AudioHelper::StreamCreated(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::SyncSocket::ScopedHandle socket_handle) {
  if (TrackedCallback::IsPending(create_callback_)) {
    // Trusted side of proxy can specify a callback to receive handles. In
    // this case we don't need to map any data or start the thread since it
    // will be handled by the proxy.
    shared_memory_for_create_callback_ = std::move(shared_memory_region);
    socket_for_create_callback_ =
        std::make_unique<base::SyncSocket>(std::move(socket_handle));

    create_callback_->Run(PP_OK);

    // It might be nice to close the handles here to free up some system
    // resources, but we can't since there's a race condition. The handles must
    // be valid until they're sent over IPC, which is done from the I/O thread
    // which will often get done after this code executes. We could do
    // something more elaborate like an ACK from the plugin or post a task to
    // the I/O thread and back, but this extra complexity doesn't seem worth it
    // just to clean up these handles faster.
  } else {
    OnSetStreamInfo(std::move(shared_memory_region), std::move(socket_handle));
  }
}

void AudioHelper::SetCreateCallback(
    scoped_refptr<ppapi::TrackedCallback> create_callback) {
  DCHECK(!TrackedCallback::IsPending(create_callback_));
  create_callback_ = create_callback;
}

}  // namespace content
