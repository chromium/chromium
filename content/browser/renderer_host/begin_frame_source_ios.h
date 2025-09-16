// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BEGIN_FRAME_SOURCE_IOS_H_
#define CONTENT_BROWSER_RENDERER_HOST_BEGIN_FRAME_SOURCE_IOS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/external_begin_frame_source_ios.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "ui/compositor/compositor.h"

namespace content {

class BeginFrameSourceIOS
    : public viz::BeginFrameObserver,
      public ui::ExternalBeginFrameControllerClientFactory,
      public viz::mojom::ExternalBeginFrameControllerClient {
 public:
  explicit BeginFrameSourceIOS(ui::Compositor* compositor);
  ~BeginFrameSourceIOS() override;

  // BeginFrameObserver implementation.
  void OnBeginFrame(const viz::BeginFrameArgs& args) override;
  const viz::BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool WantsAnimateOnlyBeginFrames() const override;

  // ui::ExternalBeginFrameControllerClientFactory implementation.
  mojo::PendingAssociatedRemote<viz::mojom::ExternalBeginFrameControllerClient>
  CreateExternalBeginFrameControllerClient() override;

  // viz::mojom::ExternalBeginFrameControllerClient implementation.
  void SetNeedsBeginFrame(bool needs_begin_frames) override;
  void SetPreferredInterval(base::TimeDelta interval) override;

 private:
  const raw_ptr<ui::Compositor> compositor_;

  viz::ExternalBeginFrameSourceIOS begin_frame_source_;

  viz::BeginFrameArgs last_used_begin_frame_args_;
  bool observing_begin_frame_source_ = false;

  mojo::AssociatedReceiverSet<viz::mojom::ExternalBeginFrameControllerClient>
      receivers_;

  base::WeakPtrFactory<BeginFrameSourceIOS> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BEGIN_FRAME_SOURCE_IOS_H_
