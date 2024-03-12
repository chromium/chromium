// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

using mojom::blink::EmbeddedPermissionControlResult;
using mojom::blink::EmbeddedPermissionRequestDescriptor;
using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionObserver;
using mojom::blink::PermissionService;
using MojoPermissionStatus = mojom::blink::PermissionStatus;

namespace {

const base::TimeDelta kDefaultDisableTimeout = base::Milliseconds(500);
constexpr FontSelectionValue kMinimumFontWeight = FontSelectionValue(200);

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
  // - Single permission: geolocation, camera, microphone.
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

// Helper to get permission text resource ID for the given map which has only
// one element.
int GetMessageIDSinglePermission(PermissionName name,
                                 MojoPermissionStatus status) {
  if (name == PermissionName::VIDEO_CAPTURE) {
    return status == MojoPermissionStatus::GRANTED
               ? IDS_PERMISSION_REQUEST_CAMERA_ALLOWED
               : IDS_PERMISSION_REQUEST_CAMERA;
  }

  if (name == PermissionName::AUDIO_CAPTURE) {
    return status == MojoPermissionStatus::GRANTED
               ? IDS_PERMISSION_REQUEST_MICROPHONE_ALLOWED
               : IDS_PERMISSION_REQUEST_MICROPHONE;
  }

  if (name == PermissionName::GEOLOCATION) {
    return status == MojoPermissionStatus::GRANTED
               ? IDS_PERMISSION_REQUEST_GEOLOCATION_ALLOWED
               : IDS_PERMISSION_REQUEST_GEOLOCATION;
  }

  return 0;
}

// Helper to get permission text resource ID for the given map which has
// multiple elements. Currently we only support "camera microphone" grouped
// permissions.
int GetMessageIDMultiplePermissions(
    const HashMap<PermissionName, MojoPermissionStatus>&
        permission_status_map) {
  CHECK_EQ(permission_status_map.size(), 2U);
  auto camera_it = permission_status_map.find(PermissionName::VIDEO_CAPTURE);
  auto mic_it = permission_status_map.find(PermissionName::AUDIO_CAPTURE);
  CHECK(camera_it != permission_status_map.end() &&
        mic_it != permission_status_map.end());

  if (camera_it->value == MojoPermissionStatus::GRANTED &&
      mic_it->value == MojoPermissionStatus::GRANTED) {
    return IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE_ALLOWED;
  }

  return IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE;
}

// Helper to get `PermissionsPolicyFeature` from permission name
mojom::blink::PermissionsPolicyFeature PermissionNameToPermissionsPolicyFeature(
    PermissionName permission_name) {
  switch (permission_name) {
    case PermissionName::AUDIO_CAPTURE:
      return mojom::blink::PermissionsPolicyFeature::kMicrophone;
    case PermissionName::VIDEO_CAPTURE:
      return mojom::blink::PermissionsPolicyFeature::kCamera;
    case PermissionName::GEOLOCATION:
      return mojom::blink::PermissionsPolicyFeature::kGeolocation;
    default:
      NOTREACHED_NORETURN() << "Not supported permission " << permission_name;
  }
}

// Helper to translate permission names into strings, primarily used for logging
// console messages.
String PermissionNameToString(PermissionName permission_name) {
  switch (permission_name) {
    case PermissionName::GEOLOCATION:
      return "geolocation";
    case PermissionName::AUDIO_CAPTURE:
      return "audio_capture";
    case PermissionName::VIDEO_CAPTURE:
      return "video_capture";
    default:
      NOTREACHED_NORETURN() << "Not supported permission " << permission_name;
  }
}

Length AdjustedMargin(const Length& margin) {
  if (margin.IsCalculated()) {
    if (margin.GetCalculationValue().IsNonNegative()) {
      return margin;
    }

    return Length(CalculationValue::CreateSimplified(
        margin.GetCalculationValue().GetOrCreateExpression(),
        Length::ValueRange::kNonNegative));
  }
  return (margin.Value() < 0) ? Length::FixedZero() : margin;
}

}  // namespace

