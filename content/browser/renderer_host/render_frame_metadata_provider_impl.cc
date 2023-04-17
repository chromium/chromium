// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_metadata_provider_impl.h"

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"

namespace content {

RenderFrameMetadataProviderImpl::RenderFrameMetadataProviderImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    FrameTokenMessageQueue* frame_token_message_queue)
    : task_runner_(task_runner),
      frame_token_message_queue_(frame_token_message_queue) {}

RenderFrameMetadataProviderImpl::~RenderFrameMetadataProviderImpl() = default;

void RenderFrameMetadataProviderImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void RenderFrameMetadataProviderImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RenderFrameMetadataProviderImpl::Bind(
    mojo::PendingReceiver<cc::mojom::RenderFrameMetadataObserverClient>
        client_receiver,
    mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserver> observer) {
  render_frame_metadata_observer_remote_.reset();
  render_frame_metadata_observer_remote_.Bind(std::move(observer));
  render_frame_metadata_observer_client_receiver_.reset();
  render_frame_metadata_observer_client_receiver_.Bind(
      std::move(client_receiver), task_runner_);

  // Reset on disconnect so that pending state will be correctly stored and
  // later forwarded in the case of a renderer crash.
  render_frame_metadata_observer_remote_.reset_on_disconnect();
#if BUILDFLAG(IS_ANDROID)
  if (pending_root_scroll_offset_update_frequency_.has_value()) {
    UpdateRootScrollOffsetUpdateFrequency(
        *pending_root_scroll_offset_update_frequency_);
    pending_root_scroll_offset_update_frequency_.reset();
  }
#endif
  if (pending_report_all_frame_submission_for_testing_.has_value()) {
    ReportAllFrameSubmissionsForTesting(
        *pending_report_all_frame_submission_for_testing_);
    pending_report_all_frame_submission_for_testing_.reset();
  }
}

#if BUILDFLAG(IS_ANDROID)
void RenderFrameMetadataProviderImpl::UpdateRootScrollOffsetUpdateFrequency(
    cc::mojom::RootScrollOffsetUpdateFrequency frequency) {
  if (!render_frame_metadata_observer_remote_) {
    pending_root_scroll_offset_update_frequency_ = frequency;
    return;
  }

  render_frame_metadata_observer_remote_->UpdateRootScrollOffsetUpdateFrequency(
      frequency);
}
#endif

void RenderFrameMetadataProviderImpl::ReportAllFrameSubmissionsForTesting(
    bool enabled) {
  if (!render_frame_metadata_observer_remote_) {
    pending_report_all_frame_submission_for_testing_ = enabled;
    return;
  }

  render_frame_metadata_observer_remote_->ReportAllFrameSubmissionsForTesting(
      enabled);
}

const cc::RenderFrameMetadata&
RenderFrameMetadataProviderImpl::LastRenderFrameMetadata() {
  return last_render_frame_metadata_;
}

void RenderFrameMetadataProviderImpl::
    OnRenderFrameMetadataChangedAfterActivation(
        cc::RenderFrameMetadata metadata,
        base::TimeTicks activation_time) {
  last_render_frame_metadata_ = std::move(metadata);
  for (Observer& observer : observers_)
    observer.OnRenderFrameMetadataChangedAfterActivation(activation_time);
}

void RenderFrameMetadataProviderImpl::OnFrameTokenFrameSubmissionForTesting(
    base::TimeTicks activation_time) {
  for (Observer& observer : observers_)
    observer.OnRenderFrameSubmission();
}

void RenderFrameMetadataProviderImpl::SetLastRenderFrameMetadataForTest(
    cc::RenderFrameMetadata metadata) {
  last_render_frame_metadata_ = metadata;
}

void RenderFrameMetadataProviderImpl::OnRenderFrameMetadataChanged(
    uint32_t frame_token,
    const cc::RenderFrameMetadata& metadata) {
  // Guard for this being recursively deleted from one of the observer
  // callbacks.
  base::WeakPtr<RenderFrameMetadataProviderImpl> self =
      weak_factory_.GetWeakPtr();

  for (Observer& observer : observers_) {
    observer.OnRenderFrameMetadataChangedBeforeActivation(metadata);
    if (!self) {
      return;
    }
  }

  if (metadata.local_surface_id != last_local_surface_id_) {
    last_local_surface_id_ = metadata.local_surface_id;
    for (Observer& observer : observers_) {
      observer.OnLocalSurfaceIdChanged(metadata);
      if (!self) {
        return;
      }
    }
  }

  if (!frame_token)
    return;

  // Both RenderFrameMetadataProviderImpl and FrameTokenMessageQueue are owned
  // by the same RenderWidgetHostImpl. During shutdown the queue is cleared
  // without running the callbacks.
  frame_token_message_queue_->EnqueueOrRunFrameTokenCallback(
      frame_token,
      base::BindOnce(&RenderFrameMetadataProviderImpl::
                         OnRenderFrameMetadataChangedAfterActivation,
                     weak_factory_.GetWeakPtr(), std::move(metadata)));
}

void RenderFrameMetadataProviderImpl::OnFrameSubmissionForTesting(
    uint32_t frame_token) {
  frame_token_message_queue_->EnqueueOrRunFrameTokenCallback(
      frame_token, base::BindOnce(&RenderFrameMetadataProviderImpl::
                                      OnFrameTokenFrameSubmissionForTesting,
                                  weak_factory_.GetWeakPtr()));
}

#if BUILDFLAG(IS_ANDROID)
void RenderFrameMetadataProviderImpl::OnRootScrollOffsetChanged(
    const gfx::PointF& root_scroll_offset) {
  for (Observer& observer : observers_)
    observer.OnRootScrollOffsetChanged(root_scroll_offset);
}
#endif

}  // namespace content
