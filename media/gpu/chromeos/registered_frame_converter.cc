// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/registered_frame_converter.h"

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/chromeos/frame_registry.h"
#include "media/gpu/macros.h"

namespace media {

// static
std::unique_ptr<FrameResourceConverter> RegisteredFrameConverter::Create(
    scoped_refptr<FrameRegistry> registry) {
  return base::WrapUnique<FrameResourceConverter>(
      new RegisteredFrameConverter(std::move(registry)));
}

RegisteredFrameConverter::RegisteredFrameConverter(
    scoped_refptr<FrameRegistry> registry)
    : registry_(std::move(registry)) {}

RegisteredFrameConverter::~RegisteredFrameConverter() = default;

void RegisteredFrameConverter::ConvertFrameImpl(
    scoped_refptr<FrameResource> frame) {
  DVLOGF(4);

  if (!frame) {
    return OnError(FROM_HERE, "Invalid frame.");
  }

  // Creates a VideoFrame that wraps |frame|'s identifying token.
  scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapTrackingToken(
      frame->format(), frame->tracking_token(), frame->coded_size(),
      frame->visible_rect(), frame->natural_size(), frame->timestamp());
  if (!video_frame) {
    return OnError(FROM_HERE, "Failed to create VideoFrame.");
  }

  // It is fine to use set_metadata() to replace `video_frame`'s metadata with
  // that of `frame`. The `tracking_token` field of each are the same.
  video_frame->set_metadata(frame->metadata());
  video_frame->set_color_space(frame->ColorSpace());
  video_frame->set_hdr_metadata(frame->hdr_metadata());

  // A reference to |frame| is stored in |registry_|. Next, a reference to
  // |registry_| is stored in the destruction observer of the generated
  // frame, |video_frame|. So, the local reference to |frame| can be safely
  // dropped at the end of this function.
  registry_->RegisterFrame(frame);
  video_frame->AddDestructionObserver(base::BindOnce(
      [](scoped_refptr<FrameRegistry> registry,
         const base::UnguessableToken& token) {
        registry->UnregisterFrame(token);
      },
      registry_, frame->tracking_token()));

  Output(std::move(video_frame));
}

}  // namespace media
