// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/html/html_user_media_element.h"

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/html/user_media_request_provider.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

namespace {

PermissionDescriptorPtr CreatePermissionDescriptor(PermissionName name) {
  auto descriptor = PermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

// Helper to get permission text resource ID for the given map which has only
// one element.
uint16_t GetUntranslatedMessageIDSinglePermission(PermissionName name,
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

// Helper to get permission text resource ID for the given map which has
// multiple elements. Currently we only support "camera microphone" grouped
// permissions.
uint16_t GetUntranslatedMessageIDMultiplePermissions(bool granted) {
  return granted ? IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE_ALLOWED
                 : IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE;
}

Vector<PermissionDescriptorPtr> ParsePermissionDescriptorsFromString(
    const AtomicString& type) {
  SpaceSplitString permissions(type);
  Vector<PermissionDescriptorPtr> permission_descriptors;

  for (unsigned i = 0; i < permissions.size(); i++) {
    if (permissions[i] == "camera") {
      permission_descriptors.push_back(
          CreatePermissionDescriptor(PermissionName::VIDEO_CAPTURE));
    } else if (permissions[i] == "microphone") {
      permission_descriptors.push_back(
          CreatePermissionDescriptor(PermissionName::AUDIO_CAPTURE));
    } else {
      return Vector<PermissionDescriptorPtr>();
    }
  }

  if (permission_descriptors.size() <= 1) {
    return permission_descriptors;
  }

  if (permission_descriptors.size() >= 3) {
    return Vector<PermissionDescriptorPtr>();
  }

  if ((permission_descriptors[0]->name == PermissionName::VIDEO_CAPTURE &&
       permission_descriptors[1]->name == PermissionName::AUDIO_CAPTURE) ||
      (permission_descriptors[0]->name == PermissionName::AUDIO_CAPTURE &&
       permission_descriptors[1]->name == PermissionName::VIDEO_CAPTURE)) {
    return permission_descriptors;
  }

  return Vector<PermissionDescriptorPtr>();
}

}  // namespace

// static
bool HTMLUserMediaElement::isTypeSupported(const AtomicString& type) {
  return !ParsePermissionDescriptorsFromString(type).empty();
}

HTMLUserMediaElement::HTMLUserMediaElement(Document& document)
    : HTMLCapabilityElementBase(document, html_names::kUsermediaTag) {
  CHECK(RuntimeEnabledFeatures::UserMediaElementEnabled(
      document.GetExecutionContext()));
}

void HTMLUserMediaElement::Trace(Visitor* visitor) const {
  HTMLCapabilityElementBase::Trace(visitor);
  Supplementable<HTMLUserMediaElement>::Trace(visitor);
}

bool HTMLUserMediaElement::IsLegacyMode() const {
  // If the 'type' attribute is explicitly defined, we fallback to legacy
  // behavior.
  return FastHasAttribute(html_names::kTypeAttr);
}
void HTMLUserMediaElement::OnConstraintsSet(bool has_video, bool has_audio) {
  has_constraints_ = true;
  // If permission descriptors are already set, we do not need to update them.
  // This would be the case for legacy mode when the 'type' attribute is set.
  // We do not want to update the permission descriptors in this case as type
  // attribute is supposed to take precedence.
  if (!permission_descriptors_.empty()) {
    return;
  }
  if (has_video) {
    permission_descriptors_.push_back(
        CreatePermissionDescriptor(PermissionName::VIDEO_CAPTURE));
  }
  if (has_audio) {
    permission_descriptors_.push_back(
        CreatePermissionDescriptor(PermissionName::AUDIO_CAPTURE));
  }

  // Logic to handle registration when descriptors are set after insertion.
  if (!permission_descriptors_.empty()) {
    // Register with the cache to start receiving status updates
    MaybeRegisterCacheClient();
    // Register with the browser process (PEPC) to bind Mojo interfaces.
    MaybeRegisterPageEmbeddedPermissionControl();
    // Update the element's appearance based on initial cached statuses.
    UpdatePermissionStatusAndAppearance();
  }
}

void HTMLUserMediaElement::AttributeChanged(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kTypeAttr) {
    // `type` should only take effect once, when is added to the permission
    // element. Removing, or modifying the attribute has no effect.
    if (!type_.IsNull()) {
      return;
    }

    type_ = params.new_value;

    CHECK(permission_descriptors_.empty());
    permission_descriptors_ = ParseType(GetType());
    if (permission_descriptors_.empty()) {
      AuditsIssue::ReportPermissionElementIssue(
          GetExecutionContext(), GetDomNodeId(),
          protocol::Audits::PermissionElementIssueTypeEnum::InvalidType,
          GetType(), /*is_warning=*/false);
      EnableFallbackMode();
      return;
    }

    CHECK_LE(permission_descriptors_.size(), 2U)
        << "Unexpected permissions size " << permission_descriptors_.size();

    MaybeRegisterPageEmbeddedPermissionControl();
    return;
  }

  HTMLCapabilityElementBase::AttributeChanged(params);
}

void HTMLUserMediaElement::OnPermissionStatusChange(
    mojom::blink::PermissionName permission_name,
    mojom::blink::PermissionStatus status) {
  HTMLCapabilityElementBase::OnPermissionStatusChange(permission_name, status);

  if (PermissionsGranted() && HasPendingPermissionRequest() &&
      has_constraints_) {
    StartMediaStreamRequest();
  }
}

void HTMLUserMediaElement::DefaultEventHandler(Event& event) {
  // HTMLCapabilityElementBase::HandleActivation checks that the event is
  // trusted before proceeding with the permission request.
  // If the element only has type attribute and no constraints, we do not want
  // to start a getUserMedia request, the usermedia will keep functioning in the
  // legacy mode.
  if (event.type() == event_type_names::kDOMActivate && PermissionsGranted() &&
      has_constraints_) {
    HTMLCapabilityElementBase::HandleActivation(
        event, blink::BindOnce(&HTMLUserMediaElement::StartMediaStreamRequest,
                               WrapWeakPersistent(this)));
    return;
  }
  HTMLCapabilityElementBase::DefaultEventHandler(event);
}

mojom::blink::EmbeddedPermissionRequestDescriptorPtr
HTMLUserMediaElement::CreateEmbeddedPermissionRequestDescriptor() {
  auto descriptor = mojom::blink::EmbeddedPermissionRequestDescriptor::New(
      BoundsInWidget(),
      mojom::blink::EmbeddedPermissionControlDescriptorExtension::NewUserMedia(
          mojom::blink::UserMediaEmbeddedPermissionRequestDescriptor::New()));
  return descriptor;
}

Vector<PermissionDescriptorPtr> HTMLUserMediaElement::ParseType(
    const AtomicString& type) {
  return ParsePermissionDescriptorsFromString(type);
}

void HTMLUserMediaElement::StartMediaStreamRequest() {
  if (!media_stream_request_start_time_.is_null()) {
    return;
  }
  media_stream_request_start_time_ = base::TimeTicks::Now();

  // We should start a getUserMedia request only when the element has
  // constraints and the required permissions.
  CHECK_GT(permission_descriptors_.size(), 0U);
  CHECK_LE(permission_descriptors_.size(), 2U);
  CHECK(has_constraints_);
  CHECK(PermissionsGranted());
  if (GetDocument().domWindow()) {
    if (auto* provider =
            UserMediaRequestProvider::From(*GetDocument().domWindow())) {
      provider->StartRequest(this, permission_descriptors_);
    }
  }
}

void HTMLUserMediaElement::ResetMediaStreamRequestTime() {
  media_stream_request_start_time_ = base::TimeTicks();
}

void HTMLUserMediaElement::UpdateAppearance() {
  PermissionName permission_name;
  wtf_size_t permission_count;
  if (permission_status_map_.size() == 0U) {
    // Use |permission_descriptors_| instead and assume a "not granted" state.
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
  bool granted = PermissionsGranted();

  uint16_t untranslated_message_id;
  if (has_constraints_) {
    untranslated_message_id =
        permission_count == 1
            ? GetUntranslatedMessageIDSinglePermission(permission_name, false)
            : GetUntranslatedMessageIDMultiplePermissions(false);
  } else {
    untranslated_message_id =
        permission_count == 1
            ? GetUntranslatedMessageIDSinglePermission(permission_name, granted)
            : GetUntranslatedMessageIDMultiplePermissions(granted);
  }

  uint16_t translated_message_id =
      GetTranslatedMessageID(untranslated_message_id, language_string);
  CHECK(translated_message_id);
  permission_text_span()->setInnerText(
      GetLocale().QueryString(translated_message_id));
}

void HTMLUserMediaElement::UpdateIcon(PermissionName permission) {
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

}  // namespace blink
