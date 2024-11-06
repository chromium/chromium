// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
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
#include "third_party/blink/renderer/core/html/html_permission_element_strings_map.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/page/page.h"
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
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
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
constexpr int kMaxLengthToFontSizeRatio = 3;
constexpr int kMinLengthToFontSizeRatio = 1;
constexpr int kMaxVerticalPaddingToFontSizeRatio = 1;
constexpr int kMaxHorizontalPaddingToFontSizeRatio = 5;
// Needed to avoid IntersectionObserver false-positives caused by other elements
// being too close.
constexpr int kMinMargin = 4;
constexpr float kIntersectionThreshold = 1.0f;

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

int GetTranslatedMessageID(int message_id,
                           const AtomicString& language_string) {
  DCHECK(language_string.IsLowerASCII());
  DEFINE_STATIC_LOCAL(GeneratedMessagesMap, generated_message_ids, ());
  if (language_string.empty()) {
    return message_id;
  }

  if (generated_message_ids.empty()) {
    FillInPermissionElementTranslationsMap(generated_message_ids);
  }

  const auto language_map_itr = generated_message_ids.find(language_string);
  if (language_map_itr != generated_message_ids.end()) {
    const auto& language_map = language_map_itr->value;
    const auto translated_message_itr = language_map.find(message_id);
    if (translated_message_itr != language_map.end()) {
      return translated_message_itr->value;
    }
  }

  Vector<String> parts;
  language_string.GetString().Split('-', parts);

  if (parts.size() == 0) {
    return message_id;
  }
  // This is to support locales with unknown combination of languages and
  // countries. If the combination of language and country is not known,
  // the code will fallback to strings just from the language part of the
  // locale.
  // Eg: en-au is a unknown combination, in this case we will fall back to
  // en strings.
  if (generated_message_ids.Contains(parts[0])) {
    const auto& language_map = generated_message_ids.find(parts[0])->value;
    if (language_map.Contains(message_id)) {
      return language_map.find(message_id)->value;
    }
  }
  return message_id;
}

