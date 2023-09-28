// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/text_input_client_mac.h"

#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
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

RenderFrameHostImpl* GetFocusedRenderFrameHostImpl(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
  FrameTree* tree = rwhi->frame_tree();
  FrameTreeNode* focused_node = tree->GetFocusedFrame();
  return focused_node ? focused_node->current_frame_host() : nullptr;
}

}  // namespace

TextInputClientMac::TextInputClientMac()
    : character_index_(UINT32_MAX),
      lock_(),
      condition_(&lock_),
      wait_timeout_(features::kTextInputClientIPCTimeout.Get()) {}

TextInputClientMac::~TextInputClientMac() {
}

// static
TextInputClientMac* TextInputClientMac::GetInstance() {
  return base::Singleton<TextInputClientMac>::get();
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
  // If it doesn't have a focused frame, it calls |callback| with
  // an empty string and point.
  if (!rfhi)
    return std::move(callback).Run(nullptr, gfx::Point());

  rfhi->GetAssociatedLocalFrame()->GetStringForRange(range,
                                                     std::move(callback));
}

uint32_t TextInputClientMac::GetCharacterIndexAtPoint(RenderWidgetHost* rwh,
                                                      const gfx::Point& point) {
  RenderFrameHostImpl* rfhi = GetFocusedRenderFrameHostImpl(rwh);
  // If it doesn't have a focused frame, it calls
  // SetCharacterIndexAndSignal() with index 0.
  if (!rfhi)
    return 0;

  rfhi->GetAssociatedLocalFrame()->GetCharacterIndexAtPoint(point);

  base::TimeTicks start = base::TimeTicks::Now();

  BeforeRequest();

  // http://crbug.com/121917
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  condition_.TimedWait(wait_timeout_);
  AfterRequest();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.CharacterIndex",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  return character_index_;
}

gfx::Rect TextInputClientMac::GetFirstRectForRange(RenderWidgetHost* rwh,
                                                   const gfx::Range& range) {
  RenderFrameHostImpl* rfhi = GetFocusedRenderFrameHostImpl(rwh);
  if (!rfhi)
    return gfx::Rect();

  rfhi->GetAssociatedLocalFrame()->GetFirstRectForRange(range);

  base::TimeTicks start = base::TimeTicks::Now();

  BeforeRequest();

  // http://crbug.com/121917
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  condition_.TimedWait(wait_timeout_);
  AfterRequest();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.FirstRect",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  // `first_rect_` is in (child) frame coordinate and needs to be transformed to
  // the root frame coordinate.
  return gfx::Rect(
      rwh->GetView()->TransformPointToRootCoordSpace(first_rect_.origin()),
      first_rect_.size());
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

void TextInputClientMac::BeforeRequest() {
  base::TimeTicks start = base::TimeTicks::Now();

  lock_.Acquire();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.LockWait",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  character_index_ = UINT32_MAX;
  first_rect_ = gfx::Rect();
}

void TextInputClientMac::AfterRequest() {
  lock_.Release();
}

}  // namespace content
