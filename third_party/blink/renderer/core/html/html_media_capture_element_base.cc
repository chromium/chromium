// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/html/html_media_capture_element_base.h"

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/user_media_request_provider.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

// static
uint16_t HTMLMediaCaptureElementBase::GetUntranslatedMessageIDSinglePermission(
    PermissionName name,
    bool granted) {
  if (name == PermissionName::VIDEO_CAPTURE) {
    return granted ? IDS_PERMISSION_REQUEST_CAMERA_ALLOWED
                   : IDS_PERMISSION_REQUEST_CAMERA;
  }

  if (name == PermissionName::AUDIO_CAPTURE) {
    return granted ? IDS_PERMISSION_REQUEST_MICROPHONE_ALLOWED
                   : IDS_PERMISSION_REQUEST_MICROPHONE;
  }

  return 0;
}

// static
uint16_t
HTMLMediaCaptureElementBase::GetUntranslatedMessageIDMultiplePermissions(
    bool granted) {
  return granted ? IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE_ALLOWED
                 : IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE;
}

// static
mojom::blink::PermissionDescriptorPtr
HTMLMediaCaptureElementBase::CreatePermissionDescriptor(PermissionName name) {
  auto descriptor = PermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

HTMLMediaCaptureElementBase::HTMLMediaCaptureElementBase(
    Document& document,
    const QualifiedName& tag_name)
    : HTMLCapabilityElementBase(document, tag_name) {}

void HTMLMediaCaptureElementBase::Trace(Visitor* visitor) const {
  visitor->Trace(error_);
  HTMLCapabilityElementBase::Trace(visitor);
  Supplementable<HTMLMediaCaptureElementBase>::Trace(visitor);
}

void HTMLMediaCaptureElementBase::OnPermissionStatusChange(
    mojom::blink::PermissionName permission_name,
    mojom::blink::PermissionStatus status) {
  HTMLCapabilityElementBase::OnPermissionStatusChange(permission_name, status);

  if (PermissionsGranted() && HasPendingPermissionRequest()) {
    StartMediaStreamRequest();
  }
}

void HTMLMediaCaptureElementBase::OnEmbeddedPermissionsDecided(
    mojom::blink::EmbeddedPermissionControlResult result) {
  HTMLCapabilityElementBase::OnEmbeddedPermissionsDecided(result);
  if (result == mojom::blink::EmbeddedPermissionControlResult::kDismissed ||
      result == mojom::blink::EmbeddedPermissionControlResult::kDenied) {
    SetError(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        result == mojom::blink::EmbeddedPermissionControlResult::kDismissed
            ? "Permission dismissed"
            : "Permission denied"));
    EnqueueEvent(*Event::Create(event_type_names::kCancel),
                 TaskType::kDOMManipulation);
  }
}

void HTMLMediaCaptureElementBase::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kDOMActivate) {
    if ((!GetExecutionContext() || !GetExecutionContext()->IsSecureContext()) &&
        !RuntimeEnabledFeatures::BypassPepcSecurityForTestingEnabled()) {
      AuditsIssue::ReportPermissionElementIssue(
          GetExecutionContext(), GetDomNodeId(),
          protocol::Audits::PermissionElementIssueTypeEnum::NonSecureContext,
          GetType(), /*is_warning=*/false);
      event.SetDefaultHandled();
      return;
    }
    if (!GetDocument().GetFrame() ||
        (!LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()) &&
         !RuntimeEnabledFeatures::BypassPepcSecurityForTestingEnabled())) {
      AuditsIssue::ReportPermissionElementIssue(
          GetExecutionContext(), GetDomNodeId(),
          protocol::Audits::PermissionElementIssueTypeEnum::
              MissingTransientUserActivation,
          GetType(), /*is_warning=*/false);
      OnActivationFailed(
          "The permission element activation must be triggered by a user "
          "gesture.");
      event.SetDefaultHandled();
      return;
    }
  }

  if (event.type() == event_type_names::kDOMActivate && PermissionsGranted()) {
    HTMLCapabilityElementBase::HandleActivation(
        event,
        blink::BindOnce(&HTMLMediaCaptureElementBase::StartMediaStreamRequest,
                        WrapWeakPersistent(this)));
    return;
  }
  HTMLCapabilityElementBase::DefaultEventHandler(event);
}

