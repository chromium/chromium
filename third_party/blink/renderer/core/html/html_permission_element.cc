// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element.h"

#include <stdint.h>

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_permission_state.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/font_size_functions.h"
#include "third_party/blink/renderer/core/css/properties/css_property_instances.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_permission_element_strings_map.h"
#include "third_party/blink/renderer/core/html/html_permission_element_utils.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

using mojom::blink::EmbeddedPermissionControlResult;
using mojom::blink::EmbeddedPermissionRequestDescriptor;
using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionObserver;
using mojom::blink::PermissionService;
using MojoPermissionStatus = mojom::blink::PermissionStatus;
// A data structure that maps Permission element MessageIds to locale specific
// MessageIds.
// Key of the outer map: locale.
// Key of the inner map: The base MessageId (in english).
// Value of the outer map: The corresponding MessageId in the given locale.
using GeneratedMessagesMap = HashMap<String, HashMap<int, int>>;

namespace {

const base::TimeDelta kDefaultDisableTimeout = base::Milliseconds(500);
constexpr FontSelectionValue kMinimumFontWeight = FontSelectionValue(200);
constexpr float kMaximumWordSpacingToFontSizeRatio = 0.5;
constexpr float kMinimumAllowedContrast = 3.;
constexpr float kMaximumLetterSpacingToFontSizeRatio = 0.2;
constexpr float kMinimumLetterSpacingToFontSizeRatio = -0.05;
constexpr int kMarginVisibleContent = -4;
constexpr int kMaxLengthToFontSizeRatio = 3;
constexpr int kMinLengthToFontSizeRatio = 1;
constexpr int kMaxVerticalPaddingToFontSizeRatio = 1;
constexpr int kMaxHorizontalPaddingToFontSizeRatio = 5;
constexpr float kMaxBorderWidthToFontSizeRatio = 0.5;
constexpr float kIntersectionThreshold = 1.0f;

constexpr float kDefaultSmallFontSize = 13;     // Default 'small' font size.
constexpr float kDefaultXxxLargeFontSize = 48;  // Default 'xxxlarge' font size.

constexpr float kDefaultMaxPercentRadiusWidth = 25;
constexpr float kDefaultMaxPercentRadiusHeight = 50;

// These display styles are not allowed for permission elements as they can mess
// with the layout in unsupported ways. Additionally, all "table" styles are
// also not allowed.
constexpr EDisplay kInvalidDisplayStyles[] = {
    EDisplay::kContents, EDisplay::kInline,   EDisplay::kListItem,
    EDisplay::kRuby,     EDisplay::kRubyText,
};

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

// Helper to get permission text resource ID for the given map which has only
// one element.
uint16_t GetUntranslatedMessageIDSinglePermission(PermissionName name,
                                                  bool granted,
                                                  bool is_precise_location) {
  if (name == PermissionName::VIDEO_CAPTURE) {
    return granted ? IDS_PERMISSION_REQUEST_CAMERA_ALLOWED
                   : IDS_PERMISSION_REQUEST_CAMERA;
  }

  if (name == PermissionName::AUDIO_CAPTURE) {
    return granted ? IDS_PERMISSION_REQUEST_MICROPHONE_ALLOWED
                   : IDS_PERMISSION_REQUEST_MICROPHONE;
  }

  if (name == PermissionName::GEOLOCATION) {
    if (is_precise_location) {
      // This element uses precise location.
      return granted ? IDS_PERMISSION_REQUEST_PRECISE_GEOLOCATION_ALLOWED
                     : IDS_PERMISSION_REQUEST_PRECISE_GEOLOCATION;
    }
    return granted ? IDS_PERMISSION_REQUEST_GEOLOCATION_ALLOWED
                   : IDS_PERMISSION_REQUEST_GEOLOCATION;
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

// Helper to get `PermissionsPolicyFeature` from permission name
network::mojom::PermissionsPolicyFeature
PermissionNameToPermissionsPolicyFeature(PermissionName permission_name) {
  switch (permission_name) {
    case PermissionName::AUDIO_CAPTURE:
      return network::mojom::PermissionsPolicyFeature::kMicrophone;
    case PermissionName::VIDEO_CAPTURE:
      return network::mojom::PermissionsPolicyFeature::kCamera;
    case PermissionName::GEOLOCATION:
      return network::mojom::PermissionsPolicyFeature::kGeolocation;
    case PermissionName::WEB_APP_INSTALLATION:
      return network::mojom::PermissionsPolicyFeature::kWebAppInstallation;
    default:
      NOTREACHED() << "Not supported permission " << permission_name;
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
    case PermissionName::WEB_APP_INSTALLATION:
      return "web_app_installation";
    default:
      NOTREACHED() << "Not supported permission " << permission_name;
  }
}

// Helper to translated permission statuses to strings.
V8PermissionState::Enum PermissionStatusToV8Enum(MojoPermissionStatus status) {
  switch (status) {
    case MojoPermissionStatus::GRANTED:
      return V8PermissionState::Enum::kGranted;
    case MojoPermissionStatus::ASK:
      return V8PermissionState::Enum::kPrompt;
    case MojoPermissionStatus::DENIED:
      return V8PermissionState::Enum::kDenied;
  }
  NOTREACHED();
}

float ContrastBetweenColorAndBackgroundColor(const ComputedStyle* style) {
  return color_utils::GetContrastRatio(
      style->VisitedDependentColor(GetCSSPropertyColor()).toSkColor4f(),
      style->VisitedDependentColor(GetCSSPropertyBackgroundColor())
          .toSkColor4f());
}

// Returns the minimum contrast between the background color and all four border
// colors.
float ContrastBetweenColorAndBorderColor(const ComputedStyle* style) {
  auto background_color =
      style->VisitedDependentColor(GetCSSPropertyBackgroundColor())
          .toSkColor4f();
  SkColor4f border_colors[] = {
      style->VisitedDependentColor(GetCSSPropertyBorderBottomColor())
          .toSkColor4f(),
      style->VisitedDependentColor(GetCSSPropertyBorderTopColor())
          .toSkColor4f(),
      style->VisitedDependentColor(GetCSSPropertyBorderLeftColor())
          .toSkColor4f(),
      style->VisitedDependentColor(GetCSSPropertyBorderRightColor())
          .toSkColor4f()};

  float min_contrast = SK_FloatInfinity;
  float contrast;
  for (const auto& border_color : border_colors) {
    contrast = color_utils::GetContrastRatio(border_color, background_color);
    if (min_contrast > contrast) {
      min_contrast = contrast;
    }
  }

  return min_contrast;
}

// Returns true if the 'color' or 'background-color' properties have the
// alphas set to anything else except fully opaque.
bool AreColorsNonOpaque(const ComputedStyle* style) {
  return style->VisitedDependentColor(GetCSSPropertyColor()).Alpha() != 1. ||
         style->VisitedDependentColor(GetCSSPropertyBackgroundColor())
                 .Alpha() != 1;
}

// Returns true if any border color has an alpha that is not fully opaque.
bool AreBorderColorsNonOpaque(const ComputedStyle* style) {
  return style->VisitedDependentColor(GetCSSPropertyBorderBottomColor())
                 .Alpha() != 1. ||
         style->VisitedDependentColor(GetCSSPropertyBorderTopColor()).Alpha() !=
             1. ||
         style->VisitedDependentColor(GetCSSPropertyBorderLeftColor())
                 .Alpha() != 1. ||
         style->VisitedDependentColor(GetCSSPropertyBorderRightColor())
                 .Alpha() != 1.;
}

bool IsBorderSufficientlyDistinctFromBackgroundColor(
    const ComputedStyle* style) {
  if (!style || !style->HasBorder()) {
    return false;
  }

  if (style->BorderBottomWidth() == 0 || style->BorderTopWidth() == 0 ||
      style->BorderLeftWidth() == 0 || style->BorderRightWidth() == 0) {
    return false;
  }

  if (AreBorderColorsNonOpaque(style)) {
    return false;
  }

  if (ContrastBetweenColorAndBorderColor(style) < kMinimumAllowedContrast) {
    return false;
  }

  return true;
}

void RecordUserInteractionAccepted(bool accepted) {
  base::UmaHistogramBoolean("Blink.PermissionElement.UserInteractionAccepted",
                            accepted);
}

}  // namespace

// static
bool HTMLPermissionElement::isTypeSupported(const AtomicString& type) {
  return !ParsePermissionDescriptorsFromString(type).empty();
}

HTMLPermissionElement::HTMLPermissionElement(
    Document& document,
    std::optional<QualifiedName> tag_name)
    : HTMLElement(tag_name.value_or(html_names::kPermissionTag), document),
      permission_service_(document.GetExecutionContext()),
      embedded_permission_control_receiver_(this,
                                            document.GetExecutionContext()),
      disable_reason_expire_timer_(
          this,
          &HTMLPermissionElement::DisableReasonExpireTimerFired) {
  CHECK(RuntimeEnabledFeatures::PermissionElementEnabled(
            document.GetExecutionContext()) ||
        RuntimeEnabledFeatures::GeolocationElementEnabled(
            document.GetExecutionContext()) ||
        RuntimeEnabledFeatures::UserMediaElementEnabled(
            document.GetExecutionContext()));
  SetHasCustomStyleCallbacks();
  EnsureUserAgentShadowRoot();
  UseCounter::Count(document, WebFeature::kHTMLPermissionElement);
}

HTMLPermissionElement::~HTMLPermissionElement() = default;

const AtomicString& HTMLPermissionElement::GetType() const {
  return type_.IsNull() ? g_empty_atom : type_;
}

String HTMLPermissionElement::invalidReason() const {
  return clicking_enabled_state_.invalid_reason;
}

bool HTMLPermissionElement::isValid() const {
  return clicking_enabled_state_.is_valid;
}

V8PermissionState HTMLPermissionElement::initialPermissionStatus() const {
  return V8PermissionState(
      PermissionStatusToV8Enum(initial_aggregated_permission_status_.value_or(
          MojoPermissionStatus::ASK)));
}

V8PermissionState HTMLPermissionElement::permissionStatus() const {
  return V8PermissionState(PermissionStatusToV8Enum(
      aggregated_permission_status_.value_or(MojoPermissionStatus::ASK)));
}

void HTMLPermissionElement::Trace(Visitor* visitor) const {
  visitor->Trace(permission_service_);
  visitor->Trace(embedded_permission_control_receiver_);
  visitor->Trace(permission_container_);
  visitor->Trace(permission_text_span_);
  visitor->Trace(permission_internal_icon_);
  visitor->Trace(intersection_observer_);
  visitor->Trace(disable_reason_expire_timer_);
  HTMLElement::Trace(visitor);
}

void HTMLPermissionElement::OnPermissionStatusInitialized(
    PermissionStatusMap initilized_map) {
  permission_status_map_ = std::move(initilized_map);
  HTMLPermissionElement::UpdatePermissionStatusAndAppearance();
}

Node::InsertionNotificationRequest HTMLPermissionElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  if (!is_cache_registered_ && !permission_descriptors_.empty()) {
    CachedPermissionStatus::From(GetDocument().domWindow())
        ->RegisterClient(this, permission_descriptors_);
    is_cache_registered_ = true;
  }
  return kInsertionDone;
}

void HTMLPermissionElement::AttachLayoutTree(AttachContext& context) {
  Element::AttachLayoutTree(context);
  if (fallback_mode_) {
    return;
  }
  DisableClickingTemporarily(DisableReason::kRecentlyAttachedToLayoutTree,
                             kDefaultDisableTimeout);
  CHECK(GetDocument().View());
  GetDocument().View()->RegisterForLifecycleNotifications(this);
  if (!intersection_observer_) {
    intersection_observer_ = IntersectionObserver::Create(
        GetDocument(),
        BindRepeating(&HTMLPermissionElement::OnIntersectionChanged,
                      WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kPermissionElementIntersectionObserver,
        IntersectionObserver::Params{
            .margin = {Length::Fixed(kMarginVisibleContent)},
            .margin_target = IntersectionObserver::kApplyMarginToTarget,
            .thresholds = {kIntersectionThreshold},
            .semantics = IntersectionObserver::kFractionOfTarget,
            .behavior = IntersectionObserver::kDeliverDuringPostLifecycleSteps,
            .delay = base::Milliseconds(100),
            .track_visibility = true,
            .expose_occluder_id = true,
        });

    intersection_observer_->observe(this);
  }
}

void HTMLPermissionElement::DetachLayoutTree(bool performing_reattach) {
  Element::DetachLayoutTree(performing_reattach);
  if (auto* view = GetDocument().View()) {
    view->UnregisterFromLifecycleNotifications(this);
  }
}

void HTMLPermissionElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  permission_status_map_.clear();
  aggregated_permission_status_ = std::nullopt;
  if (disable_reason_expire_timer_.IsActive()) {
    disable_reason_expire_timer_.Stop();
  }
  intersection_rect_ = std::nullopt;
  LocalDOMWindow* window = GetDocument().domWindow();
  if (window && is_cache_registered_) {
    CachedPermissionStatus::From(window)->UnregisterClient(
        this, permission_descriptors_);
    is_cache_registered_ = false;
  }
  EnsureUnregisterPageEmbeddedPermissionControl();
}

void HTMLPermissionElement::Focus(const FocusParams& params) {
  // In fallback mode the permission element behaves like a regular element.
  if (fallback_mode_) {
    return HTMLElement::Focus(params);
  }
  // This will only apply to `focus` and `blur` JS API. Other focus types (like
  // accessibility focusing and manual user focus), will still be permitted as
  // usual.
  if (params.type == mojom::blink::FocusType::kScript &&
      !LocalFrame::HasTransientUserActivation(GetDocument().GetFrame())) {
    return;
  }

  HTMLElement::Focus(params);
}

FocusableState HTMLPermissionElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (fallback_mode_) {
    return HTMLElement::SupportsFocus(update_behavior);
  }

