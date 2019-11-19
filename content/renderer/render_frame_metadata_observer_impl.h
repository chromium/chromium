// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_METADATA_OBSERVER_IMPL_H_
#define CONTENT_RENDERER_RENDER_FRAME_METADATA_OBSERVER_IMPL_H_

#include "build/build_config.h"
#include "cc/trees/render_frame_metadata.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "content/common/content_export.h"
#include "content/common/render_frame_metadata.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// Implementation of cc::RenderFrameMetadataObserver which exists in the
// renderer an observers frame submission. It then notifies the
// mojom::RenderFrameMetadataObserverClient, which is expected to be in the
// browser process, of the metadata associated with the frame.
//
// BindToCurrentThread should be called from the Compositor thread so that the
// Mojo pipe is properly bound.
//
// Subsequent usage should only be from the Compositor thread.
class CONTENT_EXPORT RenderFrameMetadataObserverImpl
    : public cc::RenderFrameMetadataObserver,
      public mojom::RenderFrameMetadataObserver {
 public:
  RenderFrameMetadataObserverImpl(
      mojo::PendingReceiver<mojom::RenderFrameMetadataObserver> receiver,
      mojo::PendingRemote<mojom::RenderFrameMetadataObserverClient>
          client_remote);
  ~RenderFrameMetadataObserverImpl() override;

  // cc::RenderFrameMetadataObserver:
  void BindToCurrentThread() override;
  void OnRenderFrameSubmission(
      const cc::RenderFrameMetadata& render_frame_metadata,
      viz::CompositorFrameMetadata* compositor_frame_metadata,
      bool force_send) override;

  // mojom::RenderFrameMetadataObserver:
#if defined(OS_ANDROID)
  void ReportAllRootScrollsForAccessibility(bool enabled) override;
#endif
  void ReportAllFrameSubmissionsForTesting(bool enabled) override;

 private:
  friend class RenderFrameMetadataObserverImplTest;

  // Certain fields should always have their changes reported. This will return
  // true when there is a difference between |rfm1| and |rfm2| for those fields.
  // These fields have a low frequency rate of change.
  // |needs_activation_notification| indicates whether the browser process
  // expects notification of activation of the assoicated CompositorFrame from
  // Viz.
  bool ShouldSendRenderFrameMetadata(const cc::RenderFrameMetadata& rfm1,
                                     const cc::RenderFrameMetadata& rfm2,
                                     bool* needs_activation_notification) const;

  void SendLastRenderFrameMetadata();

#if defined(OS_ANDROID)
  // When true this will notify |render_frame_metadata_observer_client_| of all
  // frame submissions that involve a root scroll offset change.
  bool report_all_root_scrolls_for_accessibility_enabled_ = false;
#endif

  // When true this will notify |render_frame_metadata_observer_client_| of all
  // frame submissions.
  bool report_all_frame_submissions_for_testing_enabled_ = false;

  uint32_t last_frame_token_ = 0;
  base::Optional<cc::RenderFrameMetadata> last_render_frame_metadata_;

  // These are destroyed when BindToCurrentThread() is called.
  mojo::PendingReceiver<mojom::RenderFrameMetadataObserver> receiver_;
  mojo::PendingRemote<mojom::RenderFrameMetadataObserverClient> client_remote_;

  mojo::Receiver<mojom::RenderFrameMetadataObserver>
      render_frame_metadata_observer_receiver_{this};
  mojo::Remote<mojom::RenderFrameMetadataObserverClient>
      render_frame_metadata_observer_client_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameMetadataObserverImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_METADATA_OBSERVER_IMPL_H_