HTMLPermissionElement::HTMLPermissionElement(Document& document)
    : HTMLElement(html_names::kPermissionTag, document),
      permission_service_(document.GetExecutionContext()),
      permission_observer_receivers_(this, document.GetExecutionContext()),
      embedded_permission_control_receiver_(this,
                                            document.GetExecutionContext()) {
  DCHECK(RuntimeEnabledFeatures::PermissionElementEnabled());
  SetHasCustomStyleCallbacks();
  intersection_observer_ = IntersectionObserver::Create(
      GetDocument(),
      WTF::BindRepeating(&HTMLPermissionElement::OnIntersectionChanged,
                         WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kPermissionElementIntersectionObserver,
      IntersectionObserver::Params{
          .thresholds = {1.0f},
          .semantics = IntersectionObserver::kFractionOfTarget,
          .behavior = IntersectionObserver::kDeliverDuringPostLifecycleSteps,
          .delay = 100,
          .track_visibility = true,
      });

  intersection_observer_->observe(this);
  EnsureUserAgentShadowRoot();
}

HTMLPermissionElement::~HTMLPermissionElement() = default;

const AtomicString& HTMLPermissionElement::GetType() const {
  return type_.IsNull() ? g_empty_atom : type_;
}

void HTMLPermissionElement::Trace(Visitor* visitor) const {
  visitor->Trace(permission_service_);
  visitor->Trace(permission_observer_receivers_);
  visitor->Trace(embedded_permission_control_receiver_);
  visitor->Trace(shadow_element_);
  visitor->Trace(permission_text_span_);
  visitor->Trace(intersection_observer_);
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

    CHECK(permission_descriptors_.empty());

    permission_descriptors_ = ParsePermissionDescriptorsFromString(GetType());
    switch (permission_descriptors_.size()) {
      case 0:
        AddConsoleError(
            String::Format("The permission type '%s' is not supported by the "
                           "permission element.",
                           GetType().Utf8().c_str()));
        return;
      case 1:
        permission_text_span_->setInnerText(
            GetLocale().QueryString(GetMessageIDSinglePermission(
                permission_descriptors_[0]->name, MojoPermissionStatus::ASK)));
        break;
      case 2:
        permission_text_span_->setInnerText(
            GetLocale().QueryString(IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE));
        break;
      default:
        NOTREACHED() << "Unexpected permissions size "
                     << permission_descriptors_.size();
    }

    if (GetDocument().GetFrame()->IsInFencedFrameTree()) {
      AddConsoleError(
          String::Format("The permission '%s' is not allowed in fenced frame",
                         GetType().Utf8().c_str()));
      return;
    }

    for (const PermissionDescriptorPtr& descriptor : permission_descriptors_) {
      if (!GetExecutionContext()->IsFeatureEnabled(
              PermissionNameToPermissionsPolicyFeature(descriptor->name))) {
        AddConsoleError(String::Format(
            "The permission '%s' is not allowed in the current context due to "
            "PermissionsPolicy",
            PermissionNameToString(descriptor->name).Utf8().c_str()));
        return;
      }
    }

    // TODO(crbug.com/1462930): We might consider not displaying the element
    // until the element is registered
    mojo::PendingRemote<EmbeddedPermissionControlClient> client;
    embedded_permission_control_receiver_.Bind(
        client.InitWithNewPipeAndPassReceiver(), GetTaskRunner());
    GetPermissionService()->RegisterPageEmbeddedPermissionControl(
        mojo::Clone(permission_descriptors_), std::move(client));
  }

  HTMLElement::AttributeChanged(params);
}

void HTMLPermissionElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  CHECK(!shadow_element_);
  shadow_element_ = MakeGarbageCollected<PermissionShadowElement>(*this);
  root.AppendChild(shadow_element_);
  permission_text_span_ = MakeGarbageCollected<HTMLSpanElement>(GetDocument());
  permission_text_span_->SetShadowPseudoId(
      shadow_element_names::kPseudoInternalPermissionTextSpan);
  shadow_element_->AppendChild(permission_text_span_);
}