  return FocusableState::kFocusable;
}

int HTMLPermissionElement::DefaultTabIndex() const {
  // The permission element behaves similarly to a button and therefore is
  // focusable via keyboard by default.
  return 0;
}

CascadeFilter HTMLPermissionElement::GetCascadeFilter() const {
  // Reject all properties for which 'kValidForPermissionElement' is false.
  return CascadeFilter(CSSProperty::kValidForPermissionElement);
}

bool HTMLPermissionElement::CanGeneratePseudoElement(PseudoId id) const {
  switch (id) {
    case PseudoId::kPseudoIdAfter:
    case PseudoId::kPseudoIdBefore:
    case PseudoId::kPseudoIdCheckMark:
    case PseudoId::kPseudoIdPickerIcon:
    case PseudoId::kPseudoIdInterestHint:
      return false;
    default:
      return Element::CanGeneratePseudoElement(id);
  }
}

bool HTMLPermissionElement::IsRenderered() const {
  if (GetComputedStyle() &&
      GetComputedStyle()->Visibility() == EVisibility::kVisible) {
    return true;
  }

  return false;
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

uint16_t HTMLPermissionElement::GetTranslatedMessageID(
    uint16_t message_id,
    const AtomicString& language_string) {
  DCHECK(language_string.IsLowerASCII());
  if (language_string.empty()) {
    return message_id;
  }

  StringUtf8Adaptor lang_adaptor(language_string);
  std::string_view lang_utf8 = lang_adaptor.AsStringView();
  if (auto mapped_id = GetPermissionElementMessageId(lang_utf8, message_id);
      mapped_id.has_value()) {
    return *mapped_id;
  }

  auto parts = base::SplitStringOnce(lang_utf8, '-');
  if (!parts) {
    return message_id;
  }
  // This is to support locales with unknown combination of languages and
  // countries. If the combination of language and country is not known,
  // the code will fallback to strings just from the language part of the
  // locale.
  // Eg: en-au is a unknown combination, in this case we will fall back to
  // en strings.
  return GetPermissionElementMessageId(parts->first, message_id)
      .value_or(message_id);
}

void HTMLPermissionElement::UpdateAppearance() {
  bool permission_granted;
  PermissionName permission_name;
  wtf_size_t permission_count;
  if (permission_status_map_.size() == 0U) {
    // Use |permission_descriptors_| instead and assume a "not granted" state.
    if (permission_descriptors_.size() == 0U) {
      return;
    }
    permission_granted = false;
    permission_name = permission_descriptors_[0]->name;
    permission_count = permission_descriptors_.size();
  } else {
    CHECK_LE(permission_status_map_.size(), 2u);
    permission_granted = PermissionsGranted();
    permission_name = permission_status_map_.begin()->key;
    permission_count = permission_status_map_.size();
  }

  UpdateIcon(permission_count == 1 ? permission_name
                                   : PermissionName::VIDEO_CAPTURE);

  AtomicString language_string = ComputeInheritedLanguage().LowerASCII();

  uint16_t untranslated_message_id =
      permission_count == 1
          ? GetUntranslatedMessageIDSinglePermission(
                permission_name, permission_granted, is_precise_location_)
          : GetUntranslatedMessageIDMultiplePermissions(permission_granted);
  uint16_t translated_message_id =
      GetTranslatedMessageID(untranslated_message_id, language_string);
  CHECK(translated_message_id);
  permission_text_span_->setInnerText(
      GetLocale().QueryString(translated_message_id));
}

void HTMLPermissionElement::UpdateIcon(PermissionName permnission) {
  permission_internal_icon_->SetIcon(permnission, is_precise_location_);
}

void HTMLPermissionElement::UpdatePermissionStatusAndAppearance() {
  UpdatePermissionStatus();
  PseudoStateChanged(CSSSelector::kPseudoPermissionGranted);
  UpdateAppearance();
}

void HTMLPermissionElement::SetPreciseLocation(bool is_precise_location) {
  if (is_precise_location_ == is_precise_location) {
    return;
  }
  DisableClickingTemporarily(DisableReason::kAttributeChanged,
                             kDefaultDisableTimeout);
  is_precise_location_ = is_precise_location;
  UpdateAppearance();
}

mojom::blink::EmbeddedPermissionRequestDescriptorPtr
HTMLPermissionElement::CreateEmbeddedPermissionRequestDescriptor() {
  auto descriptor = EmbeddedPermissionRequestDescriptor::New();
  descriptor->element_position = BoundsInWidget();
  return descriptor;
}

void HTMLPermissionElement::UpdatePermissionStatus() {
  if (std::ranges::any_of(permission_status_map_, [](const auto& status) {
        return status.value == MojoPermissionStatus::DENIED;
      })) {
    aggregated_permission_status_ = MojoPermissionStatus::DENIED;
  } else if (std::ranges::any_of(
                 permission_status_map_, [](const auto& status) {
                   return status.value == MojoPermissionStatus::ASK;
                 })) {
    aggregated_permission_status_ = MojoPermissionStatus::ASK;
  } else {
    aggregated_permission_status_ = MojoPermissionStatus::GRANTED;
  }

  if (!initial_aggregated_permission_status_.has_value()) {
    initial_aggregated_permission_status_ = aggregated_permission_status_;
  }
}

// static
Vector<PermissionDescriptorPtr>
HTMLPermissionElement::ParsePermissionDescriptorsForTesting(
    const AtomicString& type) {
  return ParsePermissionDescriptorsFromString(type);
}

// static
String HTMLPermissionElement::DisableReasonToString(DisableReason reason) {
  switch (reason) {
    case DisableReason::kRecentlyAttachedToLayoutTree:
      return "being recently attached to layout tree";
    case DisableReason::kIntersectionWithViewportChanged:
      return "intersection with viewport changed";
    case DisableReason::kIntersectionVisibilityOutOfViewPortOrClipped:
      return "intersection out of viewport or clipped";
    case DisableReason::kIntersectionVisibilityOccludedOrDistorted:
      return "intersection occluded or distorted";
    case DisableReason::kInvalidStyle:
      return "invalid style";
    case DisableReason::kAttributeChanged:
      return "an attribute changed";
    case DisableReason::kUnknown:
      NOTREACHED();
  }
}

// static
HTMLPermissionElement::UserInteractionDeniedReason
HTMLPermissionElement::DisableReasonToUserInteractionDeniedReason(
    DisableReason reason) {
  switch (reason) {
    case DisableReason::kRecentlyAttachedToLayoutTree:
      return UserInteractionDeniedReason::kRecentlyAttachedToLayoutTree;
    case DisableReason::kIntersectionWithViewportChanged:
      return UserInteractionDeniedReason::kIntersectionWithViewportChanged;
    case DisableReason::kIntersectionVisibilityOutOfViewPortOrClipped:
      return UserInteractionDeniedReason::
          kIntersectionVisibilityOutOfViewPortOrClipped;
    case DisableReason::kIntersectionVisibilityOccludedOrDistorted:
      return UserInteractionDeniedReason::
          kIntersectionVisibilityOccludedOrDistorted;
    case DisableReason::kInvalidStyle:
      return UserInteractionDeniedReason::kInvalidStyle;
    case DisableReason::kAttributeChanged:
      return UserInteractionDeniedReason::kAttributeChanged;
    case DisableReason::kUnknown:
      NOTREACHED();
  }
}

// static
AtomicString HTMLPermissionElement::DisableReasonToInvalidReasonString(
    DisableReason reason) {
  switch (reason) {
    case DisableReason::kRecentlyAttachedToLayoutTree:
      return AtomicString("recently_attached");
    case DisableReason::kIntersectionWithViewportChanged:
      return AtomicString("intersection_changed");
    case DisableReason::kIntersectionVisibilityOutOfViewPortOrClipped:
      return AtomicString("intersection_out_of_viewport_or_clipped");
    case DisableReason::kIntersectionVisibilityOccludedOrDistorted:
      return AtomicString("intersection_occluded_or_distorted");
    case DisableReason::kInvalidStyle:
      return AtomicString("style_invalid");
    case DisableReason::kAttributeChanged:
      return AtomicString("attribute_changed");
    case DisableReason::kUnknown:
      NOTREACHED();
  }
}

PermissionService* HTMLPermissionElement::GetPermissionService() {
  if (!permission_service_.is_bound()) {
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        permission_service_.BindNewPipeAndPassReceiver(GetTaskRunner()));
    permission_service_.set_disconnect_handler(
        BindOnce(&HTMLPermissionElement::OnPermissionServiceConnectionFailed,
                 WrapWeakPersistent(this)));
  }

  return permission_service_.get();
}

