// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/html/html_permission_element_utils.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

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

// To support group permissions, the `type` attribute of permission element
// would contain a list of permissions (type is a space-separated string, for
// example <permission type=”camera microphone”>).
// This helper converts the type string to a list of `PermissionDescriptor`. If
// any of the splitted strings is invalid or not supported, return an empty
// list.
Vector<PermissionDescriptorPtr> ParsePermissionDescriptorsFromString(
    const AtomicString& type) {
  SpaceSplitString permissions(type);
  Vector<PermissionDescriptorPtr> permission_descriptors;

  // TODO(crbug.com/1462930): For MVP, we only support:
  // - Single permission: geolocation, camera, microphone, installation.
  // - Group of 2 permissions: camera and microphone (order does not matter).
  // - Repeats are *not* allowed: "camera camera" is invalid.
  for (unsigned i = 0; i < permissions.size(); i++) {
    if (permissions[i] == "geolocation") {
      permission_descriptors.push_back(
          CreatePermissionDescriptor(PermissionName::GEOLOCATION));
    } else if (permissions[i] == "camera") {
      permission_descriptors.push_back(
          CreatePermissionDescriptor(PermissionName::VIDEO_CAPTURE));
    } else if (permissions[i] == "microphone") {
      permission_descriptors.push_back(
          CreatePermissionDescriptor(PermissionName::AUDIO_CAPTURE));
    } else if (permissions[i] == "install") {
      permission_descriptors.push_back(
          CreatePermissionDescriptor(PermissionName::WEB_APP_INSTALLATION));
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
bool HTMLPermissionElement::isTypeSupported(const AtomicString& type) {
  return !ParsePermissionDescriptorsFromString(type).empty();
}

HTMLPermissionElement::HTMLPermissionElement(Document& document)
    : HTMLCapabilityElementBase(document, html_names::kPermissionTag) {
  CHECK(RuntimeEnabledFeatures::PermissionElementEnabled(
            document.GetExecutionContext()) ||
        RuntimeEnabledFeatures::GeolocationElementEnabled(
            document.GetExecutionContext()) ||
        RuntimeEnabledFeatures::UserMediaElementEnabled(
            document.GetExecutionContext()) ||
        RuntimeEnabledFeatures::InstallElementEnabled(
            document.GetExecutionContext()));
}

HTMLPermissionElement::~HTMLPermissionElement() = default;

const AtomicString& HTMLPermissionElement::GetType() const {
  return type_.IsNull() ? g_empty_atom : type_;
}

// static
Vector<PermissionDescriptorPtr>
HTMLPermissionElement::ParsePermissionDescriptorsForTesting(
    const AtomicString& type) {
  return ParsePermissionDescriptorsFromString(type);
}

void HTMLPermissionElement::setType(const AtomicString& type) {
  // `type` should only take effect once, when is added to the permission
  // element. Removing, or modifying the attribute has no effect.
  if (!type_.IsNull()) {
    return;
  }

  type_ = type;

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

  if (TagQName() == html_names::kPermissionTag && GetType() == "geolocation") {
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::GeolocationDeprecated,
        GetType(), /*is_warning=*/true);
  }
  CHECK_LE(permission_descriptors_.size(), 2U)
      << "Unexpected permissions size " << permission_descriptors_.size();
}

Vector<PermissionDescriptorPtr> HTMLPermissionElement::ParseType(
    const AtomicString& type) {
  return ParsePermissionDescriptorsFromString(type);
}

void HTMLPermissionElement::AttributeChanged(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kTypeAttr) {
    setType(params.new_value);
  }

  HTMLCapabilityElementBase::AttributeChanged(params);
}

}  // namespace blink