void HTMLPermissionElement::AdjustStyle(ComputedStyleBuilder& builder) {
  Element::AdjustStyle(builder);

  builder.SetOutlineOffset(builder.OutlineOffset().ClampNegativeToZero());

  builder.SetMarginLeft(AdjustedMargin(builder.MarginLeft()));
  builder.SetMarginRight(AdjustedMargin(builder.MarginRight()));
  builder.SetMarginTop(AdjustedMargin(builder.MarginTop()));
  builder.SetMarginBottom(AdjustedMargin(builder.MarginBottom()));

  // Check and modify (if needed) properties related to the font.
  std::optional<FontDescription> new_font_description;

  // Font weight has to be at least kMinimumFontWeight.
  if (builder.GetFontDescription().Weight() <= kMinimumFontWeight) {
    if (!new_font_description) {
      new_font_description = builder.GetFontDescription();
    }
    new_font_description->SetWeight(kMinimumFontWeight);
  }

  // Any other values other than 'italic' and 'normal' are reset to 'normal'.
  if (builder.GetFontDescription().Style() != kItalicSlopeValue &&
      builder.GetFontDescription().Style() != kNormalSlopeValue) {
    if (!new_font_description) {
      new_font_description = builder.GetFontDescription();
    }
    new_font_description->SetStyle(kNormalSlopeValue);
  }

  if (new_font_description) {
    builder.SetFontDescription(*new_font_description);
  }

  // TODO(crbug.com/1462930): Validate here that the 'background-color' and
  // 'color' properties pass accessibility checks (and are at 100% alpha).

  // TODO(crbug.com/1462930): Add here checks to force the min/max-width/height.

  // TODO(crbug.com/1462930): Validate here the `letter-spacing` property, and
  // that it's not too big.

  // TODO(crbug.com/1462930): Set text direction (ltr\rtl) based on language.

  // TODO(crbug.com/1462930): Set word-spacing so it's at most 5px.

  // TODO(crbug.com/1462930): Ensure font-size at least as large as the
  // equivalent of 'small'.

  // TODO(crbug.com/1462930): Ensure any value of display other than 'none' is
  // converted to 'inline-block'.
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
  CHECK_GT(permission_descriptors_.size(), 0U);
  CHECK_LE(permission_descriptors_.size(), 2U);
  auto descriptor = EmbeddedPermissionRequestDescriptor::New();
  // TODO(crbug.com/1462930): Send element position to browser and use the
  // rect to calculate expected prompt position in screen coordinates.
  descriptor->element_position = GetBoundingClientRect()->ToEnclosingRect();
  descriptor->permissions = mojo::Clone(permission_descriptors_);
  GetPermissionService()->RequestPageEmbeddedPermission(
      std::move(descriptor),
      WTF::BindOnce(&HTMLPermissionElement::OnEmbeddedPermissionsDecided,
                    WrapWeakPersistent(this)));
}

void HTMLPermissionElement::RegisterPermissionObserver(
    const PermissionDescriptorPtr& descriptor,
    MojoPermissionStatus current_status) {
  mojo::PendingRemote<PermissionObserver> observer;
  permission_observer_receivers_.Add(observer.InitWithNewPipeAndPassReceiver(),
                                     descriptor->name, GetTaskRunner());
  GetPermissionService()->AddPermissionObserver(
      descriptor.Clone(), current_status, std::move(observer));
}

void HTMLPermissionElement::OnPermissionStatusChange(
    MojoPermissionStatus status) {
  auto permission_name = permission_observer_receivers_.current_context();
  auto it = permission_status_map_.find(permission_name);
  CHECK(it != permission_status_map_.end());
  it->value = status;
  UpdateAppearance();
}