void HTMLPermissionElement::OnPermissionServiceConnectionFailed() {
  permission_service_.reset();
}

bool HTMLPermissionElement::MaybeRegisterPageEmbeddedPermissionControl() {
  if (embedded_permission_control_receiver_.is_bound()) {
    return true;
  }

  if (permission_descriptors_.empty()) {
    return false;
  }

  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame) {
    return false;
  }

  if (frame->IsInFencedFrameTree()) {
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::FencedFrameDisallowed,
        GetType(), /*is_warning=*/false);
    return false;
  }

  if (frame->IsCrossOriginToOutermostMainFrame() &&
      !GetExecutionContext()
           ->GetContentSecurityPolicy()
           ->HasEnforceFrameAncestorsDirectives()) {
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::
            CspFrameAncestorsMissing,
        GetType(), /*is_warning=*/false);
    return false;
  }

  for (const PermissionDescriptorPtr& descriptor : permission_descriptors_) {
    if (!GetExecutionContext()->IsFeatureEnabled(
            PermissionNameToPermissionsPolicyFeature(descriptor->name))) {
      AuditsIssue::ReportPermissionElementIssue(
          GetExecutionContext(), GetDomNodeId(),
          protocol::Audits::PermissionElementIssueTypeEnum::
              PermissionsPolicyBlocked,
          GetType(), /*is_warning=*/false,
          PermissionNameToString(descriptor->name));
      return false;
    }
  }

  if (!IsRenderered()) {
    return false;
  }

  mojo::PendingRemote<EmbeddedPermissionControlClient> client;
  embedded_permission_control_receiver_.Bind(
      client.InitWithNewPipeAndPassReceiver(), GetTaskRunner());
  CHECK(embedded_permission_control_receiver_.is_bound());
  GetPermissionService()->RegisterPageEmbeddedPermissionControl(
      mojo::Clone(permission_descriptors_),
      CreateEmbeddedPermissionRequestDescriptor(), std::move(client));
  return true;
}

