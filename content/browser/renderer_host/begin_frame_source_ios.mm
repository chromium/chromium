// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/begin_frame_source_ios.h"

#include "base/functional/callback_helpers.h"

namespace content {

////////////////////////////////////////////////////////////////////////////////
// BeginFrameSourceIOS

BeginFrameSourceIOS::BeginFrameSourceIOS(ui::Compositor* compositor)
    : compositor_(compositor),
      begin_frame_source_(viz::BackToBackBeginFrameSource::kNotRestartableId) {
  DCHECK(compositor_);
  compositor_->SetExternalBeginFrameControllerClientFactory(this);
}

BeginFrameSourceIOS::~BeginFrameSourceIOS() {
  SetNeedsBeginFrame(false);
}

void BeginFrameSourceIOS::OnBeginFrame(const viz::BeginFrameArgs& args) {
  last_used_begin_frame_args_ = args;
  compositor_->IssueExternalBeginFrameNoAck(args);
}

const viz::BeginFrameArgs& BeginFrameSourceIOS::LastUsedBeginFrameArgs() const {
  return last_used_begin_frame_args_;
}

void BeginFrameSourceIOS::OnBeginFrameSourcePausedChanged(bool paused) {}

bool BeginFrameSourceIOS::WantsAnimateOnlyBeginFrames() const {
  return false;
}

mojo::PendingAssociatedRemote<viz::mojom::ExternalBeginFrameControllerClient>
BeginFrameSourceIOS::CreateExternalBeginFrameControllerClient() {
  mojo::PendingAssociatedRemote<viz::mojom::ExternalBeginFrameControllerClient>
      remote;
  receivers_.Add(this, remote.InitWithNewEndpointAndPassReceiver());
  return remote;
}

void BeginFrameSourceIOS::SetNeedsBeginFrame(bool needs_begin_frames) {
  if (needs_begin_frames == observing_begin_frame_source_) {
    return;
  }
  observing_begin_frame_source_ = needs_begin_frames;
  if (needs_begin_frames) {
    begin_frame_source_.AddObserver(this);
  } else {
    begin_frame_source_.RemoveObserver(this);
  }
}

void BeginFrameSourceIOS::SetPreferredInterval(base::TimeDelta interval) {
  begin_frame_source_.SetPreferredInterval(interval);
}

}  // namespace content
