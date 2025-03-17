// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/frame_resource.h"

namespace media {

namespace {
FrameResource::ID GetNextID() {
  static std::atomic_uint64_t counter(1u);
  return FrameResource::ID::FromUnsafeValue(
      counter.fetch_add(1u, std::memory_order_relaxed));
}

}  // namespace

FrameResource::FrameResource() : unique_id_(GetNextID()) {}

VideoFrameResource* FrameResource::AsVideoFrameResource() {
  return nullptr;
}

const NativePixmapFrameResource* FrameResource::AsNativePixmapFrameResource()
    const {
  return nullptr;
}

FrameResource::ID FrameResource::unique_id() const {
  return unique_id_;
}
}  // namespace media
