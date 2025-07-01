// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/gamepad_resource.h"

#include <string.h>

#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/threading/platform_thread.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppb_gamepad_shared.h"

namespace ppapi {
namespace proxy {

GamepadResource::GamepadResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance),
      buffer_(NULL) {
  memset(&last_read_, 0, sizeof(last_read_));

  SendCreate(BROWSER, PpapiHostMsg_Gamepad_Create());
  Call<PpapiPluginMsg_Gamepad_SendMemory>(
      BROWSER, PpapiHostMsg_Gamepad_RequestMemory(),
      base::BindOnce(&GamepadResource::OnPluginMsgSendMemory, this));
}

GamepadResource::~GamepadResource() {
}

thunk::PPB_Gamepad_API* GamepadResource::AsPPB_Gamepad_API() {
  return this;
}

void GamepadResource::Sample(PP_Instance /* instance */,
                             PP_GamepadsSampleData* data) {
  if (!buffer_) {
    // Browser hasn't sent back our shared memory, give the plugin gamepad
    // data corresponding to "not connected".
    memset(data, 0, sizeof(PP_GamepadsSampleData));
    return;
  }

  // ==========
  //   DANGER
  // ==========
  //
  // This logic is duplicated in the renderer as well. If you change it, that
  // also needs to be in sync. See gamepad_shared_memory_reader.cc.

  // Only try to read this many times before failing to avoid waiting here
  // very long in case of contention with the writer.
  const int kMaximumContentionCount = 10;
  int contention_count = -1;
  base::subtle::Atomic32 version;
  device::Gamepads read_into;
  do {
    version = buffer_->seqlock.ReadBegin();
    memcpy(&read_into, &buffer_->data, sizeof(read_into));
    ++contention_count;
    if (contention_count == kMaximumContentionCount)
      break;
  } while (buffer_->seqlock.ReadRetry(version));

  // In the event of a read failure, just leave the last read data as-is (the
  // hardware thread is taking unusally long).
  if (contention_count < kMaximumContentionCount)
    ConvertDeviceGamepadData(read_into, &last_read_);

  memcpy(data, &last_read_, sizeof(PP_GamepadsSampleData));
}

void GamepadResource::OnPluginMsgSendMemory(
    const ResourceMessageReplyParams& params) {
  // On failure, the handle will be null and the CHECK below will be tripped.
  base::ReadOnlySharedMemoryRegion region;
  params.TakeReadOnlySharedMemoryRegionAtIndex(0, &region);

  shared_memory_mapping_ = region.Map();
  CHECK(shared_memory_mapping_.IsValid());
  buffer_ = static_cast<const device::GamepadHardwareBuffer*>(
      shared_memory_mapping_.memory());
}

}  // namespace proxy
}  // namespace ppapi