// Helper to get permission text resource ID for the given map which has only
// one element.
int GetUntranslatedMessageIDSinglePermission(PermissionName name,
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
int GetUntranslatedMessageIDMultiplePermissions(bool granted) {
  return granted ? IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE_ALLOWED
                 : IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE;
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

// Build an expression that is equivalent to `size * |factor|)`. To be used
// inside a `calc-size` expression.
scoped_refptr<const CalculationExpressionNode> BuildFitContentExpr(
    float factor) {
  auto constant_expr =
      base::MakeRefCounted<CalculationExpressionNumberNode>(factor);
  auto size_expr = base::MakeRefCounted<CalculationExpressionSizingKeywordNode>(
      CalculationExpressionSizingKeywordNode::Keyword::kSize);
  return CalculationExpressionOperationNode::CreateSimplified(
      CalculationExpressionOperationNode::Children({constant_expr, size_expr}),
      CalculationOperator::kMultiply);
}

// Builds an expression that takes a |length| and bounds it lower, higher, or on
// both sides with the provided expressions.
scoped_refptr<const CalculationExpressionNode> BuildLengthBoundExpr(
    const Length& length,
    std::optional<scoped_refptr<const CalculationExpressionNode>>
        lower_bound_expr,
    std::optional<scoped_refptr<const CalculationExpressionNode>>
        upper_bound_expr) {
  if (lower_bound_expr.has_value() && upper_bound_expr.has_value()) {
    return CalculationExpressionOperationNode::CreateSimplified(
        CalculationExpressionOperationNode::Children(
            {lower_bound_expr.value(),
             length.AsCalculationValue()->GetOrCreateExpression(),
             upper_bound_expr.value()}),
        CalculationOperator::kClamp);
  }

  if (lower_bound_expr.has_value()) {
    return CalculationExpressionOperationNode::CreateSimplified(
        CalculationExpressionOperationNode::Children(
            {lower_bound_expr.value(),
             length.AsCalculationValue()->GetOrCreateExpression()}),
        CalculationOperator::kMax);
  }

  if (upper_bound_expr.has_value()) {
    return CalculationExpressionOperationNode::CreateSimplified(
        CalculationExpressionOperationNode::Children(
            {upper_bound_expr.value(),
             length.AsCalculationValue()->GetOrCreateExpression()}),
        CalculationOperator::kMin);
  }

  NOTREACHED();
}

}  // namespace

HTMLPermissionElement::HTMLPermissionElement(Document& document)
    : HTMLElement(html_names::kPermissionTag, document),
      ScrollSnapshotClient(GetDocument().GetFrame()),
      permission_service_(document.GetExecutionContext()),
      permission_observer_receivers_(this, document.GetExecutionContext()),
      embedded_permission_control_receiver_(this,
                                            document.GetExecutionContext()),
      disable_reason_expire_timer_(
          this,
          &HTMLPermissionElement::DisableReasonExpireTimerFired) {
  DCHECK(RuntimeEnabledFeatures::PermissionElementEnabled(
      document.GetExecutionContext()));
  SetHasCustomStyleCallbacks();
  intersection_observer_ = IntersectionObserver::Create(
      GetDocument(),
      WTF::BindRepeating(&HTMLPermissionElement::OnIntersectionChanged,
                         WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kPermissionElementIntersectionObserver,
      IntersectionObserver::Params{
          .thresholds = {kIntersectionThreshold},
          .semantics = IntersectionObserver::kFractionOfTarget,
          .behavior = IntersectionObserver::kDeliverDuringPostLifecycleSteps,
          .delay = base::Milliseconds(100),
          .track_visibility = true,
          .expose_occluder_id = true,
      });

  intersection_observer_->observe(this);
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
  visitor->Trace(permission_observer_receivers_);
  visitor->Trace(embedded_permission_control_receiver_);
  visitor->Trace(permission_text_span_);
  visitor->Trace(intersection_observer_);
  visitor->Trace(disable_reason_expire_timer_);
  HTMLElement::Trace(visitor);
}

void HTMLPermissionElement::OnPermissionStatusInitialized(
    PermissionStatusMap initilized_map) {
  permission_status_map_ = std::move(initilized_map);
  UpdatePermissionStatusAndAppearance();
}

Node::InsertionNotificationRequest HTMLPermissionElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  MaybeRegisterPageEmbeddedPermissionControl();
  return kInsertionDone;
}

void HTMLPermissionElement::AttachLayoutTree(AttachContext& context) {
  Element::AttachLayoutTree(context);
  DisableClickingTemporarily(DisableReason::kRecentlyAttachedToLayoutTree,
                             kDefaultDisableTimeout);
  CHECK(GetDocument().View());
  GetDocument().View()->RegisterForLifecycleNotifications(this);
}

void HTMLPermissionElement::DetachLayoutTree(bool performing_reattach) {
  Element::DetachLayoutTree(performing_reattach);
  if (auto* view = GetDocument().View()) {
    view->UnregisterFromLifecycleNotifications(this);
  }
}

void HTMLPermissionElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  // We also need to remove all permission observer receivers from the set, to
  // effectively stop listening the permission status change events.
  permission_observer_receivers_.Clear();
  permission_status_map_.clear();
  aggregated_permission_status_ = std::nullopt;
  pseudo_state_ = {/*has_invalid_style*/ false, /*is_occluded*/ false};
  if (disable_reason_expire_timer_.IsActive()) {
    disable_reason_expire_timer_.Stop();
  }
  intersection_rect_ = std::nullopt;
  if (embedded_permission_control_receiver_.is_bound()) {
    embedded_permission_control_receiver_.reset();
  }

  if (LocalDOMWindow* window = GetDocument().domWindow()) {
    CachedPermissionStatus::From(window)->UnregisterClient(
        this, permission_descriptors_);
  }
}

void HTMLPermissionElement::Focus(const FocusParams& params) {
  // This will only apply to `focus` and `blur` JS API. Other focus types (like
  // accessibility focusing and manual user focus), will still be permitted as
  // usual.
  if (params.type == mojom::blink::FocusType::kScript) {
    return;
  }

  HTMLElement::Focus(params);
}

FocusableState HTMLPermissionElement::SupportsFocus(UpdateBehavior) const {
  // The permission element is only focusable if it has a valid type.
  return permission_descriptors_.empty() ? FocusableState::kNotFocusable
                                         : FocusableState::kFocusable;
}

int HTMLPermissionElement::DefaultTabIndex() const {
  // The permission element behaves similarly to a button and therefore is
  // focusable via keyboard by default.
  return 0;
}

CascadeFilter HTMLPermissionElement::GetCascadeFilter() const {
  // Reject all properties for which 'kValidForPermissionElement' is false.
  return CascadeFilter(CSSProperty::kValidForPermissionElement, false);
}

bool HTMLPermissionElement::CanGeneratePseudoElement(PseudoId id) const {
  switch (id) {
    case PseudoId::kPseudoIdAfter:
    case PseudoId::kPseudoIdBefore:
    case PseudoId::kPseudoIdCheck:
    case PseudoId::kPseudoIdSelectArrow:
      return false;
    default:
      return Element::CanGeneratePseudoElement(id);
  }
}

bool HTMLPermissionElement::HasInvalidStyle() const {
  return IsClickingDisabledIndefinitely(DisableReason::kInvalidStyle);
}

