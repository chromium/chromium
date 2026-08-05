// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/html/html_user_media_element.h"

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

namespace {

Vector<PermissionDescriptorPtr> ParsePermissionDescriptorsFromString(
    const AtomicString& type) {
  SpaceSplitString permissions(type);
  Vector<PermissionDescriptorPtr> permission_descriptors;

  for (unsigned i = 0; i < permissions.size(); i++) {
    if (permissions[i] == "camera") {
      permission_descriptors.push_back(
          HTMLMediaCaptureElementBase::CreatePermissionDescriptor(
              PermissionName::VIDEO_CAPTURE));
    } else if (permissions[i] == "microphone") {
      permission_descriptors.push_back(
          HTMLMediaCaptureElementBase::CreatePermissionDescriptor(
              PermissionName::AUDIO_CAPTURE));
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
    : HTMLMediaCaptureElementBase(document, html_names::kUsermediaTag) {
  CHECK(RuntimeEnabledFeatures::UserMediaElementEnabled(
      document.GetExecutionContext()));
}

bool HTMLUserMediaElement::IsLegacyMode() const {
  return RuntimeEnabledFeatures::UserMediaElementLegacyEnabled(
             GetExecutionContext()) &&
         hasAttribute(html_names::kTypeAttr);
}

DOMException* HTMLUserMediaElement::error() const {
  if (IsLegacyMode()) {
    return nullptr;
  }
  return HTMLMediaCaptureElementBase::error();
}

void HTMLUserMediaElement::AttributeChanged(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kTypeAttr) {
    if (!RuntimeEnabledFeatures::UserMediaElementLegacyEnabled(
            GetExecutionContext())) {
      HTMLCapabilityElementBase::AttributeChanged(params);
      return;
    }
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

  HTMLMediaCaptureElementBase::AttributeChanged(params);
}

void HTMLUserMediaElement::OnPermissionStatusChange(
    mojom::blink::PermissionName permission_name,
    mojom::blink::PermissionStatus status) {
  if (IsLegacyMode()) {
    HTMLCapabilityElementBase::OnPermissionStatusChange(permission_name,
                                                        status);
    return;
  }
  HTMLMediaCaptureElementBase::OnPermissionStatusChange(permission_name,
                                                        status);
}

void HTMLUserMediaElement::OnEmbeddedPermissionsDecided(
    mojom::blink::EmbeddedPermissionControlResult result) {
  if (IsLegacyMode()) {
    HTMLCapabilityElementBase::OnEmbeddedPermissionsDecided(result);
    return;
  }
  HTMLMediaCaptureElementBase::OnEmbeddedPermissionsDecided(result);
}

void HTMLUserMediaElement::DefaultEventHandler(Event& event) {
  if (IsLegacyMode()) {
    HTMLCapabilityElementBase::DefaultEventHandler(event);
    return;
  }
  HTMLMediaCaptureElementBase::DefaultEventHandler(event);
}

void HTMLUserMediaElement::OnActivationFailed(const String& error_message) {
  if (IsLegacyMode()) {
    return;
  }
  HTMLMediaCaptureElementBase::OnActivationFailed(error_message);
}

Vector<mojom::blink::PermissionDescriptorPtr> HTMLUserMediaElement::ParseType(
    const AtomicString& type) {
  return ParsePermissionDescriptorsFromString(type);
}

bool HTMLUserMediaElement::ShouldShowGrantedAppearance() const {
  return IsLegacyMode() && PermissionsGranted();
}

void HTMLUserMediaElement::ApplyDefaultConstraints() {
  if (!IsLegacyMode() && permission_descriptors_.empty()) {
    permission_descriptors_.push_back(
        CreatePermissionDescriptor(PermissionName::VIDEO_CAPTURE));
    permission_descriptors_.push_back(
        CreatePermissionDescriptor(PermissionName::AUDIO_CAPTURE));
  }
  HTMLMediaCaptureElementBase::ApplyDefaultConstraints();
}

void HTMLUserMediaElement::Trace(Visitor* visitor) const {
  HTMLMediaCaptureElementBase::Trace(visitor);
}

}  // namespace blink