void HTMLPermissionElement::EnsureUnregisterPageEmbeddedPermissionControl() {
  if (embedded_permission_control_receiver_.is_bound()) {
    embedded_permission_control_receiver_.reset();
  }

  is_registered_in_browser_process_ = false;
}

void HTMLPermissionElement::LangAttributeChanged() {
  UpdateAppearance();
  HTMLElement::LangAttributeChanged();
}

void HTMLPermissionElement::AttributeChanged(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kTypeAttr) {
    setType(params.new_value);
  }

  MaybeRegisterPageEmbeddedPermissionControl();

  if (params.name == html_names::kPreciselocationAttr) {
    SetPreciseLocation(params.new_value != nullptr);
  }

  HTMLElement::AttributeChanged(params);
}

void HTMLPermissionElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  permission_container_ = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  permission_container_->SetShadowPseudoId(
      shadow_element_names::kPseudoInternalPermissionContainer);
  root.AppendChild(permission_container_);
  permission_internal_icon_ =
      MakeGarbageCollected<HTMLPermissionIconElement>(GetDocument());
  permission_container_->AppendChild(permission_internal_icon_);
  permission_text_span_ = MakeGarbageCollected<HTMLSpanElement>(GetDocument());
  permission_text_span_->SetShadowPseudoId(
      shadow_element_names::kPseudoInternalPermissionTextSpan);
  permission_container_->AppendChild(permission_text_span_);
}

