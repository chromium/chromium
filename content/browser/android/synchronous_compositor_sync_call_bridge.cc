// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_sync_call_bridge.h"

#include <memory>

#include "base/functional/bind.h"
#include "content/browser/android/synchronous_compositor_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "ui/android/window_android.h"

namespace content {

SynchronousCompositorSyncCallBridge::SynchronousCompositorSyncCallBridge(
    SynchronousCompositorHost* host)
    : host_(host), begin_frame_condition_(&lock_) {
  DCHECK(host);
}

SynchronousCompositorSyncCallBridge::~SynchronousCompositorSyncCallBridge() {
  DCHECK(frame_futures_.empty());
}

void SynchronousCompositorSyncCallBridge::RemoteReady() {
  base::AutoLock lock(lock_);
  if (remote_state_ != RemoteState::INIT)
    return;
  remote_state_ = RemoteState::READY;
}

void SynchronousCompositorSyncCallBridge::RemoteClosedOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::AutoLock lock(lock_);
  SignalRemoteClosedToAllWaitersOnIOThread();
}

bool SynchronousCompositorSyncCallBridge::ReceiveFrameOnIOThread(
    int layer_tree_frame_sink_id,
    uint32_t metadata_version,
    std::optional<viz::LocalSurfaceId> local_surface_id,
    std::optional<viz::CompositorFrame> compositor_frame,
    std::optional<viz::HitTestRegionList> hit_test_region_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::AutoLock lock(lock_);
  if (remote_state_ != RemoteState::READY || frame_futures_.empty())
    return false;
  auto frame_ptr = std::make_unique<SynchronousCompositor::Frame>();
  frame_ptr->layer_tree_frame_sink_id = layer_tree_frame_sink_id;
  scoped_refptr<SynchronousCompositor::FrameFuture> future =
      std::move(frame_futures_.front());
  DCHECK(future);
  frame_futures_.pop_front();

  if (compositor_frame) {
    if (!local_surface_id)
      return false;
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&SynchronousCompositorSyncCallBridge::
                                      ProcessFrameMetadataOnUIThread,
                                  this, metadata_version,
                                  compositor_frame->metadata.Clone(),
                                  local_surface_id.value()));
    frame_ptr->frame = std::make_unique<viz::CompositorFrame>();
    *frame_ptr->frame = std::move(*compositor_frame);
    frame_ptr->local_surface_id = local_surface_id.value();
    frame_ptr->hit_test_region_list = std::move(hit_test_region_list);
  }
  future->SetFrame(std::move(frame_ptr));
  return true;
}

bool SynchronousCompositorSyncCallBridge::BeginFrameResponseOnIOThread(
    blink::mojom::SyncCompositorCommonRendererParamsPtr render_params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::AutoLock lock(lock_);
  if (begin_frame_response_valid_)
    return false;
  begin_frame_response_valid_ = true;
  last_render_params_ = *render_params;
  begin_frame_condition_.Signal();
  return true;
}

bool SynchronousCompositorSyncCallBridge::WaitAfterVSyncOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock lock(lock_);
  if (remote_state_ != RemoteState::READY)
    return false;
  CHECK(!begin_frame_response_valid_);
  host_->AddBeginFrameCompletionCallback(base::BindOnce(
      &SynchronousCompositorSyncCallBridge::BeginFrameCompleteOnUIThread,
      this));
  return true;
}

bool SynchronousCompositorSyncCallBridge::SetFrameFutureOnUIThread(
    scoped_refptr<SynchronousCompositor::FrameFuture> frame_future) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(frame_future);
  base::AutoLock lock(lock_);
  if (remote_state_ != RemoteState::READY)
    return false;

  // Allowing arbitrary number of pending futures can lead to increase in frame
  // latency. Due to this, Android platform already ensures that here that there
  // can be at most 2 pending frames. Here, we rely on Android to do the
  // necessary blocking, which allows more parallelism without increasing
  // latency. But DCHECK Android blocking is working.
  DCHECK_LT(frame_futures_.size(), 2u);
  frame_futures_.emplace_back(std::move(frame_future));
  return true;
}

void SynchronousCompositorSyncCallBridge::HostDestroyedOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(host_);
  host_ = nullptr;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SynchronousCompositorSyncCallBridge::CloseHostControlOnIOThread,
          this));
}

bool SynchronousCompositorSyncCallBridge::IsRemoteReadyOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock lock(lock_);
  return remote_state_ == RemoteState::READY;
}

void SynchronousCompositorSyncCallBridge::BeginFrameCompleteOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  blink::mojom::SyncCompositorCommonRendererParamsPtr render_params;
  {
    base::AutoLock lock(lock_);
    if (remote_state_ != RemoteState::READY)
      return;

    // If we haven't received a response yet. Wait for it.
    if (!begin_frame_response_valid_) {
      base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
          allow_base_sync_primitives;
      while (!begin_frame_response_valid_ &&
             remote_state_ == RemoteState::READY) {
        begin_frame_condition_.Wait();
      }
    }
    DCHECK(begin_frame_response_valid_ || remote_state_ != RemoteState::READY);
    begin_frame_response_valid_ = false;
    if (remote_state_ == RemoteState::READY) {
      render_params = last_render_params_.Clone();
    }
  }
  if (render_params) {
    host_->BeginFrameComplete(std::move(render_params));
  }
}

void SynchronousCompositorSyncCallBridge::ProcessFrameMetadataOnUIThread(
    uint32_t metadata_version,
    viz::CompositorFrameMetadata metadata,
    const viz::LocalSurfaceId& local_surface_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (host_) {
    host_->UpdateFrameMetaData(metadata_version, std::move(metadata),
                               local_surface_id);
  }
}

void SynchronousCompositorSyncCallBridge::
    SignalRemoteClosedToAllWaitersOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  lock_.AssertAcquired();
  remote_state_ = RemoteState::CLOSED;
  for (auto& future_ptr : frame_futures_) {
    future_ptr->SetFrame(nullptr);
  }
  frame_futures_.clear();
  begin_frame_condition_.Signal();
}

void SynchronousCompositorSyncCallBridge::SetHostControlReceiverOnIOThread(
    mojo::SelfOwnedReceiverRef<blink::mojom::SynchronousCompositorControlHost>
        host_control_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  host_control_receiver_ = host_control_receiver;
}

void SynchronousCompositorSyncCallBridge::CloseHostControlOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (host_control_receiver_) {
    host_control_receiver_->Close();
    host_control_receiver_.reset();
  }
}

}  // namespace content