bool HTMLPermissionElement::IsOccluded() const {
  return !GetRecentlyAttachedTimeoutRemaining() &&
         IsClickingDisabledIndefinitely(
             DisableReason::kIntersectionVisibilityOccludedOrDistorted);
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
    case DisableReason::kIntersectionRecentlyFullyVisible:
      return "being recently fully visible";
    case DisableReason::kIntersectionWithViewportChanged:
      return "intersection with viewport changed";
    case DisableReason::kIntersectionVisibilityOutOfViewPortOrClipped:
      return "intersection out of viewport or clipped";
    case DisableReason::kIntersectionVisibilityOccludedOrDistorted:
      return "intersection occluded or distorted";
    case DisableReason::kInvalidStyle:
      return "invalid style";
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
    case DisableReason::kIntersectionRecentlyFullyVisible:
      return UserInteractionDeniedReason::kIntersectionRecentlyFullyVisible;
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
    case DisableReason::kIntersectionRecentlyFullyVisible:
      return AtomicString("intersection_visible");
    case DisableReason::kIntersectionWithViewportChanged:
      return AtomicString("intersection_changed");
    case DisableReason::kIntersectionVisibilityOutOfViewPortOrClipped:
      return AtomicString("intersection_out_of_viewport_or_clipped");
    case DisableReason::kIntersectionVisibilityOccludedOrDistorted:
      return AtomicString("intersection_occluded_or_distorted");
    case DisableReason::kInvalidStyle:
      return AtomicString("style_invalid");
    case DisableReason::kUnknown:
      NOTREACHED();
  }
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
    AddConsoleError(
        String::Format("The permission '%s' is not allowed in fenced frame",
                       GetType().Utf8().c_str()));
    return false;
  }

  if (frame->IsCrossOriginToOutermostMainFrame() &&
      !GetExecutionContext()
           ->GetContentSecurityPolicy()
           ->HasEnforceFrameAncestorsDirectives()) {
    AddConsoleError(
        String::Format("The permission '%s' is not allowed without the CSP "
                       "'frame-ancestors' directive present.",
                       GetType().Utf8().c_str()));
    return false;
  }

  for (const PermissionDescriptorPtr& descriptor : permission_descriptors_) {
    if (!GetExecutionContext()->IsFeatureEnabled(
            PermissionNameToPermissionsPolicyFeature(descriptor->name))) {
      AddConsoleError(String::Format(
          "The permission '%s' is not allowed in the current context due to "
          "PermissionsPolicy",
          PermissionNameToString(descriptor->name).Utf8().c_str()));
      return false;
    }
  }

  CachedPermissionStatus::From(GetDocument().domWindow())
      ->RegisterClient(this, permission_descriptors_);
  mojo::PendingRemote<EmbeddedPermissionControlClient> client;
  embedded_permission_control_receiver_.Bind(
      client.InitWithNewPipeAndPassReceiver(), GetTaskRunner());
  GetPermissionService()->RegisterPageEmbeddedPermissionControl(
      mojo::Clone(permission_descriptors_), std::move(client));
  return true;
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
    if (permission_descriptors_.empty()) {
      AddConsoleError("The permission type '" + GetType().GetString() +
                      "' is not supported by the "
                      "permission element.");
      return;
    }

    CHECK_LE(permission_descriptors_.size(), 2U)
        << "Unexpected permissions size " << permission_descriptors_.size();
  }

  MaybeRegisterPageEmbeddedPermissionControl();

  if (params.name == html_names::kPreciselocationAttr) {
    // This attribute can only be set once, and can not be modified afterwards.
    if (is_precise_location_) {
      return;
    }

    is_precise_location_ = true;
    UpdateText();
  }

  if (params.name == html_names::kLangAttr) {
    UpdateText();
  }

  HTMLElement::AttributeChanged(params);
}

void HTMLPermissionElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  permission_text_span_ = MakeGarbageCollected<HTMLSpanElement>(GetDocument());
  permission_text_span_->SetShadowPseudoId(
      shadow_element_names::kPseudoInternalPermissionTextSpan);
  root.AppendChild(permission_text_span_);
}

