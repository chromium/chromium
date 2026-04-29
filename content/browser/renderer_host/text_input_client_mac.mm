// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/text_input_client_mac.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/features.h"
#include "ui/base/mojom/attributed_string.mojom.h"

namespace content {

namespace {

class DefaultAsyncRequestDelegate final
    : public TextInputClientMac::AsyncRequestDelegate {
 public:
  DefaultAsyncRequestDelegate() = default;
  ~DefaultAsyncRequestDelegate() final = default;

  DefaultAsyncRequestDelegate(const DefaultAsyncRequestDelegate&) = delete;
  DefaultAsyncRequestDelegate& operator=(const DefaultAsyncRequestDelegate&) =
      delete;

  void GetCharacterIndexAtPoint(
      RenderFrameHost* rfh,
      const TextInputClientMac::RequestToken& request_token,
      const gfx::Point& point) final {
    RenderFrameHostImpl::From(rfh)
        ->GetAssociatedLocalFrame()
        ->GetCharacterIndexAtPoint(request_token.value(), point);
  }

  void GetFirstRectForRange(
      RenderFrameHost* rfh,
      const TextInputClientMac::RequestToken& request_token,
      const gfx::Range& range) final {
    RenderFrameHostImpl::From(rfh)
        ->GetAssociatedLocalFrame()
        ->GetFirstRectForRange(request_token.value(), range);
  }
};

RenderFrameHostImpl* GetFocusedRenderFrameHostImpl(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
  FrameTree* tree = rwhi->frame_tree();
  FrameTreeNode* focused_node = tree->GetFocusedFrame();
  return focused_node ? focused_node->current_frame_host() : nullptr;
}

base::WeakPtr<RenderFrameHostImpl> GetWeakFocusedRenderFrameHostImpl(
    RenderWidgetHost* widget) {
  if (RenderFrameHostImpl* rhfi = GetFocusedRenderFrameHostImpl(widget)) {
    return rhfi->GetWeakPtr();
  }
  return nullptr;
}

}  // namespace

TextInputClientMac::TextInputClientMac()
    : condition_(&lock_),
      async_request_delegate_(std::make_unique<DefaultAsyncRequestDelegate>()) {
}

TextInputClientMac::~TextInputClientMac() = default;

// static
TextInputClientMac* TextInputClientMac::GetInstance() {
  static base::NoDestructor<TextInputClientMac> client;
  return client.get();
}

void TextInputClientMac::GetStringAtPoint(RenderWidgetHost* rwh,
                                          const gfx::Point& point,
                                          GetStringCallback callback) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(rwh);
  if (rwhi && rwhi->GetAssociatedFrameWidget()) {
    rwhi->GetAssociatedFrameWidget()->GetStringAtPoint(point,
                                                       std::move(callback));
  } else {
    std::move(callback).Run(nullptr, gfx::Point());
  }
}

void TextInputClientMac::GetStringFromRange(RenderWidgetHost* rwh,
                                            const gfx::Range& range,
                                            GetStringCallback callback) {
  RenderFrameHostImpl* rfhi = GetFocusedRenderFrameHostImpl(rwh);
  // If it doesn't have a focused frame, it calls |callback| with an empty
  // string and point.
  if (!rfhi) {
    return std::move(callback).Run(nullptr, gfx::Point());
  }

  rfhi->GetAssociatedLocalFrame()->GetStringForRange(range,
                                                     std::move(callback));
}

uint32_t TextInputClientMac::GetCharacterIndexAtPoint(RenderWidgetHost* rwh,
                                                      const gfx::Point& point) {
  return GetCharacterIndexAtPoint(GetWeakFocusedRenderFrameHostImpl(rwh),
                                  point);
}

uint32_t TextInputClientMac::GetCharacterIndexAtPoint(
    base::WeakPtr<RenderFrameHostImpl> rfhi,
    const gfx::Point& point) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If it doesn't have a focused frame, it calls SetCharacterIndexAndSignal()
  // with index 0.
  if (!rfhi) {
    return 0;
  }

  base::UmaHistogramBoolean("TextInputClient.InSyncRequest.CharacterIndex",
                            in_sync_request_);
  if (in_sync_request_) {
    return 0;
  }

  base::LiveTicks start = base::LiveTicks::Now();
  base::TimeDelta wait_timeout = features::kTextInputClientIPCTimeout.Get();

  BeforeRequest();
  async_request_delegate_->GetCharacterIndexAtPoint(
      rfhi.get(), current_request_.value(), point);

  base::TimeDelta remaining_timeout = wait_timeout;
  while (!character_index_ && remaining_timeout.is_positive()) {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    condition_.TimedWait(remaining_timeout);
    remaining_timeout = start + wait_timeout - base::LiveTicks::Now();
  }
  base::UmaHistogramBoolean("TextInputClient.CharacterIndex.TimedOut",
                            !character_index_.has_value());

  // Return a sentinel if no response was received.
  uint32_t index = character_index_.value_or(UINT32_MAX);
  AfterRequest();

  base::UmaHistogramLongTimes("TextInputClient.CharacterIndex2",
                              base::LiveTicks::Now() - start);

  return index;
}

