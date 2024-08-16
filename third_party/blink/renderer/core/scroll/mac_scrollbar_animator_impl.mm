// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/mac_scrollbar_animator_impl.h"

#import "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_mac.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {
namespace {

typedef HeapHashMap<WeakMember<const Scrollbar>, MacScrollbarImplV2*>
    ScrollbarToAnimatorV2Map;

ScrollbarToAnimatorV2Map& GetScrollbarToAnimatorV2Map() {
  DEFINE_STATIC_LOCAL(Persistent<ScrollbarToAnimatorV2Map>, map,
                      (MakeGarbageCollected<ScrollbarToAnimatorV2Map>()));
  return *map;
}

blink::ScrollbarThemeMac* MacOverlayScrollbarTheme(
    blink::ScrollbarTheme& scrollbar_theme) {
  return !scrollbar_theme.IsMockTheme()
             ? static_cast<blink::ScrollbarThemeMac*>(&scrollbar_theme)
             : nullptr;
}

bool IsScrollbarRegistered(blink::Scrollbar& scrollbar) {
  if (blink::ScrollbarThemeMac* scrollbar_theme =
          MacOverlayScrollbarTheme(scrollbar.GetTheme())) {
    return scrollbar_theme->IsScrollbarRegistered(scrollbar);
  }
  return false;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// MacScrollbarImplV2

MacScrollbarImplV2::MacScrollbarImplV2(
    Scrollbar& scrollbar,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : scrollbar_(scrollbar) {
  if (ScrollbarThemeMac::PreferOverlayScrollerStyle()) {
    int track_box_width_expanded = 0;
    int track_box_width_unexpanded = 0;
    switch (scrollbar_->CSSScrollbarWidth()) {
      case EScrollbarWidth::kNone:
        break;
      case EScrollbarWidth::kThin:
        track_box_width_expanded = 14;
        track_box_width_unexpanded = 10;
        break;
      case EScrollbarWidth::kAuto:
        track_box_width_expanded = 16;
        track_box_width_unexpanded = 12;
        break;
    }
    overlay_animator_ = std::make_unique<ui::OverlayScrollbarAnimatorMac>(
        this, track_box_width_expanded, track_box_width_unexpanded,
        task_runner);
  }
  GetScrollbarToAnimatorV2Map().Set(scrollbar_, this);
}

MacScrollbarImplV2::~MacScrollbarImplV2() {
  auto it = GetScrollbarToAnimatorV2Map().find(scrollbar_);
  GetScrollbarToAnimatorV2Map().erase(it);
}

bool MacScrollbarImplV2::IsAnimatorFor(Scrollbar& scrollbar) const {
  return &scrollbar == scrollbar_;
}

void MacScrollbarImplV2::MouseDidEnter() {
  if (overlay_animator_)
    overlay_animator_->MouseDidEnter();
}
void MacScrollbarImplV2::MouseDidExit() {
  if (overlay_animator_)
    overlay_animator_->MouseDidExit();
}

void MacScrollbarImplV2::DidScroll() {
  if (overlay_animator_)
    overlay_animator_->DidScroll();
}

float MacScrollbarImplV2::GetKnobAlpha() {
  if (overlay_animator_)
    return overlay_animator_->GetThumbAlpha();
  return 1.f;
}

float MacScrollbarImplV2::GetTrackAlpha() {
  if (overlay_animator_)
    return overlay_animator_->GetTrackAlpha();
  return 1.f;
}

int MacScrollbarImplV2::GetTrackBoxWidth() {
  if (overlay_animator_)
    return overlay_animator_->GetThumbWidth();

  switch (scrollbar_->CSSScrollbarWidth()) {
    case EScrollbarWidth::kNone:
      return 0;
    case EScrollbarWidth::kThin:
      return 11;
    case EScrollbarWidth::kAuto:
      return 15;
  }
}

bool MacScrollbarImplV2::IsMouseInScrollbarFrameRect() const {
  return scrollbar_->LastKnownMousePositionInFrameRect();
}
void MacScrollbarImplV2::SetHidden(bool hidden) {
  scrollbar_->SetScrollbarsHiddenFromExternalAnimator(hidden);
}
void MacScrollbarImplV2::SetThumbNeedsDisplay() {
  scrollbar_->SetNeedsPaintInvalidation(kThumbPart);
}
void MacScrollbarImplV2::SetTrackNeedsDisplay() {
  scrollbar_->SetNeedsPaintInvalidation(kTrackBGPart);
}

///////////////////////////////////////////////////////////////////////////////
// MacScrollbarAnimatorV2

MacScrollbarAnimatorV2::MacScrollbarAnimatorV2(ScrollableArea* scrollable_area)
    : task_runner_(scrollable_area->GetCompositorTaskRunner()) {}
MacScrollbarAnimatorV2::~MacScrollbarAnimatorV2() = default;

void MacScrollbarAnimatorV2::MouseEnteredScrollbar(Scrollbar& scrollbar) const {
  if (horizontal_scrollbar_ && horizontal_scrollbar_->IsAnimatorFor(scrollbar))
    horizontal_scrollbar_->MouseDidEnter();
  if (vertical_scrollbar_ && vertical_scrollbar_->IsAnimatorFor(scrollbar))
    vertical_scrollbar_->MouseDidEnter();
}

void MacScrollbarAnimatorV2::MouseExitedScrollbar(Scrollbar& scrollbar) const {
  if (horizontal_scrollbar_ && horizontal_scrollbar_->IsAnimatorFor(scrollbar))
    horizontal_scrollbar_->MouseDidExit();
  if (vertical_scrollbar_ && vertical_scrollbar_->IsAnimatorFor(scrollbar))
    vertical_scrollbar_->MouseDidExit();
}

void MacScrollbarAnimatorV2::DidAddVerticalScrollbar(Scrollbar& scrollbar) {
  if (!IsScrollbarRegistered(scrollbar))
    return;
  DCHECK(!vertical_scrollbar_);
  vertical_scrollbar_ =
      std::make_unique<MacScrollbarImplV2>(scrollbar, task_runner_);
}

void MacScrollbarAnimatorV2::WillRemoveVerticalScrollbar(Scrollbar& scrollbar) {
  vertical_scrollbar_.reset();
}

void MacScrollbarAnimatorV2::DidAddHorizontalScrollbar(Scrollbar& scrollbar) {
  if (!IsScrollbarRegistered(scrollbar))
    return;
  DCHECK(!horizontal_scrollbar_);
  horizontal_scrollbar_ =
      std::make_unique<MacScrollbarImplV2>(scrollbar, task_runner_);
}

void MacScrollbarAnimatorV2::WillRemoveHorizontalScrollbar(
    Scrollbar& scrollbar) {
  horizontal_scrollbar_.reset();
}

void MacScrollbarAnimatorV2::DidChangeUserVisibleScrollOffset(
    const ScrollOffset& new_offset) {
  if (horizontal_scrollbar_ && new_offset.x() != 0)
    horizontal_scrollbar_->DidScroll();
  if (vertical_scrollbar_ && new_offset.y() != 0)
    vertical_scrollbar_->DidScroll();
}

void MacScrollbarAnimatorV2::Dispose() {
  vertical_scrollbar_.reset();
  horizontal_scrollbar_.reset();
}

///////////////////////////////////////////////////////////////////////////////
// MacScrollbarAnimator

// static
MacScrollbarAnimator* MacScrollbarAnimator::Create(
    ScrollableArea* scrollable_area) {
  return MakeGarbageCollected<MacScrollbarAnimatorV2>(
      const_cast<ScrollableArea*>(scrollable_area));
}

///////////////////////////////////////////////////////////////////////////////
// MacScrollbar

// static
MacScrollbar* MacScrollbar::GetForScrollbar(const Scrollbar& scrollbar) {
  auto found = GetScrollbarToAnimatorV2Map().find(&scrollbar);
  if (found != GetScrollbarToAnimatorV2Map().end())
    return found->value;
  return nullptr;
}

}  // namespace blink