void HTMLPermissionElement::AdjustStyle(ComputedStyleBuilder& builder) {
  Element::AdjustStyle(builder);

  builder.SetOutlineOffset(builder.OutlineOffset().ClampNegativeToZero());

  auto device_pixel_ratio =
      GetDocument().GetFrame()->LocalFrameRoot().DevicePixelRatio();

  builder.SetMarginLeft(AdjustedBoundedLength(
      builder.MarginLeft(), /*lower_bound=*/kMinMargin * device_pixel_ratio,
      /*upper_bound=*/std::nullopt,
      /*should_multiply_by_content_size=*/false));
  builder.SetMarginRight(AdjustedBoundedLength(
      builder.MarginRight(), /*lower_bound=*/kMinMargin * device_pixel_ratio,
      /*upper_bound=*/std::nullopt,
      /*should_multiply_by_content_size=*/false));
  builder.SetMarginTop(AdjustedBoundedLength(
      builder.MarginTop(), /*lower_bound=*/kMinMargin * device_pixel_ratio,
      /*upper_bound=*/std::nullopt,
      /*should_multiply_by_content_size=*/false));
  builder.SetMarginBottom(AdjustedBoundedLength(
      builder.MarginBottom(), /*lower_bound=*/kMinMargin * device_pixel_ratio,
      /*upper_bound=*/std::nullopt,
      /*should_multiply_by_content_size=*/false));

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
    builder.SetWordSpacing(builder.FontSize() *
                           kMaximumWordSpacingToFontSizeRatio);
  } else if (builder.GetFontDescription().WordSpacing() < 0) {
    builder.SetWordSpacing(0);
  }

  if (builder.GetDisplayStyle().Display() != EDisplay::kNone &&
      builder.GetDisplayStyle().Display() != EDisplay::kInlineBlock) {
    builder.SetDisplay(EDisplay::kInlineBlock);
  }

  if (builder.GetFontDescription().LetterSpacing() >
      kMaximumLetterSpacingToFontSizeRatio * builder.FontSize()) {
    builder.SetLetterSpacing(builder.FontSize() *
                             kMaximumLetterSpacingToFontSizeRatio);
  } else if (builder.GetFontDescription().LetterSpacing() <
             kMinimumLetterSpacingToFontSizeRatio * builder.FontSize()) {
    builder.SetLetterSpacing(builder.FontSize() *
                             kMinimumLetterSpacingToFontSizeRatio);
  }

  builder.SetMinHeight(AdjustedBoundedLength(
      builder.MinHeight(),
      /*lower_bound=*/builder.FontSize() * kMinLengthToFontSizeRatio,
      /*upper_bound=*/builder.FontSize() * kMaxLengthToFontSizeRatio,
      /*should_multiply_by_content_size=*/false));
  builder.SetMaxHeight(AdjustedBoundedLength(
      builder.MaxHeight(),
      /*lower_bound=*/std::nullopt,
      /*upper_bound=*/builder.FontSize() * kMaxLengthToFontSizeRatio,
      /*should_multiply_by_content_size=*/false));

  builder.SetMinWidth(
      AdjustedBoundedLength(builder.MinWidth(),
                            /*lower_bound=*/kMinLengthToFontSizeRatio,
                            /*upper_bound=*/kMaxLengthToFontSizeRatio,
                            /*should_multiply_by_content_size=*/true));

  bool unlimited_width_allowed =
      IsBorderSufficientlyDistinctFromBackgroundColor(builder.CloneStyle());

  if (unlimited_width_allowed) {
    if (builder.PaddingRight().IsSpecified() &&
        !builder.PaddingRight().IsZero() &&
        builder.PaddingLeft() != builder.PaddingRight()) {
      AddConsoleError(
          "The permission element does not support 'padding-right'. "
          "'padding-right' is always set to be identical to 'padding-left'.");
    }
    builder.SetPaddingRight(builder.PaddingLeft());
  } else {
    builder.SetMaxWidth(AdjustedBoundedLength(
        builder.MaxWidth(),
        /*lower_bound=*/std::nullopt, /*upper_bound=*/kMaxLengthToFontSizeRatio,
        /*should_multiply_by_content_size=*/true));

    // If width is set to auto and there is left padding specified, we will
    // respect the padding (up to a certain maximum), otherwise the padding has
    // no effect. We treat height and top/bottom padding similarly.
    if (builder.Width().IsAuto() && builder.PaddingLeft().IsSpecified() &&
        !builder.PaddingLeft().IsZero()) {
      if (builder.PaddingRight().IsSpecified() &&
          !builder.PaddingRight().IsZero() &&
          builder.PaddingLeft() != builder.PaddingRight()) {
        AddConsoleError(
            "The permission element does not support 'padding-right'. "
            "'padding-right' is always set to be identical to 'padding-left'.");
      }

      builder.SetPaddingLeft(
          AdjustedBoundedLength(builder.PaddingLeft(),
                                /*lower_bound=*/std::nullopt,
                                /*upper_bound=*/builder.FontSize() *
                                    kMaxHorizontalPaddingToFontSizeRatio,
                                /*should_multiply_by_content_size=*/false));
      builder.SetPaddingRight(builder.PaddingLeft());
    } else {
      builder.ResetPaddingLeft();
      builder.ResetPaddingRight();
    }
  }

  if (builder.Height().IsAuto() && builder.PaddingTop().IsSpecified() &&
      !builder.PaddingTop().IsZero()) {
    if (builder.PaddingBottom().IsSpecified() &&
        !builder.PaddingBottom().IsZero() &&
        builder.PaddingTop() != builder.PaddingBottom()) {
      AddConsoleError(
          "The permission element does not support 'padding-bottom'. "
          "'padding-bottom' is always set to be identical to 'padding-top'.");
    }
    builder.SetPaddingTop(AdjustedBoundedLength(
        builder.PaddingTop(),
        /*lower_bound=*/std::nullopt,
        /*upper_bound=*/builder.FontSize() * kMaxVerticalPaddingToFontSizeRatio,
        /*should_multiply_by_content_size=*/false));
    builder.SetPaddingBottom(builder.PaddingTop());
  } else {
    builder.ResetPaddingTop();
    builder.ResetPaddingBottom();
  }
}

