// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/sync_token.h"

#include <sstream>

#include "base/ranges/algorithm.h"

namespace gpu {

SyncPointClientId::SyncPointClientId(CommandBufferNamespace in_namespace_id,
                                     CommandBufferId in_command_buffer_id)
    : namespace_id(in_namespace_id), command_buffer_id(in_command_buffer_id) {}

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

std::vector<SyncToken> ReduceSyncTokens(base::span<const SyncToken> tokens) {
  std::vector<SyncToken> reduced;
  for (const SyncToken& next_token : tokens) {
    auto itr =
        base::ranges::find_if(reduced, [&next_token](const SyncToken& token) {
          return next_token.namespace_id() == token.namespace_id() &&
                 next_token.command_buffer_id() == token.command_buffer_id();
        });
    if (itr == reduced.end()) {
      reduced.push_back(next_token);
    } else {
      if (itr->release_count() < next_token.release_count()) {
        *itr = next_token;
      }
    }
  }
  return reduced;
}

}  // namespace gpu