void HTMLPermissionElement::AdjustStyle(ComputedStyleBuilder& builder) {
  Element::AdjustStyle(builder);

  // As the permission element's type is invalid the permission element starts
  // behaving as an HTMLUnknownElement.
  if (fallback_mode_) {
    return;
  }

  builder.SetOutlineOffset(builder.OutlineOffset().ClampNegativeToZero());

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

  if (builder.GetFontDescription().WordSpacing() >
      kMaximumWordSpacingToFontSizeRatio * builder.FontSize()) {
    builder.SetWordSpacing(
        Length::Fixed(builder.FontSize() * kMaximumWordSpacingToFontSizeRatio));
  } else if (builder.GetFontDescription().WordSpacing() < 0) {
    builder.SetWordSpacing(Length::Fixed(0));
  }

  if (builder.GetFontDescription().LetterSpacing() >
      kMaximumLetterSpacingToFontSizeRatio * builder.FontSize()) {
    builder.SetLetterSpacing(Length::Fixed(
        builder.FontSize() * kMaximumLetterSpacingToFontSizeRatio));
  } else if (builder.GetFontDescription().LetterSpacing() <
             kMinimumLetterSpacingToFontSizeRatio * builder.FontSize()) {
    builder.SetLetterSpacing(Length::Fixed(
        builder.FontSize() * kMinimumLetterSpacingToFontSizeRatio));
  }

  builder.SetMinHeight(AdjustedBoundedLengthWrapper(
      builder.MinHeight(),
      /*lower_bound=*/builder.FontSize() * kMinLengthToFontSizeRatio,
      /*upper_bound=*/builder.FontSize() * kMaxLengthToFontSizeRatio,
      /*should_multiply_by_content_size=*/false));
  builder.SetMaxHeight(AdjustedBoundedLengthWrapper(
      builder.MaxHeight(),
      /*lower_bound=*/std::nullopt,
      /*upper_bound=*/builder.FontSize() * kMaxLengthToFontSizeRatio,
      /*should_multiply_by_content_size=*/false));

  builder.SetMinWidth(
      AdjustedBoundedLengthWrapper(builder.MinWidth(),
                                   /*lower_bound=*/kMinLengthToFontSizeRatio,
                                   /*upper_bound=*/kMaxLengthToFontSizeRatio,
                                   /*should_multiply_by_content_size=*/true));

  bool unlimited_width_allowed =
      IsBorderSufficientlyDistinctFromBackgroundColor(builder.CloneStyle());

  if (unlimited_width_allowed) {
    if (builder.PaddingRight().HasOnlyFixedAndPercent() &&
        !builder.PaddingRight().IsZero() &&
        builder.PaddingLeft() != builder.PaddingRight()) {
      AuditsIssue::ReportPermissionElementIssue(
          GetExecutionContext(), GetDomNodeId(),
          protocol::Audits::PermissionElementIssueTypeEnum::
              PaddingRightUnsupported,
          GetType(), /*is_warning=*/false);
    }
    builder.SetPaddingRight(builder.PaddingLeft());
  } else {
    builder.SetMaxWidth(AdjustedBoundedLengthWrapper(
        builder.MaxWidth(),
        /*lower_bound=*/std::nullopt, /*upper_bound=*/kMaxLengthToFontSizeRatio,
        /*should_multiply_by_content_size=*/true));

    // If width is set to auto and there is left padding specified, we will
    // respect the padding (up to a certain maximum), otherwise the padding has
    // no effect. We treat height and top/bottom padding similarly.
    if (builder.Width().IsAuto() &&
        builder.PaddingLeft().HasOnlyFixedAndPercent() &&
        !builder.PaddingLeft().IsZero()) {
      if (builder.PaddingRight().HasOnlyFixedAndPercent() &&
          !builder.PaddingRight().IsZero() &&
          builder.PaddingLeft() != builder.PaddingRight()) {
        AuditsIssue::ReportPermissionElementIssue(
            GetExecutionContext(), GetDomNodeId(),
            protocol::Audits::PermissionElementIssueTypeEnum::
                PaddingRightUnsupported,
            GetType(), /*is_warning=*/false);
      }

      builder.SetPaddingLeft(AdjustedBoundedLengthWrapper(
          builder.PaddingLeft(),
          /*lower_bound=*/std::nullopt,
          /*upper_bound=*/builder.FontSize() *
              kMaxHorizontalPaddingToFontSizeRatio,
          /*should_multiply_by_content_size=*/false));
      builder.SetPaddingRight(builder.PaddingLeft());
      builder.SetWidth(Length::FitContent());
    } else {
      builder.ResetPaddingLeft();
      builder.ResetPaddingRight();
    }
  }

  if (builder.Height().IsAuto() &&
      builder.PaddingTop().HasOnlyFixedAndPercent() &&
      !builder.PaddingTop().IsZero()) {
    if (builder.PaddingBottom().HasOnlyFixedAndPercent() &&
        !builder.PaddingBottom().IsZero() &&
        builder.PaddingTop() != builder.PaddingBottom()) {
      AuditsIssue::ReportPermissionElementIssue(
          GetExecutionContext(), GetDomNodeId(),
          protocol::Audits::PermissionElementIssueTypeEnum::
              PaddingBottomUnsupported,
          GetType(), /*is_warning=*/false);
    }
    builder.SetPaddingTop(AdjustedBoundedLengthWrapper(
        builder.PaddingTop(),
        /*lower_bound=*/std::nullopt,
        /*upper_bound=*/builder.FontSize() * kMaxVerticalPaddingToFontSizeRatio,
        /*should_multiply_by_content_size=*/false));
    builder.SetPaddingBottom(builder.PaddingTop());
    builder.SetHeight(Length::FitContent());
  } else {
    builder.ResetPaddingTop();
    builder.ResetPaddingBottom();
  }

  if (builder.BorderBottomWidth() >
      builder.FontSize() * kMaxBorderWidthToFontSizeRatio) {
    builder.SetBorderBottomWidth(builder.FontSize() *
                                 kMaxBorderWidthToFontSizeRatio);
  }
  if (builder.BorderTopWidth() >
      builder.FontSize() * kMaxBorderWidthToFontSizeRatio) {
    builder.SetBorderTopWidth(builder.FontSize() *
                              kMaxBorderWidthToFontSizeRatio);
  }
  if (builder.BorderLeftWidth() >
      builder.FontSize() * kMaxBorderWidthToFontSizeRatio) {
    builder.SetBorderLeftWidth(builder.FontSize() *
                               kMaxBorderWidthToFontSizeRatio);
  }
  if (builder.BorderRightWidth() >
      builder.FontSize() * kMaxBorderWidthToFontSizeRatio) {
    builder.SetBorderRightWidth(builder.FontSize() *
                                kMaxBorderWidthToFontSizeRatio);
  }

  // The radius is adjusted to be at most the hardcoded percentage.
  // However if all border radius are identical there is no need as it will
  // result in a "pill"-shape which is desired behavior. Applying these
  // restrictions would prevent this behavior.
  if (builder.BorderTopLeftRadius() != builder.BorderTopRightRadius() ||
      builder.BorderTopLeftRadius() != builder.BorderBottomLeftRadius() ||
      builder.BorderTopLeftRadius() != builder.BorderBottomRightRadius() ||
      builder.BorderTopLeftRadius().Height() !=
          builder.BorderTopLeftRadius().Width()) {
    builder.SetBorderTopLeftRadius(AdjustedPercentBoundedRadius(
        builder.BorderTopLeftRadius(), kDefaultMaxPercentRadiusWidth,
        kDefaultMaxPercentRadiusHeight));
    builder.SetBorderTopRightRadius(AdjustedPercentBoundedRadius(
        builder.BorderTopRightRadius(), kDefaultMaxPercentRadiusWidth,
        kDefaultMaxPercentRadiusHeight));
    builder.SetBorderBottomLeftRadius(AdjustedPercentBoundedRadius(
        builder.BorderBottomLeftRadius(), kDefaultMaxPercentRadiusWidth,
        kDefaultMaxPercentRadiusHeight));
    builder.SetBorderBottomRightRadius(AdjustedPercentBoundedRadius(
        builder.BorderBottomRightRadius(), kDefaultMaxPercentRadiusWidth,
        kDefaultMaxPercentRadiusHeight));
  }

  // The base `text-decoration` property must be reset for each `<permission>`
  // element. This prevents any `text-decoration` from a parent element from
  // being propagated to the `<permission>` element.
  builder.SetBaseTextDecorationData(nullptr);

  // Cursor only allows 'pointer' (default) and 'not-allowed'. No custom images.
  builder.ClearCursorList();
  if (builder.Cursor() != ECursor::kNotAllowed) {
    builder.SetCursor(ECursor::kPointer);
  }
  builder.SetCursorIsInherited(false);

  if (builder.BoxShadow()) {
    for (const auto& shadow : builder.BoxShadow()->Shadows()) {
      if (shadow.Style() == ShadowStyle::kInset) {
        AuditsIssue::ReportPermissionElementIssue(
            GetExecutionContext(), GetDomNodeId(),
            protocol::Audits::PermissionElementIssueTypeEnum::
                InsetBoxShadowUnsupported,
            GetType(), /*is_warning=*/false);
        builder.SetBoxShadow(Member<ShadowList>());
        break;
      }
    }
  }

  // These webkit-prefixed properties are not supported by the permission
  // element. But since they are inherited by default, they are passed through
  // to the internal permission text span, even if they're not on the list of
  // allowed CSS properties.
  // Reset them here to avoid any side effects.
  builder.ResetTextStrokeWidth();
  builder.ResetTextFillColor();
  builder.ResetTextStrokeColor();
}

void HTMLPermissionElement::DidRecalcStyle(const StyleRecalcChange change) {
  HTMLElement::DidRecalcStyle(change);

  if (fallback_mode_) {
    return;
  }

  if (!IsStyleValid()) {
    DisableClickingIndefinitely(DisableReason::kInvalidStyle);
    return;
  }
  EnableClickingAfterDelay(DisableReason::kInvalidStyle,
                           kDefaultDisableTimeout);
  gfx::Rect intersection_rect =
      ComputeIntersectionRectWithViewport(GetDocument().GetPage());
  if (intersection_rect_.has_value() &&
      intersection_rect_.value() != intersection_rect) {
    DisableClickingTemporarily(DisableReason::kIntersectionWithViewportChanged,
                               kDefaultDisableTimeout);
  }
  intersection_rect_ = intersection_rect;
}

void HTMLPermissionElement::HandleActivation(Event& event,
                                             base::OnceClosure on_success) {
  event.SetDefaultHandled();
  if (event.IsFullyTrusted() ||
      RuntimeEnabledFeatures::BypassPepcSecurityForTestingEnabled()) {
    // TODO(crbug.com/352496162): After confirming all permission requests
    // eventually call |OnEmbeddedPermissionsDecided|, block multiple
    // permission requests when one is in progress, instead of temporairly
    // disallowing them.
    if (pending_request_created_ &&
        base::TimeTicks::Now() - *pending_request_created_ <
            kDefaultDisableTimeout) {
      AuditsIssue::ReportPermissionElementIssue(
          GetExecutionContext(), GetDomNodeId(),
          protocol::Audits::PermissionElementIssueTypeEnum::RequestInProgress,
          GetType(), /*is_warning=*/false);
      RecordUserInteractionAccepted(false);
      return;
    }

    bool is_user_interaction_enabled = IsClickingEnabled();
    RecordUserInteractionAccepted(is_user_interaction_enabled);
    if (is_user_interaction_enabled) {
      std::move(on_success).Run();
    }
  } else {
    // For automated testing purposes this behavior can be overridden by
    // adding '--enable-features=BypassPepcSecurityForTesting' to the
    // command line when launching the browser.
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::UntrustedEvent,
        GetType(), /*is_warning=*/false);
    RecordUserInteractionAccepted(false);
    base::UmaHistogramEnumeration(
        "Blink.PermissionElement.UserInteractionDeniedReason",
        UserInteractionDeniedReason::kUntrustedEvent);
  }
}

