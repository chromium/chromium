// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_VIDEO_FRAME_SUBMITTER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_VIDEO_FRAME_SUBMITTER_H_

#include "cc/layers/video_frame_provider.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "media/base/video_rotation.h"
#include "third_party/blink/public/platform/web_common.h"

namespace cc {
class LayerTreeSettings;
}

namespace viz {
class ContextProvider;
}  // namespace viz

namespace blink {

// Sets the proper context_provider and compositing mode onto the Submitter.
using WebSubmitterConfigurationCallback =
    base::OnceCallback<void(bool, scoped_refptr<viz::ContextProvider>)>;
// Callback to obtain the media ContextProvider and a bool indicating whether
// we are in software compositing mode.
using WebContextProviderCallback =
    base::RepeatingCallback<void(scoped_refptr<viz::ContextProvider>,
                                 WebSubmitterConfigurationCallback)>;
using WebFrameSinkDestroyedCallback = base::RepeatingCallback<void()>;

// Exposes the VideoFrameSubmitter, which submits CompositorFrames containing
// decoded VideoFrames from the VideoFrameProvider to the compositor for
// display.
class BLINK_PLATFORM_EXPORT WebVideoFrameSubmitter
    : public cc::VideoFrameProvider::Client {
 public:
  static std::unique_ptr<WebVideoFrameSubmitter> Create(
      WebContextProviderCallback,
      const cc::LayerTreeSettings&,
      bool use_sync_primitives);
  ~WebVideoFrameSubmitter() override = default;

  // Intialize must be called before submissions occur, pulled out of
  // StartSubmitting() to enable tests without the full mojo statck running.
  virtual void Initialize(cc::VideoFrameProvider*) = 0;

  // Set the rotation state of the video to be used while appending frames.
  virtual void SetRotation(media::VideoRotation) = 0;

  // Set if the video is opaque or not.
  virtual void SetIsOpaque(bool) = 0;

  // Prepares the compositor frame sink to accept frames by providing
  // a SurfaceId, with its associated allocation time. The callback is to be
  // used when on context loss to prevent the submitter from continuing to
  // submit frames with invalid resources.
  virtual void EnableSubmission(viz::SurfaceId,
                                base::TimeTicks,
                                WebFrameSinkDestroyedCallback) = 0;

  // Updates whether we should submit frames or not based on whether the video
  // is visible on screen.
  virtual void UpdateSubmissionState(bool) = 0;

  // Set whether frames should always be submitted regardless of visibility.
  virtual void SetForceSubmit(bool) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_VIDEO_FRAME_SUBMITTER_H_