gfx::Rect TextInputClientMac::GetFirstRectForRange(RenderWidgetHost* rwh,
                                                   const gfx::Range& range) {
  return GetFirstRectForRange(GetWeakFocusedRenderFrameHostImpl(rwh), range);
}

gfx::Rect TextInputClientMac::GetFirstRectForRange(
    base::WeakPtr<RenderFrameHostImpl> rfhi,
    const gfx::Range& range) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!rfhi) {
    return gfx::Rect();
  }

  base::UmaHistogramBoolean("TextInputClient.InSyncRequest.FirstRect",
                            in_sync_request_);
  if (in_sync_request_) {
    return gfx::Rect();
  }

  base::LiveTicks start = base::LiveTicks::Now();
  base::TimeDelta wait_timeout = features::kTextInputClientIPCTimeout.Get();

  BeforeRequest();
  async_request_delegate_->GetFirstRectForRange(
      rfhi.get(), current_request_.value(), range);

  base::TimeDelta remaining_timeout = wait_timeout;
  while (!first_rect_ && remaining_timeout.is_positive()) {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    condition_.TimedWait(remaining_timeout);
    remaining_timeout = start + wait_timeout - base::LiveTicks::Now();
  }
  base::UmaHistogramBoolean("TextInputClient.FirstRect.TimedOut",
                            !first_rect_.has_value());

  // `first_rect_` is in (child) frame coordinate and needs to be transformed to
  // the root frame coordinate. If `rfhi` has been deleted, it's too late to do
  // the transform but the result is moot anyway.
  gfx::Rect rect =
      first_rect_ && rfhi
          ? gfx::Rect(rfhi->GetView()->TransformPointToRootCoordSpace(
                          first_rect_->origin()),
                      first_rect_->size())
          : gfx::Rect();
  AfterRequest();

  base::UmaHistogramLongTimes("TextInputClient.FirstRect2",
                              base::LiveTicks::Now() - start);

  return rect;
}

void TextInputClientMac::SetCharacterIndexAndSignal(
    const RequestToken& request_token,
    uint32_t index) {
  {
    base::AutoLock lock(lock_);
    if (!current_request_.has_value() ||
        current_request_.value() != request_token) {
      // Stale request.
      return;
    }
    character_index_ = index;
  }
  condition_.Signal();
}

void TextInputClientMac::SetFirstRectAndSignal(
    const RequestToken& request_token,
    const gfx::Rect& first_rect) {
  {
    base::AutoLock lock(lock_);
    if (!current_request_.has_value() ||
        current_request_.value() != request_token) {
      // Stale request.
      return;
    }
    first_rect_ = first_rect;
  }
  condition_.Signal();
}

void TextInputClientMac::SetAsyncRequestDelegateForTesting(
    std::unique_ptr<AsyncRequestDelegate> delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  async_request_delegate_ =
      delegate ? std::move(delegate)
               : std::make_unique<DefaultAsyncRequestDelegate>();
}

void TextInputClientMac::SetCharacterIndexWhileLockedForTesting(
    const RequestToken& request_token,
    uint32_t index) {
  // Drop the lock to signal the condition variable. Tests use this to simulate
  // a GetCharacterIndexAtPoint() response that arrives before the
  // `condition_.Wait()` call, so it must run on the same thread (not just
  // sequence) that calls Wait() to preserve ordering.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoUnlock unlock(lock_);
  SetCharacterIndexAndSignal(request_token, index);
}

void TextInputClientMac::SetFirstRectWhileLockedForTesting(
    const RequestToken& request_token,
    const gfx::Rect& first_rect) {
  // Drop the lock to signal the condition variable. Tests use this to simulate
  // a GetFirstRectForRange() response that arrives before the
  // `condition_.Wait()` call, so it must run on the same thread (not just
  // sequence) that calls Wait() to preserve ordering.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoUnlock unlock(lock_);
  SetFirstRectAndSignal(request_token, first_rect);
}

void TextInputClientMac::BeforeRequest() {
  CHECK(!in_sync_request_);
  in_sync_request_ = true;

  base::LiveTicks start = base::LiveTicks::Now();

  lock_.Acquire();

  base::UmaHistogramLongTimes("TextInputClient.LockWait2",
                              base::LiveTicks::Now() - start);

  CHECK(!current_request_.has_value());
  current_request_ = RequestToken();
  character_index_.reset();
  first_rect_.reset();
}

void TextInputClientMac::AfterRequest() {
  CHECK(current_request_.has_value());
  current_request_.reset();
  lock_.Release();

  CHECK(in_sync_request_);
  in_sync_request_ = false;
}

}  // namespace content