void HTMLPermissionElement::DefaultEventHandler(Event& event) {
  if (fallback_mode_) {
    HTMLElement::DefaultEventHandler(event);
    return;
  }

  if (event.type() == event_type_names::kDOMActivate) {
    HandleActivation(
        event,
        blink::BindOnce(&HTMLPermissionElement::RequestPageEmbededPermissions,
                        WrapWeakPersistent(this)));
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
  pending_request_created_ = base::TimeTicks::Now();

  GetPermissionService()->RequestPageEmbeddedPermission(
      mojo::Clone(permission_descriptors_),
      CreateEmbeddedPermissionRequestDescriptor(),
      BindOnce(&HTMLPermissionElement::OnEmbeddedPermissionsDecided,
               WrapWeakPersistent(this)));
}

void HTMLPermissionElement::OnPermissionStatusChange(
    PermissionName permission_name,
    MojoPermissionStatus status) {
  auto it = permission_status_map_.find(permission_name);
  CHECK(it != permission_status_map_.end());
  it->value = status;

  UpdatePermissionStatusAndAppearance();
}

void HTMLPermissionElement::OnEmbeddedPermissionControlRegistered(
    bool allowed,
    const std::optional<Vector<MojoPermissionStatus>>& statuses) {
  if (!allowed) {
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::RegistrationFailed,
        GetType(), /*is_warning=*/false);
    return;
  }

  CHECK_GT(permission_descriptors_.size(), 0U);
  CHECK_LE(permission_descriptors_.size(), 2U);
  CHECK(statuses.has_value());
  CHECK_EQ(statuses->size(), permission_descriptors_.size());

  is_registered_in_browser_process_ = true;
  for (wtf_size_t i = 0; i < permission_descriptors_.size(); ++i) {
    auto status = (*statuses)[i];
    const auto& descriptor = permission_descriptors_[i];
    permission_status_map_.Set(descriptor->name, status);
  }

  UpdatePermissionStatusAndAppearance();
  MaybeDispatchValidationChangeEvent();
}

void HTMLPermissionElement::OnEmbeddedPermissionsDecided(
    EmbeddedPermissionControlResult result) {
  pending_request_created_ = std::nullopt;

  switch (result) {
    case EmbeddedPermissionControlResult::kDismissed:
      DispatchEvent(
          *Event::CreateCancelableBubble(event_type_names::kPromptdismiss));
      return;
    case EmbeddedPermissionControlResult::kGranted:
      aggregated_permission_status_ = MojoPermissionStatus::GRANTED;
      DispatchEvent(
          *Event::CreateCancelableBubble(event_type_names::kPromptaction));
      return;
    case EmbeddedPermissionControlResult::kDenied:
      DispatchEvent(
          *Event::CreateCancelableBubble(event_type_names::kPromptaction));
      return;
    case EmbeddedPermissionControlResult::kNotSupported:
      AuditsIssue::ReportPermissionElementIssue(
          GetExecutionContext(), GetDomNodeId(),
          protocol::Audits::PermissionElementIssueTypeEnum::TypeNotSupported,
          GetType(), /*is_warning=*/false);
      return;
    case EmbeddedPermissionControlResult::kResolvedNoUserGesture:
      return;
  }
  NOTREACHED();
}

void HTMLPermissionElement::DisableReasonExpireTimerFired(TimerBase* timer) {
  EnableClicking(static_cast<DisableReasonExpireTimer*>(timer)->reason());
}

void HTMLPermissionElement::MaybeDispatchValidationChangeEvent() {
  auto state = GetClickingEnabledState();
  if (clicking_enabled_state_ == state) {
    return;
  }

  // Always keep `clicking_enabled_state_` up-to-date
  clicking_enabled_state_ = state;
  EnqueueEvent(
      *Event::CreateCancelableBubble(event_type_names::kValidationstatuschange),
      TaskType::kDOMManipulation);
}

scoped_refptr<base::SingleThreadTaskRunner>
HTMLPermissionElement::GetTaskRunner() {
  return GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault);
}

bool HTMLPermissionElement::IsClickingEnabled() {
  if (permission_descriptors_.empty()) {
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::InvalidTypeActivation,
        GetType(), /*is_warning=*/false);
    base::UmaHistogramEnumeration(
        "Blink.PermissionElement.UserInteractionDeniedReason",
        UserInteractionDeniedReason::kInvalidType);
    return false;
  }

  // Do not check click-disabling reasons if the PEPC validation feature is
  // disabled. This should only occur in testing scenarios.
  if (RuntimeEnabledFeatures::BypassPepcSecurityForTestingEnabled()) {
    return true;
  }

  if (!is_registered_in_browser_process_) {
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::SecurityChecksFailed,
        GetType(), /*is_warning=*/false);
    base::UmaHistogramEnumeration(
        "Blink.PermissionElement.UserInteractionDeniedReason",
        UserInteractionDeniedReason::kFailedOrHasNotBeenRegistered);
    return false;
  }

  // Remove expired reasons. If the remaining map is not empty, clicking is
  // disabled. Record and log all the remaining reasons in the map in this case.
  base::TimeTicks now = base::TimeTicks::Now();
  clicking_disabled_reasons_.erase_if(
      [&now](const auto& it) { return it.value < now; });

  for (const auto& it : clicking_disabled_reasons_) {
    ReportActivationDisabledAuditsIssue(it.key);

    base::UmaHistogramEnumeration(
        "Blink.PermissionElement.UserInteractionDeniedReason",
        DisableReasonToUserInteractionDeniedReason(it.key));
  }

  return clicking_disabled_reasons_.empty();
}

void HTMLPermissionElement::DisableClickingIndefinitely(DisableReason reason) {
  clicking_disabled_reasons_.Set(reason, base::TimeTicks::Max());
  StopTimerDueToIndefiniteReason(reason);
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

  // An active timer indicates that the element is temporarily disabled with a
  // reason, which is the longest alive temporary reason in
  // `clicking_disabled_reasons_`. If the timer's next fire time is less than
  // the `timeout_time` (`NextFireInterval() < duration`), a new "longest alive
  // temporary reason" emerges and we need an adjustment to the timer.
  clicking_disabled_reasons_.Set(reason, timeout_time);
  if (!disable_reason_expire_timer_.IsActive() ||
      disable_reason_expire_timer_.NextFireInterval() < duration) {
    disable_reason_expire_timer_.StartOrRestartWithReason(reason, duration);
  }

  MaybeDispatchValidationChangeEvent();
}

void HTMLPermissionElement::EnableClicking(DisableReason reason) {
  clicking_disabled_reasons_.erase(reason);
  RefreshDisableReasonsAndUpdateTimer();
}

void HTMLPermissionElement::EnableClickingAfterDelay(
    DisableReason reason,
    const base::TimeDelta& delay) {
  if (clicking_disabled_reasons_.Contains(reason)) {
    clicking_disabled_reasons_.Set(reason, base::TimeTicks::Now() + delay);
    RefreshDisableReasonsAndUpdateTimer();
  }
}