void HTMLMediaCaptureElementBase::OnActivationFailed(
    const String& error_message) {
  SetError(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kInvalidStateError, error_message));
  EnqueueEvent(*Event::Create(event_type_names::kError),
               TaskType::kDOMManipulation);
}

mojom::blink::EmbeddedPermissionRequestDescriptorPtr
HTMLMediaCaptureElementBase::CreateEmbeddedPermissionRequestDescriptor() {
  auto descriptor = mojom::blink::EmbeddedPermissionRequestDescriptor::New(
      BoundsInWidget(),
      mojom::blink::EmbeddedPermissionControlDescriptorExtension::NewUserMedia(
          mojom::blink::UserMediaEmbeddedPermissionRequestDescriptor::New()));
  return descriptor;
}

void HTMLMediaCaptureElementBase::StartMediaStreamRequest() {
  if (!media_stream_request_start_time_.is_null()) {
    return;
  }
  media_stream_request_start_time_ = base::TimeTicks::Now();

  CHECK_GT(permission_descriptors_.size(), 0U);
  CHECK_LE(permission_descriptors_.size(), 2U);
  CHECK(PermissionsGranted());
  if (GetDocument().domWindow()) {
    if (auto* provider =
            UserMediaRequestProvider::From(*GetDocument().domWindow())) {
      provider->StartRequest(this, permission_descriptors_);
    }
  }
}

void HTMLMediaCaptureElementBase::ResetMediaStreamRequestTime() {
  media_stream_request_start_time_ = base::TimeTicks();
}

void HTMLMediaCaptureElementBase::UpdateAppearance() {
  PermissionName permission_name;
  wtf_size_t permission_count;
  if (permission_status_map_.size() == 0U) {
    if (permission_descriptors_.size() == 0U) {
      return;
    }
    permission_name = permission_descriptors_[0]->name;
    permission_count = permission_descriptors_.size();
  } else {
    CHECK_LE(permission_status_map_.size(), 2u);
    permission_name = permission_status_map_.begin()->key;
    permission_count = permission_status_map_.size();
  }

  UpdateIcon(permission_count == 1 ? permission_name
                                   : PermissionName::VIDEO_CAPTURE);

  AtomicString language_string = ComputeInheritedLanguage().ToAsciiLower();
  bool granted = ShouldShowGrantedAppearance();

  uint16_t untranslated_message_id =
      permission_count == 1
          ? GetUntranslatedMessageIDSinglePermission(permission_name, granted)
          : GetUntranslatedMessageIDMultiplePermissions(granted);

  uint16_t translated_message_id =
      GetTranslatedMessageID(untranslated_message_id, language_string);
  CHECK(translated_message_id);
  permission_text_span()->setInnerText(
      GetLocale().QueryString(translated_message_id));
}

void HTMLMediaCaptureElementBase::UpdateIcon(PermissionName permission) {
  PermissionIconType icon_type;
  switch (permission) {
    case PermissionName::VIDEO_CAPTURE:
      icon_type = PermissionIconType::kCamera;
      break;
    case PermissionName::AUDIO_CAPTURE:
      icon_type = PermissionIconType::kMicrophone;
      break;
    default:
      return;
  }
  permission_internal_icon()->SetIcon(icon_type);
}

Node::InsertionNotificationRequest HTMLMediaCaptureElementBase::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLCapabilityElementBase::InsertedInto(insertion_point);
  if (permission_descriptors_.empty()) {
    GetTaskRunner()->PostTask(
        FROM_HERE,
        blink::BindOnce(
            [](HTMLMediaCaptureElementBase* element) {
              if (element) {
                element->ApplyDefaultConstraints();
              }
            },
            WrapWeakPersistent(this)));
  }
  return kInsertionDone;
}

void HTMLMediaCaptureElementBase::ApplyDefaultConstraints() {
  if (isConnected() && !permission_descriptors_.empty()) {
    MaybeRegisterCacheClient();
    MaybeRegisterPageEmbeddedPermissionControl();
    UpdatePermissionStatusAndAppearance();
  }
}

}  // namespace blink
