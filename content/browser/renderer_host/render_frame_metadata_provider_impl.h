// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_METADATA_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_METADATA_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/common/render_frame_metadata.mojom.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class FrameTokenMessageQueue;

// Observes RenderFrameMetadata associated with the submission of a frame for a
// given RenderWidgetHost. The renderer will notify this when sumitting a
// CompositorFrame.
//
// When ReportAllFrameSubmissionsForTesting(true) is called, this will be
// notified of all frame submissions.
//
// All RenderFrameMetadataProvider::Observer will be notified.
class CONTENT_EXPORT RenderFrameMetadataProviderImpl
    : public RenderFrameMetadataProvider,
      public mojom::RenderFrameMetadataObserverClient {
 public:
  RenderFrameMetadataProviderImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      FrameTokenMessageQueue* frame_token_message_queue);
  ~RenderFrameMetadataProviderImpl() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void Bind(mojo::PendingReceiver<mojom::RenderFrameMetadataObserverClient>
                client_receiver,
            mojo::PendingRemote<mojom::RenderFrameMetadataObserver> observer);

  const cc::RenderFrameMetadata& LastRenderFrameMetadata() override;

#if defined(OS_ANDROID)
  // Notifies the renderer to begin sending a notification on all root scroll
  // changes, which is needed for accessibility on Android.
  void ReportAllRootScrollsForAccessibility(bool enabled);
#endif

  // Notifies the renderer to begin sending a notification on all frame
  // submissions.
  void ReportAllFrameSubmissionsForTesting(bool enabled);

 private:
  friend class FakeRenderWidgetHostViewAura;

  // Paired with the mojom::RenderFrameMetadataObserverClient overrides, these
  // methods are enqueued in |frame_token_message_queue_|. They are invoked when
  // the browser process receives their associated frame tokens. These then
  // notify any |observers_|.
  void OnRenderFrameMetadataChangedAfterActivation(
      cc::RenderFrameMetadata metadata);
  void OnFrameTokenFrameSubmissionForTesting();

  // Set |last_render_frame_metadata_| to the given |metadata| for testing
  // purpose.
  void SetLastRenderFrameMetadataForTest(cc::RenderFrameMetadata metadata);

  // mojom::RenderFrameMetadataObserverClient:
  void OnRenderFrameMetadataChanged(
      uint32_t frame_token,
      const cc::RenderFrameMetadata& metadata) override;
  void OnFrameSubmissionForTesting(uint32_t frame_token) override;

  base::ObserverList<Observer>::Unchecked observers_;

  cc::RenderFrameMetadata last_render_frame_metadata_;

  base::Optional<viz::LocalSurfaceIdAllocation>
      last_local_surface_id_allocation_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Not owned.
  FrameTokenMessageQueue* const frame_token_message_queue_;

  mojo::Receiver<mojom::RenderFrameMetadataObserverClient>
      render_frame_metadata_observer_client_receiver_{this};
  mojo::Remote<mojom::RenderFrameMetadataObserver>
      render_frame_metadata_observer_remote_;

#if defined(OS_ANDROID)
  base::Optional<bool> pending_report_all_root_scrolls_for_accessibility_;
#endif
  base::Optional<bool> pending_report_all_frame_submission_for_testing_;

  base::WeakPtrFactory<RenderFrameMetadataProviderImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderFrameMetadataProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_METADATA_PROVIDER_IMPL_H_