void HTMLPermissionElement::OnEmbeddedPermissionControlRegistered(
    bool allowed,
    const std::optional<Vector<MojoPermissionStatus>>& statuses) {
  CHECK_EQ(permission_status_map_.size(), 0U);
  CHECK(!permissions_granted_);
  if (!allowed) {
    // TODO(crbug.com/1462930): We will not display the element in this case.
    return;
  }

  CHECK_GT(permission_descriptors_.size(), 0U);
  CHECK_LE(permission_descriptors_.size(), 2U);
  CHECK(statuses.has_value());
  CHECK_EQ(statuses->size(), permission_descriptors_.size());
  permissions_granted_ = true;
  for (wtf_size_t i = 0; i < permission_descriptors_.size(); ++i) {
    auto status = (*statuses)[i];
    const auto& descriptor = permission_descriptors_[i];
    auto inserted_result =
        permission_status_map_.insert(descriptor->name, status);
    CHECK(inserted_result.is_new_entry);
    permissions_granted_ &= (status == MojoPermissionStatus::GRANTED);
    RegisterPermissionObserver(descriptor, status);
  }

  UpdateAppearance();
}

void HTMLPermissionElement::OnEmbeddedPermissionsDecided(
    EmbeddedPermissionControlResult result) {
  switch (result) {
    case EmbeddedPermissionControlResult::kDismissed:
      DispatchEvent(*Event::Create(event_type_names::kDismiss));
      return;
    case EmbeddedPermissionControlResult::kGranted:
      permissions_granted_ = true;
      DispatchEvent(*Event::Create(event_type_names::kResolve));
      return;
    case EmbeddedPermissionControlResult::kDenied:
      DispatchEvent(*Event::Create(event_type_names::kResolve));
      return;
    case EmbeddedPermissionControlResult::kNotSupported:
      AddConsoleError(String::Format(
          "The permission request type '%s' is not supported and "
          "this <permission> element will not be functional.",
          GetType().Utf8().c_str()));
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
  // TODO(crbug.com/1462930): We might consider not displaying the element in
  // some certain situations, such as when the permission type is invalid or the
  // element was not able to be registered from browser process.
  if (permission_descriptors_.empty()) {
    return false;
  }

  // Do not check click-disabling reasons if the PEPC validation feature is
  // disabled. This should only occur in testing scenarios.
  if (RuntimeEnabledFeatures::DisablePepcSecurityForTestingEnabled()) {
    return true;
  }

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

void HTMLPermissionElement::EnableClickingAfterDelay(
    DisableReason reason,
    const base::TimeDelta& delay) {
  clicking_disabled_reasons_.Set(reason, base::TimeTicks::Now() + delay);
}

void HTMLPermissionElement::EnableClicking(DisableReason reason) {
  clicking_disabled_reasons_.erase(reason);
}

void HTMLPermissionElement::UpdateAppearance() {
  PseudoStateChanged(CSSSelector::kPseudoPermissionGranted);
  UpdateText();
}

void HTMLPermissionElement::UpdateText() {
  CHECK_GT(permission_status_map_.size(), 0U);
  CHECK_LE(permission_status_map_.size(), 2u);
  int message_id =
      permission_status_map_.size() == 1
          ? GetMessageIDSinglePermission(permission_status_map_.begin()->key,
                                         permission_status_map_.begin()->value)
          : GetMessageIDMultiplePermissions(permission_status_map_);

  CHECK(message_id);
  permission_text_span_->setInnerText(GetLocale().QueryString(message_id));
}

void HTMLPermissionElement::AddConsoleError(String error) {
  AddConsoleMessage(mojom::blink::ConsoleMessageSource::kRendering,
                    mojom::blink::ConsoleMessageLevel::kError, error);
}

void HTMLPermissionElement::OnIntersectionChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  CHECK(!entries.empty());
  Member<IntersectionObserverEntry> latest_observation = entries.back();

  CHECK_EQ(this, latest_observation->target());
  if (!latest_observation->isVisible() && is_fully_visible_) {
    is_fully_visible_ = false;
    DisableClickingIndefinitely(DisableReason::kIntersectionChanged);
    return;
  }

  if (latest_observation->isVisible() && !is_fully_visible_) {
    is_fully_visible_ = true;
    EnableClickingAfterDelay(DisableReason::kIntersectionChanged,
                             kDefaultDisableTimeout);
  }
}

}  // namespace blink
