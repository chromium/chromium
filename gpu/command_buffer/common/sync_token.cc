// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/sync_token.h"

#include <sstream>

namespace gpu {

SyncToken::SyncToken()
    : verified_flush_(false),
      namespace_id_(CommandBufferNamespace::INVALID),
      release_count_(0) {}

SyncToken::SyncToken(CommandBufferNamespace namespace_id,
                     CommandBufferId command_buffer_id,
                     uint64_t release_count)
    : verified_flush_(false),
      namespace_id_(namespace_id),
      command_buffer_id_(command_buffer_id),
      release_count_(release_count) {}

SyncToken::SyncToken(const SyncToken& other) = default;
SyncToken& SyncToken::operator=(const SyncToken& other) = default;

std::string SyncToken::ToDebugString() const {
  // At the level of the generic command buffer code, the command buffer ID is
  // an arbitrary 64-bit number. For the debug output, print its upper and lower
  // 32bit words separately. This ensures more readable output for IDs allocated
  // by gpu/ipc code which uses these words for channel and route IDs, but it's
  // still a lossless representation if the IDs don't match this convention.
  uint64_t id = command_buffer_id().GetUnsafeValue();
  uint32_t channel_or_high = 0xffffffff & id;
  uint32_t route_or_low = id >> 32;
  std::ostringstream stream;
  stream << static_cast<int>(namespace_id()) << ":" << channel_or_high << ":"
         << route_or_low << ":" << release_count();
  return stream.str();
}

}  // namespace gpu
