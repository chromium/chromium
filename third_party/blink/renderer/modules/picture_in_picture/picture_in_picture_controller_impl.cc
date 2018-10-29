// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_controller_impl.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/picture_in_picture_control_event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/picture_in_picture/enter_picture_in_picture_event.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_window.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

PictureInPictureControllerImpl::~PictureInPictureControllerImpl() = default;

// static
PictureInPictureControllerImpl* PictureInPictureControllerImpl::Create(
    Document& document) {
  return new PictureInPictureControllerImpl(document);
}

// static
PictureInPictureControllerImpl& PictureInPictureControllerImpl::From(
    Document& document) {
  return static_cast<PictureInPictureControllerImpl&>(
      PictureInPictureController::From(document));
}

bool PictureInPictureControllerImpl::PictureInPictureEnabled() const {
  return IsDocumentAllowed() == Status::kEnabled;
}

PictureInPictureController::Status
PictureInPictureControllerImpl::IsDocumentAllowed() const {
  DCHECK(GetSupplementable());

  // If document has been detached from a frame, return kFrameDetached status.
  LocalFrame* frame = GetSupplementable()->GetFrame();
  if (!frame)
    return Status::kFrameDetached;

  // `GetPictureInPictureEnabled()` returns false when the embedder or the
  // system forbids the page from using Picture-in-Picture.
  DCHECK(GetSupplementable()->GetSettings());
  if (!GetSupplementable()->GetSettings()->GetPictureInPictureEnabled())
    return Status::kDisabledBySystem;

  // If document is not allowed to use the policy-controlled feature named
  // "picture-in-picture", return kDisabledByFeaturePolicy status.
  if (RuntimeEnabledFeatures::PictureInPictureAPIEnabled() &&
      !GetSupplementable()->IsFeatureEnabled(
          blink::mojom::FeaturePolicyFeature::kPictureInPicture,
          ReportOptions::kReportOnFailure)) {
    return Status::kDisabledByFeaturePolicy;
  }

  return Status::kEnabled;
}

PictureInPictureController::Status
PictureInPictureControllerImpl::IsElementAllowed(
    const HTMLVideoElement& element) const {
  PictureInPictureController::Status status = IsDocumentAllowed();
  if (status != Status::kEnabled)
    return status;

  if (element.getReadyState() == HTMLMediaElement::kHaveNothing)
    return Status::kMetadataNotLoaded;

  if (!element.HasVideo())
    return Status::kVideoTrackNotAvailable;

  if (element.FastHasAttribute(HTMLNames::disablepictureinpictureAttr))
    return Status::kDisabledByAttribute;

  return Status::kEnabled;
}

void PictureInPictureControllerImpl::EnterPictureInPicture(
    HTMLVideoElement* element,
    ScriptPromiseResolver* resolver) {
  if (picture_in_picture_element_ != element) {
    element->enterPictureInPicture(
        WTF::Bind(&PictureInPictureControllerImpl::OnEnteredPictureInPicture,
                  WrapPersistent(this), WrapPersistent(element),
                  WrapPersistent(resolver)));
    // If the media element has already been given custom controls, this will
    // ensure that they get set. Otherwise, this will do nothing.
    element->SendCustomControlsToPipWindow();
    return;
  }

  if (resolver)
    resolver->Resolve(picture_in_picture_window_);
}

void PictureInPictureControllerImpl::OnEnteredPictureInPicture(
    HTMLVideoElement* element,
    ScriptPromiseResolver* resolver,
    const WebSize& picture_in_picture_window_size) {
  if (IsElementAllowed(*element) == Status::kDisabledByAttribute) {
    if (resolver) {
      resolver->Reject(
          DOMException::Create(DOMExceptionCode::kInvalidStateError, ""));
    }
    element->exitPictureInPicture(base::DoNothing());
    return;
  }

  picture_in_picture_element_ = element;

  picture_in_picture_element_->OnEnteredPictureInPicture();

  // Closes the current Picture-in-Picture window if any.
  if (picture_in_picture_window_)
    picture_in_picture_window_->OnClose();

  picture_in_picture_window_ = new PictureInPictureWindow(
      GetSupplementable(), picture_in_picture_window_size);

  picture_in_picture_element_->DispatchEvent(
      *EnterPictureInPictureEvent::Create(
          EventTypeNames::enterpictureinpicture,
          WrapPersistent(picture_in_picture_window_.Get())));

  element->GetWebMediaPlayer()->RegisterPictureInPictureWindowResizeCallback(
      WTF::BindRepeating(&PictureInPictureWindow::OnResize,
                         WrapPersistent(picture_in_picture_window_.Get())));

  if (resolver)
    resolver->Resolve(picture_in_picture_window_);
}

void PictureInPictureControllerImpl::ExitPictureInPicture(
    HTMLVideoElement* element,
    ScriptPromiseResolver* resolver) {
  element->exitPictureInPicture(
      WTF::Bind(&PictureInPictureControllerImpl::OnExitedPictureInPicture,
                WrapPersistent(this), WrapPersistent(resolver)));
}

void PictureInPictureControllerImpl::SetPictureInPictureCustomControls(
    HTMLVideoElement* element,
    const std::vector<PictureInPictureControlInfo>& controls) {
  element->SetPictureInPictureCustomControls(controls);
  if (IsPictureInPictureElement(element))
    element->SendCustomControlsToPipWindow();
}

void PictureInPictureControllerImpl::OnExitedPictureInPicture(
    ScriptPromiseResolver* resolver) {
  DCHECK(GetSupplementable());

  // Bail out if document is not active.
  if (!GetSupplementable()->IsActive())
    return;

  if (picture_in_picture_window_)
    picture_in_picture_window_->OnClose();

  if (picture_in_picture_element_) {
    HTMLVideoElement* element = picture_in_picture_element_;
    picture_in_picture_element_ = nullptr;

    element->OnExitedPictureInPicture();
    element->DispatchEvent(
        *Event::CreateBubble(EventTypeNames::leavepictureinpicture));
  }

  if (resolver)
    resolver->Resolve();
}

void PictureInPictureControllerImpl::OnPictureInPictureControlClicked(
    const WebString& control_id) {
  DCHECK(GetSupplementable());

  // Bail out if document is not active.
  if (!GetSupplementable()->IsActive())
    return;

  if (RuntimeEnabledFeatures::PictureInPictureControlEnabled() &&
      picture_in_picture_element_) {
    picture_in_picture_element_->DispatchEvent(
        *PictureInPictureControlEvent::Create(
            EventTypeNames::pictureinpicturecontrolclick, control_id));
  }
}

Element* PictureInPictureControllerImpl::PictureInPictureElement(
    TreeScope& scope) const {
  if (!picture_in_picture_element_)
    return nullptr;

  return scope.AdjustedElement(*picture_in_picture_element_);
}

bool PictureInPictureControllerImpl::IsPictureInPictureElement(
    const Element* element) const {
  DCHECK(element);
  return element == picture_in_picture_element_;
}

void PictureInPictureControllerImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(picture_in_picture_element_);
  visitor->Trace(picture_in_picture_window_);
  Supplement<Document>::Trace(visitor);
}

PictureInPictureControllerImpl::PictureInPictureControllerImpl(
    Document& document)
    : PictureInPictureController(document) {}

}  // namespace blink
