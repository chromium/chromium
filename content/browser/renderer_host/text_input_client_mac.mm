// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/text_input_client_mac.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/base/mojom/attributed_string.mojom.h"

namespace content {

namespace {

// TODO(ekaramad): TextInputClientMac expects to have a RenderWidgetHost to use
// for handling mojo calls. However, for fullscreen flash, we end up with a
// PepperWidget. For those scenarios, do not send the mojo calls. We need to
// figure out what features are properly supported and perhaps send the
// mojo call to the parent widget of the plugin (https://crbug.com/663384).
bool IsFullScreenRenderWidget(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
  if (!rwhi->delegate() ||
      rwhi == rwhi->delegate()->GetFullscreenRenderWidgetHost()) {
    return true;
  }
  return false;
}

RenderFrameHostImpl* GetFocusedRenderFrameHostImpl(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
  FrameTree* tree = rwhi->delegate()->GetFrameTree();
  FrameTreeNode* focused_node = tree->GetFocusedFrame();
  return focused_node ? focused_node->current_frame_host() : nullptr;
}

}  // namespace

// The amount of time in milliseconds that the browser process will wait for a
// response from the renderer.
// TODO(rsesek): Using the histogram data, find the best upper-bound for this
// value.
const float kWaitTimeout = 1500;

TextInputClientMac::TextInputClientMac()
    : character_index_(UINT32_MAX), lock_(), condition_(&lock_) {}

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
  rwhi->GetAssociatedFrameWidget()->GetStringAtPoint(point,
                                                     std::move(callback));
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
  if (IsFullScreenRenderWidget(rwh))
    return UINT32_MAX;

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
  condition_.TimedWait(base::TimeDelta::FromMilliseconds(kWaitTimeout));
  AfterRequest();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.CharacterIndex",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  return character_index_;
}

gfx::Rect TextInputClientMac::GetFirstRectForRange(RenderWidgetHost* rwh,
                                                   const gfx::Range& range) {
  if (IsFullScreenRenderWidget(rwh))
    return gfx::Rect();

  RenderFrameHostImpl* rfhi = GetFocusedRenderFrameHostImpl(rwh);
  if (!rfhi)
    return gfx::Rect();

  rfhi->GetAssociatedLocalFrame()->GetFirstRectForRange(range);

  base::TimeTicks start = base::TimeTicks::Now();

  BeforeRequest();

  // http://crbug.com/121917
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  condition_.TimedWait(base::TimeDelta::FromMilliseconds(kWaitTimeout));
  AfterRequest();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.FirstRect",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  return first_rect_;
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
