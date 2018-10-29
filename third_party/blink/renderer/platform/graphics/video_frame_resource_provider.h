// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_RESOURCE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_RESOURCE_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/client/shared_bitmap_reporter.h"
#include "third_party/blink/public/platform/web_video_frame_submitter.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace media {
class VideoFrame;
class VideoResourceUpdater;
}  // namespace media

namespace viz {
class RenderPass;
}

namespace blink {

// VideoFrameResourceProvider obtains required GPU resources for the video
// frame.
// VideoFrameResourceProvider methods are currently called on the media thread.
// TODO(lethalantidote): Move the usage of this class off media thread
// https://crbug.com/753605
class PLATFORM_EXPORT VideoFrameResourceProvider {
 public:
  // |use_sync_primitives| controls whether we ScopedAllowBaseSyncPrimitives
  // when calling into |resource_updater_|.  It waits, but the cc impl thread
  // doesn't seem to mind.  It does mind, however, the ScopedAllow.  When this
  // is run on the media thread, we need to ScopedAllow first.
  VideoFrameResourceProvider(const cc::LayerTreeSettings&,
                             bool use_sync_primitives);

  virtual ~VideoFrameResourceProvider();

  virtual void Initialize(viz::ContextProvider*, viz::SharedBitmapReporter*);
  virtual void AppendQuads(viz::RenderPass*,
                           scoped_refptr<media::VideoFrame>,
                           media::VideoRotation,
                           bool is_opaque);
  virtual void ReleaseFrameResources();

  // Once the context is lost, we must call Initialize again before we can
  // continue doing work.
  void OnContextLost();

  bool IsInitialized() { return resource_updater_.get(); }

  virtual void PrepareSendToParent(
      const std::vector<viz::ResourceId>& resource_ids,
      std::vector<viz::TransferableResource>* transferable_resources);
  virtual void ReceiveReturnsFromParent(
      const std::vector<viz::ReturnedResource>& transferable_resources);

 private:
  const cc::LayerTreeSettings settings_;

  viz::ContextProvider* context_provider_;
  std::unique_ptr<viz::ClientResourceProvider> resource_provider_;
  std::unique_ptr<media::VideoResourceUpdater> resource_updater_;
  bool use_sync_primitives_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_RESOURCE_PROVIDER_H_
