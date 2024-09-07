// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/picture_in_picture_interstitial.h"

#include "cc/layers/layer.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/media_controls.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace {

constexpr base::TimeDelta kPictureInPictureStyleChangeTransitionDuration =
    base::Milliseconds(200);
constexpr base::TimeDelta kPictureInPictureHiddenAnimationSeconds =
    base::Milliseconds(300);

}  // namespace

namespace blink {

class PictureInPictureInterstitial::VideoElementResizeObserverDelegate final
    : public ResizeObserver::Delegate {
 public:
  explicit VideoElementResizeObserverDelegate(
      PictureInPictureInterstitial* interstitial)
      : interstitial_(interstitial) {
    DCHECK(interstitial);
  }
  ~VideoElementResizeObserverDelegate() override = default;

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(1u, entries.size());
    DCHECK_EQ(entries[0]->target(), interstitial_->GetVideoElement());
    DCHECK(entries[0]->contentRect());
    interstitial_->NotifyElementSizeChanged(*entries[0]->contentRect());
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(interstitial_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<PictureInPictureInterstitial> interstitial_;
};

PictureInPictureInterstitial::PictureInPictureInterstitial(
    HTMLVideoElement& videoElement)
    : HTMLDivElement(videoElement.GetDocument()),
      resize_observer_(ResizeObserver::Create(
          videoElement.GetDocument().domWindow(),
          MakeGarbageCollected<VideoElementResizeObserverDelegate>(this))),
      interstitial_timer_(
          videoElement.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &PictureInPictureInterstitial::ToggleInterstitialTimerFired),
      video_element_(&videoElement) {
  SetShadowPseudoId(AtomicString("-internal-media-interstitial"));

  background_image_ = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  background_image_->SetShadowPseudoId(
      AtomicString("-internal-media-interstitial-background-image"));
  background_image_->setAttribute(
      html_names::kSrcAttr,
      videoElement.FastGetAttribute(html_names::kPosterAttr));
  ParserAppendChild(background_image_);

  message_element_ = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  message_element_->SetShadowPseudoId(
      AtomicString("-internal-picture-in-picture-interstitial-message"));
  message_element_->setInnerText(GetVideoElement().GetLocale().QueryString(
      IDS_MEDIA_PICTURE_IN_PICTURE_INTERSTITIAL_TEXT));
  ParserAppendChild(message_element_);

  resize_observer_->observe(video_element_);
}

void PictureInPictureInterstitial::Show() {
  if (should_be_visible_)
    return;

  if (interstitial_timer_.IsActive())
    interstitial_timer_.Stop();
  should_be_visible_ = true;
  RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
  interstitial_timer_.StartOneShot(
      kPictureInPictureStyleChangeTransitionDuration, FROM_HERE);

  DCHECK(GetVideoElement().CcLayer());
  GetVideoElement().CcLayer()->SetIsDrawable(false);
  GetVideoElement().CcLayer()->SetHitTestable(false);
}

void PictureInPictureInterstitial::Hide() {
  if (!should_be_visible_)
    return;

  if (interstitial_timer_.IsActive())
    interstitial_timer_.Stop();
  should_be_visible_ = false;
  SetInlineStyleProperty(CSSPropertyID::kOpacity, 0,
                         CSSPrimitiveValue::UnitType::kNumber);
  interstitial_timer_.StartOneShot(kPictureInPictureHiddenAnimationSeconds,
                                   FROM_HERE);

  if (GetVideoElement().CcLayer()) {
    GetVideoElement().CcLayer()->SetIsDrawable(true);
    GetVideoElement().CcLayer()->SetHitTestable(true);
  }
}

Node::InsertionNotificationRequest PictureInPictureInterstitial::InsertedInto(
    ContainerNode& root) {
  if (GetVideoElement().isConnected() && !resize_observer_) {
    resize_observer_ = ResizeObserver::Create(
        GetVideoElement().GetDocument().domWindow(),
        MakeGarbageCollected<VideoElementResizeObserverDelegate>(this));
    resize_observer_->observe(&GetVideoElement());
  }

  return HTMLDivElement::InsertedInto(root);
}

void PictureInPictureInterstitial::RemovedFrom(ContainerNode& insertion_point) {
  DCHECK(!GetVideoElement().isConnected());

  if (resize_observer_) {
    resize_observer_->disconnect();
    resize_observer_.Clear();
  }

  HTMLDivElement::RemovedFrom(insertion_point);
}

void PictureInPictureInterstitial::NotifyElementSizeChanged(
    const DOMRectReadOnly& new_size) {
  message_element_->setAttribute(
      html_names::kClassAttr,
      MediaControls::GetSizingCSSClass(
          MediaControls::GetSizingClass(new_size.width())));

  // Force a layout since |LayoutMedia::UpdateLayout()| will sometimes miss a
  // layout otherwise.
  if (GetLayoutObject())
    GetLayoutObject()->SetNeedsLayout(layout_invalidation_reason::kSizeChanged);
}

void PictureInPictureInterstitial::ToggleInterstitialTimerFired(TimerBase*) {
  interstitial_timer_.Stop();
  if (should_be_visible_) {
    SetInlineStyleProperty(CSSPropertyID::kBackgroundColor, CSSValueID::kBlack);
    SetInlineStyleProperty(CSSPropertyID::kOpacity, 1,
                           CSSPrimitiveValue::UnitType::kNumber);
  } else {
    SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
  }
}

void PictureInPictureInterstitial::OnPosterImageChanged() {
  background_image_->setAttribute(
      html_names::kSrcAttr,
      GetVideoElement().FastGetAttribute(html_names::kPosterAttr));
}

void PictureInPictureInterstitial::Trace(Visitor* visitor) const {
  visitor->Trace(resize_observer_);
  visitor->Trace(interstitial_timer_);
  visitor->Trace(video_element_);
  visitor->Trace(background_image_);
  visitor->Trace(message_element_);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
