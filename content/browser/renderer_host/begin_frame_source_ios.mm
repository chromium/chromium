// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/begin_frame_source_ios.h"

namespace content {

////////////////////////////////////////////////////////////////////////////////
// BeginFrameSourceIOS

BeginFrameSourceIOS::BeginFrameSourceIOS(ui::Compositor* compositor)
    : compositor_(compositor),
      begin_frame_source_(viz::BackToBackBeginFrameSource::kNotRestartableId) {
  compositor_->SetExternalBeginFrameControllerClientFactory(this);
}

BeginFrameSourceIOS::~BeginFrameSourceIOS() {
  SetNeedsBeginFrame(false);
}

void BeginFrameSourceIOS::OnBeginFrame(const viz::BeginFrameArgs& args) {
  if (!compositor_ || !send_begin_frame_) {
    return;
  }
  last_used_begin_frame_args_ = args;
  send_begin_frame_ = false;
  compositor_->IssueExternalBeginFrame(
      args, /*force=*/true,
      base::BindOnce(&BeginFrameSourceIOS::BeginFrameAck,
                     weak_factory_.GetWeakPtr()));
}

void BeginFrameSourceIOS::BeginFrameAck(const viz::BeginFrameAck&) {
  send_begin_frame_ = true;
}

const viz::BeginFrameArgs& BeginFrameSourceIOS::LastUsedBeginFrameArgs() const {
  return last_used_begin_frame_args_;
}

void BeginFrameSourceIOS::OnBeginFrameSourcePausedChanged(bool paused) {}

bool BeginFrameSourceIOS::WantsAnimateOnlyBeginFrames() const {
  return false;
}

bool BeginFrameSourceIOS::IsRoot() const {
  return true;
}

mojo::PendingAssociatedRemote<viz::mojom::ExternalBeginFrameControllerClient>
BeginFrameSourceIOS::CreateExternalBeginFrameControllerClient() {
  mojo::PendingAssociatedRemote<viz::mojom::ExternalBeginFrameControllerClient>
      remote;
  receivers_.Add(this, remote.InitWithNewEndpointAndPassReceiver());
  return remote;
}

void BeginFrameSourceIOS::SetNeedsBeginFrame(bool needs_begin_frames) {
  if (needs_begin_frames) {
    if (added_observer_) {
      return;
    }
    added_observer_ = true;
    begin_frame_source_.AddObserver(this);
  } else {
    if (!added_observer_) {
      return;
    }
    added_observer_ = false;
    begin_frame_source_.RemoveObserver(this);
  }
}

void BeginFrameSourceIOS::SetPreferredInterval(base::TimeDelta interval) {
  begin_frame_source_.SetPreferredInterval(interval);
}

}  // namespace content