HTMLPermissionElement::ClickingEnabledState
HTMLPermissionElement::GetClickingEnabledState() const {
  if (fallback_mode_) {
    return {false, AtomicString("type_invalid")};
  }

  if (LocalFrame* frame = GetDocument().GetFrame()) {
    if (frame->IsInFencedFrameTree()) {
      return {false, AtomicString("illegal_subframe")};
    }

    if (frame->IsCrossOriginToOutermostMainFrame() &&
        !GetExecutionContext()
             ->GetContentSecurityPolicy()
             ->HasEnforceFrameAncestorsDirectives()) {
      return {false, AtomicString("illegal_subframe")};
    }

    for (const PermissionDescriptorPtr& descriptor : permission_descriptors_) {
      if (!GetExecutionContext()->IsFeatureEnabled(
              PermissionNameToPermissionsPolicyFeature(descriptor->name))) {
        return {false, AtomicString("illegal_subframe")};
      }
    }
  }

  if (!is_registered_in_browser_process_) {
    return {false, AtomicString("unsuccessful_registration")};
  }

  if (RuntimeEnabledFeatures::BypassPepcSecurityForTestingEnabled()) {
    return {true, AtomicString()};
  }

  // If there's an "indefinitely disabling" for any reason, return that reason.
  // Otherwise, we will look into the reason of the current active timer.
  for (const auto& it : clicking_disabled_reasons_) {
    if (it.value == base::TimeTicks::Max()) {
      return {false, DisableReasonToInvalidReasonString(it.key)};
    }
  }

  if (disable_reason_expire_timer_.IsActive()) {
    return {false, DisableReasonToInvalidReasonString(
                       disable_reason_expire_timer_.reason())};
  }

  return {true, AtomicString()};
}

void HTMLPermissionElement::RefreshDisableReasonsAndUpdateTimer() {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks max_time_ticks = base::TimeTicks::Min();
  DisableReason reason = DisableReason::kUnknown;
  HashMap<DisableReason, base::TimeTicks> swap_clicking_disabled_reasons;
  for (auto it = clicking_disabled_reasons_.begin();
       it != clicking_disabled_reasons_.end(); ++it) {
    if (it->value == base::TimeTicks::Max()) {
      StopTimerDueToIndefiniteReason(it->key);
      return;
    }

    if (it->value < now) {
      continue;
    }

    swap_clicking_disabled_reasons.Set(it->key, it->value);
    if (it->value <= max_time_ticks) {
      continue;
    }

    max_time_ticks = it->value;
    reason = it->key;
  }
  // Restart the timer to match with  "longest alive, not indefinitely disabling
  // reason". That's the one has the max timeticks on
  // `clicking_disabled_reasons_`.
  if (max_time_ticks != base::TimeTicks::Min()) {
    disable_reason_expire_timer_.StartOrRestartWithReason(reason,
                                                          max_time_ticks - now);
  }

  clicking_disabled_reasons_.swap(swap_clicking_disabled_reasons);
  MaybeDispatchValidationChangeEvent();
}

void HTMLPermissionElement::OnIntersectionChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  CHECK(!entries.empty());
  Member<IntersectionObserverEntry> latest_observation = entries.back();
  CHECK_EQ(this, latest_observation->target());
  IntersectionVisibility new_intersection_visibility =
      IntersectionVisibility::kFullyVisible;
  // `intersectionRatio` >= `kIntersectionThreshold` (1.0f) means the element is
  // fully visible on the viewport (vs `intersectionRatio` < 1.0f means its
  // bound is clipped by the viewport or styling effects). In this case, the
  // `isVisible` false means the element is occluded by something else or has
  // distorted visual effect applied.
  // Note: It's unlikely we'll encounter an empty target rectangle (height or
  // width is 0), but if it happens, we can consider the element as visible.
  if (!latest_observation->isVisible() &&
      !latest_observation->GetGeometry().TargetRect().IsEmpty()) {
    new_intersection_visibility =
        latest_observation->intersectionRatio() >= kIntersectionThreshold
            ? IntersectionVisibility::kOccludedOrDistorted
            : IntersectionVisibility::kOutOfViewportOrClipped;
  }

  if (intersection_visibility_ == new_intersection_visibility) {
    return;
  }

  intersection_visibility_ = new_intersection_visibility;
  occluder_node_id_ = kInvalidDOMNodeId;
  switch (intersection_visibility_) {
    case IntersectionVisibility::kFullyVisible: {
      std::optional<base::TimeDelta> recently_attached_timeout_remaining =
          GetRecentlyAttachedTimeoutRemaining();
      base::TimeDelta interval =
          recently_attached_timeout_remaining
              ? recently_attached_timeout_remaining.value()
              : kDefaultDisableTimeout;
      EnableClickingAfterDelay(
          DisableReason::kIntersectionVisibilityOccludedOrDistorted, interval);
      EnableClickingAfterDelay(
          DisableReason::kIntersectionVisibilityOutOfViewPortOrClipped,
          interval);
      break;
    }
    case IntersectionVisibility::kOccludedOrDistorted:
      occluder_node_id_ = latest_observation->GetGeometry().occluder_node_id();
      DisableClickingIndefinitely(
          DisableReason::kIntersectionVisibilityOccludedOrDistorted);
      break;
    case IntersectionVisibility::kOutOfViewportOrClipped:
      DisableClickingIndefinitely(
          DisableReason::kIntersectionVisibilityOutOfViewPortOrClipped);
      break;
  }
}

bool HTMLPermissionElement::IsMaskedByAncestor() const {
  if (LayoutObject* layout_object = GetLayoutObject()) {
    for (PaintLayer* layer = layout_object->EnclosingLayer(); layer;
         layer = layer->Parent()) {
      if (layer->GetLayoutObject().HasMask()) {
        return true;
      }
    }
  }
  return false;
}

bool HTMLPermissionElement::IsStyleValid() {
  if (IsMaskedByAncestor()) {
    return false;
  }

  const ComputedStyle* style = GetComputedStyle();

  // No computed style when using `display: none`.
  if (!style) {
    base::UmaHistogramEnumeration("Blink.PermissionElement.InvalidStyleReason",
                                  InvalidStyleReason::kNoComputedStyle);
    return false;
  }

  if (base::Contains(kInvalidDisplayStyles,
                     style->GetDisplayStyle().Display()) ||
      style->IsDisplayTableType()) {
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::InvalidDisplayStyle,
        GetType(), /*is_warning=*/true);
    base::UmaHistogramEnumeration("Blink.PermissionElement.InvalidStyleReason",
                                  InvalidStyleReason::kInvalidDisplayProperty);
    return false;
  }

  if (AreColorsNonOpaque(style)) {
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::NonOpaqueColor,
        GetType(), /*is_warning=*/true);
    base::UmaHistogramEnumeration(
        "Blink.PermissionElement.InvalidStyleReason",
        InvalidStyleReason::kNonOpaqueColorOrBackgroundColor);
    return false;
  }

  if (ContrastBetweenColorAndBackgroundColor(style) < kMinimumAllowedContrast) {
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::LowContrast,
        GetType(), /*is_warning=*/true);
    base::UmaHistogramEnumeration(
        "Blink.PermissionElement.InvalidStyleReason",
        InvalidStyleReason::kLowConstrastColorAndBackgroundColor);
    return false;
  }

  // Compute the font size but reverse browser zoom as it should not affect font
  // size validation. The same font size value should always pass regardless of
  // what the user's browser zoom is or the device-level viewport zoom.
  //
  // However critically css zoom should still be part of the final computed font
  // size (as that is controlled by the site) so we cancel the css zoom factor
  // out of the layout zoom factor.

  float non_css_layout_zoom_factor =
      GetDocument().GetFrame()->LocalFrameRoot().LayoutZoomFactor() /
      GetDocument().GetFrame()->LocalFrameRoot().CssZoomFactor();

  float font_size_dip = style->ComputedFontSize() / non_css_layout_zoom_factor;

  bool is_font_monospace = style->GetFontDescription().IsMonospace();

  // The min size is what `font-size:small` looks like when rendered in the
  // document element of the local root frame, without any intervening
  // zoom factors applied.
  float min_font_size_dip = FontSizeFunctions::FontSizeForKeyword(
      &GetDocument(), FontSizeFunctions::KeywordSize(CSSValueID::kSmall),
      is_font_monospace);
  if (font_size_dip < std::min(min_font_size_dip, kDefaultSmallFontSize)) {
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::FontSizeTooSmall,
        GetType(), /*is_warning=*/true);
    base::UmaHistogramEnumeration("Blink.PermissionElement.InvalidStyleReason",
                                  InvalidStyleReason::kTooSmallFontSize);
    return false;
  }

  // The max size is what `font-size:xxxlarge` looks like when rendered in the
  // document element of the local root frame, without any intervening
  // zoom factors applied.
  float max_font_size_dip = FontSizeFunctions::FontSizeForKeyword(
      &GetDocument(), FontSizeFunctions::KeywordSize(CSSValueID::kXxxLarge),
      is_font_monospace);
  if (font_size_dip > std::max(max_font_size_dip, kDefaultXxxLargeFontSize)) {
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::FontSizeTooLarge,
        GetType(), /*is_warning=*/true);
    base::UmaHistogramEnumeration("Blink.PermissionElement.InvalidStyleReason",
                                  InvalidStyleReason::kTooLargeFontSize);
    return false;
  }

  return true;
}