void HTMLPermissionElement::DidRecalcStyle(const StyleRecalcChange change) {
  HTMLElement::DidRecalcStyle(change);

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

void HTMLPermissionElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kDOMActivate) {
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
        AddConsoleError(
            "The permission element already has a request in progress.");
        return;
      }

      if (IsClickingEnabled()) {
        RequestPageEmbededPermissions();
      }
    } else {
      // For automated testing purposes this behavior can be overridden by
      // adding '--enable-features=BypassPepcSecurityForTesting' to the
      // command line when launching the browser.
      AddConsoleError(
          "The permission element can only be activated by actual user "
          "clicks.");
      base::UmaHistogramEnumeration(
          "Blink.PermissionElement.UserInteractionDeniedReason",
          UserInteractionDeniedReason::kUntrustedEvent);
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
  descriptor->element_position = BoundsInWidget();
  descriptor->permissions = mojo::Clone(permission_descriptors_);

  pending_request_created_ = base::TimeTicks::Now();

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
  GetPermissionService()->AddPageEmbeddedPermissionObserver(
      descriptor.Clone(), current_status, std::move(observer));
}

void HTMLPermissionElement::OnPermissionStatusChange(
    MojoPermissionStatus status) {
  auto permission_name = permission_observer_receivers_.current_context();
  auto it = permission_status_map_.find(permission_name);
  CHECK(it != permission_status_map_.end());
  it->value = status;

  UpdatePermissionStatusAndAppearance();
}

void HTMLPermissionElement::OnEmbeddedPermissionControlRegistered(
    bool allowed,
    const std::optional<Vector<MojoPermissionStatus>>& statuses) {
  if (!allowed) {
    AddConsoleError(String::Format(
        "The permission '%s' has not passed security checks or has surpassed "
        "the maximum instances quota per page.",
        GetType().Utf8().c_str()));
    return;
  }

  CHECK_GT(permission_descriptors_.size(), 0U);
  CHECK_LE(permission_descriptors_.size(), 2U);
  CHECK(statuses.has_value());
  CHECK_EQ(statuses->size(), permission_descriptors_.size());

  bool needs_permission_observer_registration =
      permission_observer_receivers_.empty();
  for (wtf_size_t i = 0; i < permission_descriptors_.size(); ++i) {
    auto status = (*statuses)[i];
    const auto& descriptor = permission_descriptors_[i];
    permission_status_map_.Set(descriptor->name, status);
    if (needs_permission_observer_registration) {
      RegisterPermissionObserver(descriptor, status);
    }
  }

  UpdatePermissionStatusAndAppearance();
  MaybeDispatchValidationChangeEvent();
}

