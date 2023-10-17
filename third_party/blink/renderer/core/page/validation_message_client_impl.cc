/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/validation_message_client_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/validation_message_overlay_delegate.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

namespace {
// The max length of 256 is also used by other browsers:
// https://bugs.chromium.org/p/chromium/issues/detail?id=1261191#c17
constexpr int kMaxValidationStringLength = 256;
}  // namespace

ValidationMessageClientImpl::ValidationMessageClientImpl(Page& page)
    : page_(&page), current_anchor_(nullptr) {}

ValidationMessageClientImpl::~ValidationMessageClientImpl() = default;

LocalFrameView* ValidationMessageClientImpl::CurrentView() {
  return current_anchor_->GetDocument().View();
}

void ValidationMessageClientImpl::ShowValidationMessage(
    Element& anchor,
    const String& original_message,
    TextDirection message_dir,
    const String& sub_message,
    TextDirection sub_message_dir) {
  if (original_message.empty()) {
    HideValidationMessage(anchor);
    return;
  }
  if (!anchor.GetLayoutObject())
    return;

  // If this subframe or fencedframe is cross origin to the main frame, then
  // shorten the validation message to prevent validation popups that cover too
  // much of the main frame.
  String message = original_message;
  if (original_message.length() > kMaxValidationStringLength &&
      anchor.GetDocument().GetFrame()->IsCrossOriginToOutermostMainFrame()) {
    message = original_message.Substring(0, kMaxValidationStringLength) + "...";
  }

  if (current_anchor_)
    HideValidationMessageImmediately(*current_anchor_);
  current_anchor_ = &anchor;
  message_ = message;
  page_->GetChromeClient().RegisterPopupOpeningObserver(this);

  auto* target_frame = DynamicTo<LocalFrame>(page_->MainFrame());
  if (!target_frame)
    target_frame = &anchor.GetDocument().GetFrame()->LocalFrameRoot();

  allow_initial_empty_anchor_ = !target_frame->IsMainFrame();
  auto delegate = std::make_unique<ValidationMessageOverlayDelegate>(
      *page_, anchor, message_, message_dir, sub_message, sub_message_dir);
  overlay_delegate_ = delegate.get();
  DCHECK(!overlay_);
  overlay_ =
      MakeGarbageCollected<FrameOverlay>(target_frame, std::move(delegate));
  overlay_delegate_->CreatePage(*overlay_);
  bool success = target_frame->View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kOverlay);
  ValidationMessageVisibilityChanged(anchor);

  // The lifecycle update should always succeed, because this is not inside
  // of a throttling scope.
  DCHECK(success);
  LayoutOverlay();
}

void ValidationMessageClientImpl::HideValidationMessage(const Element& anchor) {
  if (WebTestSupport::IsRunningWebTest()) {
    HideValidationMessageImmediately(anchor);
    return;
  }
  if (!current_anchor_ || !IsValidationMessageVisible(anchor) ||
      overlay_delegate_->IsHiding()) {
    // Do not continue if already hiding, otherwise timer will never complete
    // and Reset() is never called.
    return;
  }
  DCHECK(overlay_);
  overlay_delegate_->StartToHide();
  timer_ = MakeGarbageCollected<
      DisallowNewWrapper<HeapTaskRunnerTimer<ValidationMessageClientImpl>>>(
      anchor.GetDocument().GetTaskRunner(TaskType::kInternalDefault), this,
      &ValidationMessageClientImpl::Reset);
  // This should be equal to or larger than transition duration of
  // #container.hiding in validation_bubble.css.
  const base::TimeDelta kHidingAnimationDuration = base::Seconds(0.13333);
  timer_->Value().StartOneShot(kHidingAnimationDuration, FROM_HERE);
}

void ValidationMessageClientImpl::HideValidationMessageImmediately(
    const Element& anchor) {
  if (!current_anchor_ || !IsValidationMessageVisible(anchor))
    return;
  Reset(nullptr);
}

void ValidationMessageClientImpl::Reset(TimerBase*) {
  Element& anchor = *current_anchor_;

  // Clearing out the pointer does not stop the timer.
  if (timer_)
    timer_->Value().Stop();
  timer_ = nullptr;
  current_anchor_ = nullptr;
  message_ = String();
  if (overlay_)
    overlay_.Release()->Destroy();
  overlay_delegate_ = nullptr;
  page_->GetChromeClient().UnregisterPopupOpeningObserver(this);
  ValidationMessageVisibilityChanged(anchor);
}

void ValidationMessageClientImpl::ValidationMessageVisibilityChanged(
    Element& element) {
  Document& document = element.GetDocument();
  if (AXObjectCache* cache = document.ExistingAXObjectCache())
    cache->HandleValidationMessageVisibilityChanged(&element);
}

bool ValidationMessageClientImpl::IsValidationMessageVisible(
    const Element& anchor) {
  return current_anchor_ == &anchor;
}

void ValidationMessageClientImpl::DocumentDetached(const Document& document) {
  if (current_anchor_ && current_anchor_->GetDocument() == document)
    HideValidationMessageImmediately(*current_anchor_);
}

void ValidationMessageClientImpl::DidChangeFocusTo(const Element* new_element) {
  if (current_anchor_ && current_anchor_ != new_element)
    HideValidationMessageImmediately(*current_anchor_);
}

void ValidationMessageClientImpl::CheckAnchorStatus(TimerBase*) {
  DCHECK(current_anchor_);
  if (!CurrentView()) {
    HideValidationMessage(*current_anchor_);
    return;
  }

  gfx::Rect new_anchor_rect_in_local_root =
      current_anchor_->VisibleBoundsInLocalRoot();
  if (new_anchor_rect_in_local_root.IsEmpty()) {
    // In a remote frame, VisibleBoundsInLocalRoot() may return an empty
    // rectangle while waiting for updated ancestor rects from the browser
    // (e.g. during initial load or scrolling). So we don't hide the validation
    // bubble until we see a non-empty rectangle.
    if (!allow_initial_empty_anchor_) {
      HideValidationMessage(*current_anchor_);
      return;
    }
  } else {
    allow_initial_empty_anchor_ = false;
  }
}

void ValidationMessageClientImpl::WillBeDestroyed() {
  if (current_anchor_)
    HideValidationMessageImmediately(*current_anchor_);
}

void ValidationMessageClientImpl::WillOpenPopup() {
  if (current_anchor_)
    HideValidationMessage(*current_anchor_);
}

void ValidationMessageClientImpl::ServiceScriptedAnimations(
    base::TimeTicks monotonic_frame_begin_time) {
  if (overlay_)
    overlay_->ServiceScriptedAnimations(monotonic_frame_begin_time);
}

void ValidationMessageClientImpl::LayoutOverlay() {
  if (overlay_)
    CheckAnchorStatus(nullptr);
}

void ValidationMessageClientImpl::UpdatePrePaint() {
  if (overlay_) {
    overlay_->UpdatePrePaint();
    DCHECK(overlay_delegate_);
    overlay_delegate_->UpdateFrameViewState(*overlay_);
  }
}

void ValidationMessageClientImpl::PaintOverlay(GraphicsContext& context) {
  if (overlay_)
    overlay_->Paint(context);
}

void ValidationMessageClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  visitor->Trace(current_anchor_);
  visitor->Trace(timer_);
  visitor->Trace(overlay_);
  ValidationMessageClient::Trace(visitor);
}

}  // namespace blink