Length HTMLPermissionElement::AdjustedBoundedLengthWrapper(
    const Length& length,
    std::optional<float> lower_bound,
    std::optional<float> upper_bound,
    bool should_multiply_by_content_size) {
  CHECK(lower_bound.has_value() || upper_bound.has_value());
  bool is_content_or_stretch =
      length.HasContentOrIntrinsic() || length.HasStretch();
  if (is_content_or_stretch && !length_console_error_sent_) {
    length_console_error_sent_ = true;
    AuditsIssue::ReportPermissionElementIssue(
        GetExecutionContext(), GetDomNodeId(),
        protocol::Audits::PermissionElementIssueTypeEnum::InvalidSizeValue,
        GetType(), /*is_warning=*/true);
  }
  return HTMLPermissionElementUtils::AdjustedBoundedLength(
      length, lower_bound, upper_bound, should_multiply_by_content_size);
}

LengthSize HTMLPermissionElement::AdjustedPercentBoundedRadius(
    const LengthSize& length_size,
    float width_percent_bound,
    float height_percent_bound) {
  LengthSize adjusted_length_size;

  auto* width_upper_bound_expr =
      MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(0, width_percent_bound,
                           /*has_explicit_pixels=*/false,
                           /*has_explicit_percent=*/true));
  adjusted_length_size.SetWidth(Length(CalculationValue::CreateSimplified(
      HTMLPermissionElementUtils::BuildLengthBoundExpr(
          length_size.Width(), nullptr, width_upper_bound_expr),
      Length::ValueRange::kNonNegative)));

  auto* height_upper_bound_expr =
      MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(0, height_percent_bound,
                           /*has_explicit_pixels=*/false,
                           /*has_explicit_percent=*/true));
  adjusted_length_size.SetHeight(Length(CalculationValue::CreateSimplified(
      HTMLPermissionElementUtils::BuildLengthBoundExpr(
          length_size.Height(), nullptr, height_upper_bound_expr),
      Length::ValueRange::kNonNegative)));

  return adjusted_length_size;
}

void HTMLPermissionElement::DidFinishLifecycleUpdate(
    const LocalFrameView& local_frame_view) {
  // This code monitors the stability of the HTMLPermissionElement and
  // temporarily disables the element if it detects an unstable state.
  // "Unstable state" in this context occurs when the intersection rectangle
  // between the viewport and the element's layout box changes, indicating that
  // the element has been moved or resized.
  gfx::Rect intersection_rect = ComputeIntersectionRectWithViewport(
      local_frame_view.GetFrame().GetPage());
  if (intersection_rect_.has_value() &&
      intersection_rect_.value() != intersection_rect) {
    DisableClickingTemporarily(DisableReason::kIntersectionWithViewportChanged,
                               kDefaultDisableTimeout);
  }
  intersection_rect_ = intersection_rect;

  if (IsRenderered()) {
    MaybeRegisterPageEmbeddedPermissionControl();
  } else {
    EnsureUnregisterPageEmbeddedPermissionControl();
  }
}

Vector<PermissionDescriptorPtr> HTMLPermissionElement::ParseType(
    const AtomicString& type) {
  return ParsePermissionDescriptorsFromString(type);
}

gfx::Rect HTMLPermissionElement::ComputeIntersectionRectWithViewport(
    const Page* page) {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object) {
    return gfx::Rect();
  }

  gfx::Rect viewport_in_root_frame =
      ToEnclosingRect(page->GetVisualViewport().VisibleRect());

  LayoutBox* layout_box = DynamicTo<LayoutBox>(layout_object);
  if (!layout_box) {
    return gfx::Rect();
  }

  PhysicalRect rect = layout_box->PhysicalBorderBoxRect();
  // `MapToVisualRectInAncestorSpace` with a null `ancestor` argument will
  // mutate `rect` to visible rect in the root frame's coordinate space.
  layout_object->MapToVisualRectInAncestorSpace(/*ancestor*/ nullptr, rect);
  return IntersectRects(viewport_in_root_frame, ToEnclosingRect(rect));
}

std::optional<base::TimeDelta>
HTMLPermissionElement::GetRecentlyAttachedTimeoutRemaining() const {
  base::TimeTicks now = base::TimeTicks::Now();
  auto it = clicking_disabled_reasons_.find(
      DisableReason::kRecentlyAttachedToLayoutTree);
  if (it == clicking_disabled_reasons_.end()) {
    return std::nullopt;
  }

  return it->value - now;
}

void HTMLPermissionElement::EnableFallbackMode() {
  CHECK(!fallback_mode_);
  fallback_mode_ = true;
  if (intersection_observer_) {
    intersection_observer_->unobserve(this);
  }
  // Adding this slot element will make all children of the permission element
  // render, the permission element's built-in elements are removed at the same
  // time.
  UserAgentShadowRoot()->AppendChild(
      MakeGarbageCollected<HTMLSlotElement>(GetDocument()));
  UserAgentShadowRoot()->RemoveChild(permission_container_);
  MaybeDispatchValidationChangeEvent();
}

void HTMLPermissionElement::ReportActivationDisabledAuditsIssue(
    DisableReason reason) {
  String disableReason = DisableReasonToString(reason);
  String occluderNodeInfo = String();
  String occluderParentNodeInfo = String();

  if (reason == DisableReason::kIntersectionVisibilityOccludedOrDistorted &&
      occluder_node_id_ != kInvalidDOMNodeId) {
    Node* node = DOMNodeIds::NodeForId(occluder_node_id_);
    if (node) {
      occluderNodeInfo = node->ToString();

      auto* element = DynamicTo<Element>(node);
      if (!element || (!element->HasID() && !element->HasClass())) {
        // Printing parent node might give some useful information if there's no
        // id or class attr.
        if (Node* parent = node->parentNode()) {
          occluderParentNodeInfo = parent->ToString();
        }
      }
    }
  }

  AuditsIssue::ReportPermissionElementIssue(
      GetExecutionContext(), GetDomNodeId(),
      protocol::Audits::PermissionElementIssueTypeEnum::ActivationDisabled,
      GetType(), /*is_warning=*/false, /*permissionName=*/String(),
      occluderNodeInfo, occluderParentNodeInfo, disableReason);
}

}  // namespace blink
