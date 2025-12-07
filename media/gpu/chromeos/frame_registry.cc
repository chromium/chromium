// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/frame_registry.h"

namespace media {

FrameRegistry::FrameRegistry() = default;
FrameRegistry::~FrameRegistry() = default;

void FrameRegistry::RegisterFrame(scoped_refptr<const FrameResource> frame) {
  CHECK(frame);
  CHECK(!frame->tracking_token().is_empty());
  base::AutoLock auto_lock(lock_);
  map_.emplace(frame->tracking_token(), std::move(frame));
}

void FrameRegistry::UnregisterFrame(const base::UnguessableToken& token) {
  base::AutoLock auto_lock(lock_);
  CHECK_EQ(map_.erase(token), 1u);
}

scoped_refptr<const FrameResource> FrameRegistry::AccessFrame(
    const base::UnguessableToken& token) const {
  base::AutoLock auto_lock(lock_);
  // Ensures that |token| exists in |map_|.
  auto it = map_.find(token);
  CHECK(it != map_.end());
  return it->second;
}

}  // namespace media
