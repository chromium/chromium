// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element.h"

#include "base/functional/bind.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

using mojom::blink::EmbeddedPermissionControlResult;
using mojom::blink::EmbeddedPermissionRequestDescriptor;
using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionService;

namespace {

const base::TimeDelta kDefaultDisableTimeout = base::Milliseconds(500);

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
  EnsureUserAgentShadowRoot();
}

HTMLPermissionElement::~HTMLPermissionElement() = default;

const AtomicString& HTMLPermissionElement::GetType() const {
  return type_.IsNull() ? g_empty_atom : type_;
}

void HTMLPermissionElement::Trace(Visitor* visitor) const {
  visitor->Trace(permission_service_);
  visitor->Trace(inner_element_);
  visitor->Trace(permission_text_);
  HTMLElement::Trace(visitor);
}

void HTMLPermissionElement::AttachLayoutTree(AttachContext& context) {
  Element::AttachLayoutTree(context);
  DisableClickingTemporarily(DisableReason::kRecentlyAttachedToDOM,
                             kDefaultDisableTimeout);
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
  if (params.name == html_names::kTypeAttr) {
    // `type` should only take effect once, when is added to the permission
    // element. Removing, or modifying the attribute has no effect.
    if (!type_.IsNull()) {
      return;
    }

    type_ = params.new_value;
  }

  HTMLElement::AttributeChanged(params);
}

void HTMLPermissionElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  CHECK(!inner_element_ && !permission_text_);
  inner_element_ = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  inner_element_->SetShadowPseudoId(
      AtomicString("-internal-permission-element-inner"));
  root.AppendChild(inner_element_);

  permission_text_ = MakeGarbageCollected<HTMLSpanElement>(GetDocument());
  // TODO(crbug.com/1462930): Set string based on permission type
  permission_text_->SetShadowPseudoId(
      AtomicString("-internal-permission-text"));
  inner_element_->AppendChild(permission_text_);
}

void HTMLPermissionElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kDOMActivate) {
    event.SetDefaultHandled();
    if (IsClickingEnabled()) {
      RequestPageEmbededPermissions();
    }
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
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kError,
        String::Format("The permission type '%s' is not supported by the "
                       "permission element.",
                       GetType().Utf8().c_str())));
    return;
  }

  auto descriptor = EmbeddedPermissionRequestDescriptor::New();
  // TODO(crbug.com/1462930): Send element position to browser and use the
  // rect to calculate expected prompt position in screen coordinates.
  descriptor->element_position = gfx::Rect(0, 0, 0, 0);
  descriptor->permissions = std::move(permission_descriptors);
  GetPermissionService()->RequestPageEmbeddedPermission(
      std::move(descriptor),
      WTF::BindOnce(&HTMLPermissionElement::OnEmbededPermissionsDecided,
                    WrapWeakPersistent(this)));
}

void HTMLPermissionElement::OnEmbededPermissionsDecided(
    EmbeddedPermissionControlResult result) {
  switch (result) {
    case EmbeddedPermissionControlResult::kDismissed:
      DispatchEvent(*Event::Create(event_type_names::kDismissed));
      return;
    case EmbeddedPermissionControlResult::kGranted:
      // TODO(crbug.com/1462930): Register and read permission statuses when
      // <permission> is attached to DOM and subscribe permission statuses
      // change.
      permissions_granted_ = true;
      PseudoStateChanged(CSSSelector::kPseudoPermissionGranted);
      DispatchEvent(*Event::Create(event_type_names::kResolved));
      return;
    case EmbeddedPermissionControlResult::kDenied:
      DispatchEvent(*Event::Create(event_type_names::kResolved));
      return;
    case EmbeddedPermissionControlResult::kNotSupported:
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kError,
          String::Format(
              "The permission request type '%s' is not supported and "
              "this <permission> element will not be functional.",
              GetType().Utf8().c_str())));
      return;
    case EmbeddedPermissionControlResult::kResolvedNoUserGesture:
      return;
  }
  NOTREACHED();
}

scoped_refptr<base::SingleThreadTaskRunner>
HTMLPermissionElement::GetTaskRunner() {
  return GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault);
}

bool HTMLPermissionElement::IsClickingEnabled() {
  // Remove expired reasons. If a non-expired reason is found, then clicking is
  // disabled.
  base::TimeTicks now = base::TimeTicks::Now();
  while (!clicking_disabled_reasons_.empty()) {
    auto it = clicking_disabled_reasons_.begin();
    if (it->value < now) {
      clicking_disabled_reasons_.erase(it);
    } else {
      return false;
    }
  }

  return true;
}

void HTMLPermissionElement::DisableClickingIndefinitely(DisableReason reason) {
  clicking_disabled_reasons_.insert(reason, base::TimeTicks::Max());
}

void HTMLPermissionElement::DisableClickingTemporarily(
    DisableReason reason,
    const base::TimeDelta& duration) {
  base::TimeTicks timeout_time = base::TimeTicks::Now() + duration;

  // If there is already an entry that expires later, keep the existing one.
  if (clicking_disabled_reasons_.Contains(reason) &&
      clicking_disabled_reasons_.at(reason) > timeout_time) {
    return;
  }

  clicking_disabled_reasons_.Set(reason, timeout_time);
}

void HTMLPermissionElement::EnableClicking(DisableReason reason) {
  clicking_disabled_reasons_.erase(reason);
}

}  // namespace blink
