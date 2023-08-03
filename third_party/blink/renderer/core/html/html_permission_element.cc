// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element.h"

#include "base/functional/bind.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionService;

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
    } else {
      return Vector<PermissionDescriptorPtr>();
    }
  }

  return permission_descriptors;
}

}  // namespace

HTMLPermissionElement::HTMLPermissionElement(Document& document)
    : HTMLElement(html_names::kPermissionTag, document),
      permission_service_(document.GetExecutionContext()) {
  DCHECK(RuntimeEnabledFeatures::PermissionElementEnabled());
}

HTMLPermissionElement::~HTMLPermissionElement() = default;

const AtomicString& HTMLPermissionElement::GetType() const {
  return type_.IsNull() ? g_empty_atom : type_;
}

void HTMLPermissionElement::Trace(Visitor* visitor) const {
  visitor->Trace(permission_service_);
  HTMLElement::Trace(visitor);
}

// static
Vector<PermissionDescriptorPtr>
HTMLPermissionElement::ParsePermissionDescriptorsForTesting(
    const AtomicString& type) {
  return ParsePermissionDescriptorsFromString(type);
}

PermissionService* HTMLPermissionElement::GetPermissionService() {
  if (!permission_service_.is_bound()) {
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        permission_service_.BindNewPipeAndPassReceiver(GetTaskRunner()));
    permission_service_.set_disconnect_handler(WTF::BindOnce(
        &HTMLPermissionElement::OnPermissionServiceConnectionFailed,
        WrapWeakPersistent(this)));
  }

  return permission_service_.get();
}

void HTMLPermissionElement::OnPermissionServiceConnectionFailed() {
  permission_service_.reset();
}

void HTMLPermissionElement::AttributeChanged(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kTypeAttr && type_.IsNull()) {
    // `type` should only take effect once, when is added to the permission
    // element. Removing, or modifying the attribute has no effect.
    type_ = params.new_value;
    HTMLElement::AttributeChanged(params);
  }
}

void HTMLPermissionElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kDOMActivate) {
    event.SetDefaultHandled();
    RequestPageEmbededPermissions();
    return;
  }

  if (HandleKeyboardActivation(event)) {
    return;
  }
  HTMLElement::DefaultEventHandler(event);
}

void HTMLPermissionElement::RequestPageEmbededPermissions() {
  auto permission_descriptors = ParsePermissionDescriptorsFromString(GetType());
  if (permission_descriptors.empty()) {
    return;
  }

  // TODO(crbug.com/1462930): Handle requesting permission in browser side.
  // We will propose a new mojo API in PermissionService to support requesting
  // permission from <permission> element.
  GetPermissionService()->RequestPermissions(std::move(permission_descriptors),
                                             /*user_gesture=*/true,
                                             base::NullCallback());
}

scoped_refptr<base::SingleThreadTaskRunner>
HTMLPermissionElement::GetTaskRunner() {
  return GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault);
}

}  // namespace blink
