// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_METADATA_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_METADATA_PROVIDER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/mojom/render_frame_metadata.mojom.h"
#include "content/common/content_export.h"
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
      public cc::mojom::RenderFrameMetadataObserverClient {
 public:
  RenderFrameMetadataProviderImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      FrameTokenMessageQueue* frame_token_message_queue);

  RenderFrameMetadataProviderImpl(const RenderFrameMetadataProviderImpl&) =
      delete;
  RenderFrameMetadataProviderImpl& operator=(
      const RenderFrameMetadataProviderImpl&) = delete;

  ~RenderFrameMetadataProviderImpl() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void Bind(
      mojo::PendingReceiver<cc::mojom::RenderFrameMetadataObserverClient>
          client_receiver,
      mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserver> observer);

  const cc::RenderFrameMetadata& LastRenderFrameMetadata() override;

#if BUILDFLAG(IS_ANDROID)
  // Notifies the renderer of the changes in the notification frequency of the
  // root scroll updates, which is needed for accessibility and
  // GestureListenerManager on Android.
  void UpdateRootScrollOffsetUpdateFrequency(
      cc::mojom::RootScrollOffsetUpdateFrequency frequency);
#endif

  // Notifies the renderer to begin sending a notification on all frame
  // submissions.
  void ReportAllFrameSubmissionsForTesting(bool enabled);

  // Set |last_render_frame_metadata_| to the given |metadata| for testing
  // purpose.
  void SetLastRenderFrameMetadataForTest(cc::RenderFrameMetadata metadata);

 private:
  friend class FakeRenderWidgetHostViewAura;
  friend class DelegatedInkPointTest;
  friend class RenderWidgetHostViewAndroidTest;

  // Paired with the mojom::RenderFrameMetadataObserverClient overrides, these
  // methods are enqueued in |frame_token_message_queue_|. They are invoked when
  // the browser process receives their associated frame tokens. These then
  // notify any |observers_|.
  void OnRenderFrameMetadataChangedAfterActivation(
      cc::RenderFrameMetadata metadata,
      base::TimeTicks activation_time);
  void OnFrameTokenFrameSubmissionForTesting(base::TimeTicks activation_time);

  // cc::mojom::RenderFrameMetadataObserverClient:
  void OnRenderFrameMetadataChanged(
      uint32_t frame_token,
      const cc::RenderFrameMetadata& metadata) override;
  void OnFrameSubmissionForTesting(uint32_t frame_token) override;
#if BUILDFLAG(IS_ANDROID)
  void OnRootScrollOffsetChanged(
      const gfx::PointF& root_scroll_offset) override;
#endif

  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observers_;

  cc::RenderFrameMetadata last_render_frame_metadata_;

  std::optional<viz::LocalSurfaceId> last_local_surface_id_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Not owned.
  const raw_ptr<FrameTokenMessageQueue> frame_token_message_queue_;

  mojo::Receiver<cc::mojom::RenderFrameMetadataObserverClient>
      render_frame_metadata_observer_client_receiver_{this};
  mojo::Remote<cc::mojom::RenderFrameMetadataObserver>
      render_frame_metadata_observer_remote_;

#if BUILDFLAG(IS_ANDROID)
  std::optional<cc::mojom::RootScrollOffsetUpdateFrequency>
      pending_root_scroll_offset_update_frequency_;
#endif
  std::optional<bool> pending_report_all_frame_submission_for_testing_;

  base::WeakPtrFactory<RenderFrameMetadataProviderImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_METADATA_PROVIDER_IMPL_H_