void HTMLPermissionElement::OnEmbeddedPermissionsDecided(
    EmbeddedPermissionControlResult result) {
  pending_request_created_ = std::nullopt;

  switch (result) {
    case EmbeddedPermissionControlResult::kDismissed:
      DispatchEvent(*Event::CreateCancelableBubble(event_type_names::kDismiss));
      return;
    case EmbeddedPermissionControlResult::kGranted:
      aggregated_permission_status_ = MojoPermissionStatus::GRANTED;
      DispatchEvent(*Event::CreateCancelableBubble(event_type_names::kResolve));
      return;
    case EmbeddedPermissionControlResult::kDenied:
      DispatchEvent(*Event::CreateCancelableBubble(event_type_names::kResolve));
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

void HTMLPermissionElement::DisableReasonExpireTimerFired(TimerBase* timer) {
  EnableClicking(static_cast<DisableReasonExpireTimer*>(timer)->reason());
  NotifyClickingDisablePseudoStateChanged();
}

void HTMLPermissionElement::MaybeDispatchValidationChangeEvent() {
  auto state = GetClickingEnabledState();
  if (clicking_enabled_state_ == state) {
    return;
  }

  // Always keep `clicking_enabled_state_` up-to-date
  clicking_enabled_state_ = state;
  DispatchEvent(*Event::CreateCancelableBubble(
      event_type_names::kValidationstatuschange));
}

void HTMLPermissionElement::UpdateSnapshot() {
  ValidateSnapshot();
}

bool HTMLPermissionElement::ValidateSnapshot() {
  return NotifyClickingDisablePseudoStateChanged();
}

bool HTMLPermissionElement::NotifyClickingDisablePseudoStateChanged() {
  ClickingDisablePseudoState new_state(HasInvalidStyle(), IsOccluded());
  if (new_state.is_occluded != pseudo_state_.is_occluded) {
    PseudoStateChanged(CSSSelector::kPseudoPermissionElementOccluded);
  }

  if (new_state.has_invalid_style != pseudo_state_.has_invalid_style) {
    PseudoStateChanged(CSSSelector::kPseudoPermissionElementInvalidStyle);
  }

  if (pseudo_state_ != new_state) {
    pseudo_state_ = new_state;
    return false;
  }

  return true;
}

scoped_refptr<base::SingleThreadTaskRunner>
HTMLPermissionElement::GetTaskRunner() {
  return GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault);
}

bool HTMLPermissionElement::IsClickingEnabled() {
  if (permission_descriptors_.empty()) {
    AddConsoleError(String::Format(
        "The permission element '%s' cannot be activated due to invalid type.",
        GetType().Utf8().c_str()));
    base::UmaHistogramEnumeration(
        "Blink.PermissionElement.UserInteractionDeniedReason",
        UserInteractionDeniedReason::kInvalidType);
    return false;
  }

  if (!IsRegisteredInBrowserProcess()) {
    AddConsoleError(String::Format(
        "The permission element '%s' cannot be activated because of security "
        "checks or because the page's quota has been exceeded.",
        GetType().Utf8().c_str()));
    base::UmaHistogramEnumeration(
        "Blink.PermissionElement.UserInteractionDeniedReason",
        UserInteractionDeniedReason::kFailedOrHasNotBeenRegistered);
    return false;
  }

  // Do not check click-disabling reasons if the PEPC validation feature is
  // disabled. This should only occur in testing scenarios.
  if (RuntimeEnabledFeatures::BypassPepcSecurityForTestingEnabled()) {
    return true;
  }

  // Remove expired reasons. If a non-expired reason is found, then clicking is
  // disabled.
  base::TimeTicks now = base::TimeTicks::Now();
  while (!clicking_disabled_reasons_.empty()) {
    auto it = clicking_disabled_reasons_.begin();
    if (it->value >= now) {
      AddConsoleError(String::Format(
          "The permission element '%s' cannot be activated due to %s.",
          GetType().Utf8().c_str(),
          DisableReasonToString(it->key).Utf8().c_str()));
      if (it->key ==
              DisableReason::kIntersectionVisibilityOccludedOrDistorted &&
          occluder_node_id_ != kInvalidDOMNodeId) {
        if (Node* node = DOMNodeIds::NodeForId(occluder_node_id_)) {
          AddConsoleError(
              String::Format("The permission element is occluded by node %s",
                             node->ToString().Utf8().c_str()));
        }
      }
      base::UmaHistogramEnumeration(
          "Blink.PermissionElement.UserInteractionDeniedReason",
          DisableReasonToUserInteractionDeniedReason(it->key));
      return false;
    }
    clicking_disabled_reasons_.erase(it);
  }

  return true;
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
  if (permission_descriptors_.empty()) {
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

  if (!IsRegisteredInBrowserProcess()) {
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

void HTMLPermissionElement::UpdatePermissionStatusAndAppearance() {
  if (base::ranges::any_of(permission_status_map_, [](const auto& status) {
        return status.value == MojoPermissionStatus::DENIED;
      })) {
    aggregated_permission_status_ = MojoPermissionStatus::DENIED;
  } else if (base::ranges::any_of(
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

  PseudoStateChanged(CSSSelector::kPseudoPermissionGranted);
  UpdateText();
}

void HTMLPermissionElement::UpdateText() {
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

  AtomicString language_string = ComputeInheritedLanguage().LowerASCII();

  int untranslated_message_id =
      permission_count == 1
          ? GetUntranslatedMessageIDSinglePermission(
                permission_name, permission_granted, is_precise_location_)
          : GetUntranslatedMessageIDMultiplePermissions(permission_granted);
  int translated_message_id =
      GetTranslatedMessageID(untranslated_message_id, language_string);
  CHECK(translated_message_id);
  permission_text_span_->setInnerText(
      GetLocale().QueryString(translated_message_id));
}

void HTMLPermissionElement::AddConsoleError(String error) {
  AddConsoleMessage(mojom::blink::ConsoleMessageSource::kRendering,
                    mojom::blink::ConsoleMessageLevel::kError, error);
}

void HTMLPermissionElement::AddConsoleWarning(String warning) {
  AddConsoleMessage(mojom::blink::ConsoleMessageSource::kRendering,
                    mojom::blink::ConsoleMessageLevel::kWarning, warning);
}

void HTMLPermissionElement::OnIntersectionChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  CHECK(!entries.empty());
  Member<IntersectionObserverEntry> latest_observation = entries.back();
  CHECK_EQ(this, latest_observation->target());
  IntersectionVisibility intersection_visibility =
      IntersectionVisibility::kFullyVisible;
  // `intersectionRatio` >= `kIntersectionThreshold` (1.0f) means the element is
  // fully visible on the viewport (vs `intersectionRatio` < 1.0f means its
  // bound is clipped by the viewport or styling effects). In this case, the
  // `isVisible` false means the element is occluded by something else or has
  // distorted visual effect applied.
  if (!latest_observation->isVisible()) {
    intersection_visibility =
        latest_observation->intersectionRatio() >= kIntersectionThreshold
            ? IntersectionVisibility::kOccludedOrDistorted
            : IntersectionVisibility::kOutOfViewportOrClipped;
  }

  if (intersection_visibility_ == intersection_visibility) {
    return;
  }
  intersection_visibility_ = intersection_visibility;
  occluder_node_id_ = kInvalidDOMNodeId;
  switch (intersection_visibility_) {
    case IntersectionVisibility::kFullyVisible: {
      std::optional<base::TimeDelta> interval =
          GetRecentlyAttachedTimeoutRemaining();
      DisableClickingTemporarily(
          DisableReason::kIntersectionRecentlyFullyVisible,
          interval ? interval.value() : kDefaultDisableTimeout);
      EnableClicking(DisableReason::kIntersectionVisibilityOccludedOrDistorted);
      EnableClicking(
          DisableReason::kIntersectionVisibilityOutOfViewPortOrClipped);
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

  // TODO(crbug.com/342330035): revisit it when we write spec for <permission>
  // element.
  GetTaskRunner()->PostTask(
      FROM_HERE, WTF::BindOnce(&HTMLPermissionElement::UpdateSnapshot,
                               WrapWeakPersistent(this)));
}

bool HTMLPermissionElement::IsStyleValid() {
  // No computed style when using `display: none`.
  if (!GetComputedStyle()) {
    AddConsoleWarning(
        String::Format("Cannot compute style for the permission element '%s'",
                       GetType().Utf8().c_str()));
    base::UmaHistogramEnumeration("Blink.PermissionElement.InvalidStyleReason",
                                  InvalidStyleReason::kNoComputedStyle);
    return false;
  }

  if (AreColorsNonOpaque(GetComputedStyle())) {
    AddConsoleWarning(
        String::Format("Color or background color of the permission element "
                       "'%s' is non-opaque",
                       GetType().Utf8().c_str()));
    base::UmaHistogramEnumeration(
        "Blink.PermissionElement.InvalidStyleReason",
        InvalidStyleReason::kNonOpaqueColorOrBackgroundColor);
    return false;
  }

  if (ContrastBetweenColorAndBackgroundColor(GetComputedStyle()) <
      kMinimumAllowedContrast) {
    AddConsoleWarning(
        String::Format("Contrast between color and background color of the "
                       "permission element '%s' is too low",
                       GetType().Utf8().c_str()));
    base::UmaHistogramEnumeration(
        "Blink.PermissionElement.InvalidStyleReason",
        InvalidStyleReason::kLowConstrastColorAndBackgroundColor);
    return false;
  }

  // Compute the font size but reverse browser zoom as it should not affect font
  // size validation. The same font size value should always pass regardless of
  // what the user's browser zoom is.
  // TODO(crbug.com/352046941): This does not currently distinguish between
  // browser zoom and cross-origin iframe zoom (set via CSS).
  float font_size_dip = GetComputedStyle()->ComputedFontSize() /
                        GetComputedStyle()->EffectiveZoom();

  float css_zoom_factor =
      GetComputedStyle()->EffectiveZoom() /
      GetDocument().GetFrame()->LocalFrameRoot().LayoutZoomFactor();

  // The min size is what `font-size:small` looks like when rendered in the
  // document element of the local root frame, without any intervening CSS
  // zoom factors applied.
  float min_font_size_dip = FontSizeFunctions::FontSizeForKeyword(
      &GetDocument(), FontSizeFunctions::KeywordSize(CSSValueID::kSmall),
      GetComputedStyle()->GetFontDescription().IsMonospace());
  if (font_size_dip < min_font_size_dip / css_zoom_factor) {
    AddConsoleWarning(
        String::Format("Font size of the permission element '%s' is too small",
                       GetType().Utf8().c_str()));
    base::UmaHistogramEnumeration("Blink.PermissionElement.InvalidStyleReason",
                                  InvalidStyleReason::kTooSmallFontSize);
    return false;
  }

  // The max size is what `font-size:xxxlarge` looks like when rendered in the
  // document element of the local root frame, without any intervening CSS
  // zoom factors applied.
  float max_font_size_dip = FontSizeFunctions::FontSizeForKeyword(
      &GetDocument(), FontSizeFunctions::KeywordSize(CSSValueID::kXxxLarge),
      GetComputedStyle()->GetFontDescription().IsMonospace());
  if (font_size_dip > max_font_size_dip / css_zoom_factor) {
    AddConsoleWarning(
        String::Format("Font size of the permission element '%s' is too large",
                       GetType().Utf8().c_str()));
    base::UmaHistogramEnumeration("Blink.PermissionElement.InvalidStyleReason",
                                  InvalidStyleReason::kTooLargeFontSize);
    return false;
  }

  return true;
}

Length HTMLPermissionElement::AdjustedBoundedLength(
    const Length& length,
    std::optional<float> lower_bound,
    std::optional<float> upper_bound,
    bool should_multiply_by_content_size) {
  CHECK(lower_bound.has_value() || upper_bound.has_value());
  bool is_content_or_stretch =
      length.HasContentOrIntrinsic() || length.HasStretch();
  if (is_content_or_stretch && !length_console_error_sent_) {
    length_console_error_sent_ = true;
    AddConsoleWarning(
        "content, intrinsic, or stretch sizes are not supported as values for "
        "the min/max width and height of the permission element");
  }

  const Length& length_to_use =
      is_content_or_stretch || length.IsNone() ? Length::Auto() : length;

  // If the |length| is not supported and the |bound| is static, return a simple
  // fixed length.
  if (length_to_use.IsAuto() && !should_multiply_by_content_size) {
    return Length(
        lower_bound.has_value() ? lower_bound.value() : upper_bound.value(),
        Length::Type::kFixed);
  }

  // If the |length| is supported and the |bound| is static, return a
  // min|max|clamp expression-type length.
  if (!should_multiply_by_content_size) {
    auto lower_bound_expr =
        lower_bound.has_value()
            ? std::optional(base::MakeRefCounted<
                            blink::CalculationExpressionPixelsAndPercentNode>(
                  PixelsAndPercent(lower_bound.value())))
            : std::nullopt;

    auto upper_bound_expr =
        upper_bound.has_value()
            ? std::optional(base::MakeRefCounted<
                            blink::CalculationExpressionPixelsAndPercentNode>(
                  PixelsAndPercent(upper_bound.value())))
            : std::nullopt;

    // expr = min|max|clamp(bound, length, [bound2])
    auto expr =
        BuildLengthBoundExpr(length_to_use, lower_bound_expr, upper_bound_expr);
    return Length(CalculationValue::CreateSimplified(
        std::move(expr), Length::ValueRange::kNonNegative));
  }

  // bound_expr = size * bound.
  auto lower_bound_expr =
      lower_bound.has_value()
          ? std::optional(BuildFitContentExpr(lower_bound.value()))
          : std::nullopt;
  auto upper_bound_expr =
      upper_bound.has_value()
          ? std::optional(BuildFitContentExpr(upper_bound.value()))
          : std::nullopt;

  scoped_refptr<const CalculationExpressionNode> bound_expr;

  if (!length_to_use.IsAuto()) {
    // bound_expr = min|max|clamp(size * bound, length, [size * bound2])
    bound_expr =
        BuildLengthBoundExpr(length_to_use, lower_bound_expr, upper_bound_expr);
  } else {
    bound_expr = lower_bound_expr.has_value()
                     ? std::move(lower_bound_expr.value())
                     : std::move(upper_bound_expr.value());
  }

  // This uses internally the CalculationExpressionSizingKeywordNode to create
  // an expression that depends on the size of the contents of the permission
  // element, in order to set necessary min/max bounds on width and height. If
  // https://drafts.csswg.org/css-values-5/#calc-size is ever abandoned,
  // the functionality should still be kept around in some way that can
  // facilitate this use case.

  auto fit_content_expr =
      base::MakeRefCounted<CalculationExpressionSizingKeywordNode>(
          CalculationExpressionSizingKeywordNode::Keyword::kFitContent);

  // expr = calc-size(fit-content, bound_expr)
  auto expr = CalculationExpressionOperationNode::CreateSimplified(
      CalculationExpressionOperationNode::Children(
          {fit_content_expr, bound_expr}),
      CalculationOperator::kCalcSize);

  return Length(CalculationValue::CreateSimplified(
      std::move(expr), Length::ValueRange::kNonNegative));
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
}

gfx::Rect HTMLPermissionElement::ComputeIntersectionRectWithViewport(
    const Page* page) {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object) {
    return gfx::Rect();
  }

  gfx::Rect viewport_in_root_frame =
      ToEnclosingRect(page->GetVisualViewport().VisibleRect());
  PhysicalRect rect = To<LayoutBox>(layout_object)->PhysicalBorderBoxRect();
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

}  // namespace blink
