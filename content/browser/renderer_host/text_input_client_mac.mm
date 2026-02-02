// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/text_input_client_mac.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
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

  void GetCharacterIndexAtPoint(RenderFrameHost* rfh,
                                const gfx::Point& point) final {
    RenderFrameHostImpl::From(rfh)
        ->GetAssociatedLocalFrame()
        ->GetCharacterIndexAtPoint(point);
  }

  void GetFirstRectForRange(RenderFrameHost* rfh,
                            const gfx::Range& range) final {
    RenderFrameHostImpl::From(rfh)
        ->GetAssociatedLocalFrame()
        ->GetFirstRectForRange(range);
  }
};

RenderFrameHostImpl* GetFocusedRenderFrameHostImpl(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
  FrameTree* tree = rwhi->frame_tree();
  FrameTreeNode* focused_node = tree->GetFocusedFrame();
  return focused_node ? focused_node->current_frame_host() : nullptr;
}

}  // namespace

TextInputClientMac::TextInputClientMac()
    : condition_(&lock_),
      wait_timeout_(features::kTextInputClientIPCTimeout.Get()),
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RenderFrameHostImpl* rfhi = GetFocusedRenderFrameHostImpl(rwh);
  // If it doesn't have a focused frame, it calls SetCharacterIndexAndSignal()
  // with index 0.
  if (!rfhi) {
    return 0;
  }

  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeDelta remaining_timeout = wait_timeout_;

  BeforeRequest();
  async_request_delegate_->GetCharacterIndexAtPoint(rfhi, point);
  while (!character_index_ && remaining_timeout.is_positive()) {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    condition_.TimedWait(remaining_timeout);
    remaining_timeout = start + wait_timeout_ - base::TimeTicks::Now();
  }
  // Return a sentinel if no response was received.
  uint32_t index = character_index_.value_or(UINT32_MAX);
  AfterRequest();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.CharacterIndex",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  return index;
}

gfx::Rect TextInputClientMac::GetFirstRectForRange(RenderWidgetHost* rwh,
                                                   const gfx::Range& range) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RenderFrameHostImpl* rfhi = GetFocusedRenderFrameHostImpl(rwh);
  if (!rfhi) {
    return gfx::Rect();
  }

  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeDelta remaining_timeout = wait_timeout_;

  BeforeRequest();
  async_request_delegate_->GetFirstRectForRange(rfhi, range);
  while (!first_rect_ && remaining_timeout.is_positive()) {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    condition_.TimedWait(remaining_timeout);
    remaining_timeout = start + wait_timeout_ - base::TimeTicks::Now();
  }
  // `first_rect_` is in (child) frame coordinate and needs to be transformed to
  // the root frame coordinate.
  gfx::Rect rect =
      first_rect_ ? gfx::Rect(rwh->GetView()->TransformPointToRootCoordSpace(
                                  first_rect_->origin()),
                              first_rect_->size())
                  : gfx::Rect();
  AfterRequest();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.FirstRect",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  return rect;
}

void TextInputClientMac::SetCharacterIndexAndSignal(uint32_t index) {
  lock_.Acquire();
  character_index_ = index;
  lock_.Release();
  condition_.Signal();
}

void TextInputClientMac::SetFirstRectAndSignal(const gfx::Rect& first_rect) {
  lock_.Acquire();
  first_rect_ = first_rect;
  lock_.Release();
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
    uint32_t index) {
  // Drop the lock to signal the condition variable. Tests use this to simulate
  // a GetCharacterIndexAtPoint() response that arrives before the
  // `condition_.Wait()` call, so it must run on the same thread (not just
  // sequence) that calls Wait() to preserve ordering.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoUnlock unlock(lock_);
  SetCharacterIndexAndSignal(index);
}

void TextInputClientMac::SetFirstRectWhileLockedForTesting(
    const gfx::Rect& first_rect) {
  // Drop the lock to signal the condition variable. Tests use this to simulate
  // a GetFirstRectForRange() response that arrives before the
  // `condition_.Wait()` call, so it must run on the same thread (not just
  // sequence) that calls Wait() to preserve ordering.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoUnlock unlock(lock_);
  SetFirstRectAndSignal(first_rect);
}

void TextInputClientMac::BeforeRequest() {
  base::TimeTicks start = base::TimeTicks::Now();

  lock_.Acquire();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.LockWait",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  character_index_.reset();
  first_rect_.reset();
}

void TextInputClientMac::AfterRequest() {
  lock_.Release();
}

}  // namespace content
