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
#include "third_party/blink/renderer/core/css/font_size_functions.h"
#include "third_party/blink/renderer/core/css/properties/css_property_instances.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
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
#include "ui/gfx/color_utils.h"

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
constexpr float kMaximumWordSpacingToFontSizeRatio = 0.5;
constexpr float kMinimumAllowedContrast = 3.;
constexpr float kMaximumLetterSpacingToFontSizeRatio = 0.2;
constexpr float kMinimumLetterSpacingToFontSizeRatio = -0.05;
constexpr int kMaxLengthToFontSizeRatio = 3;
constexpr int kMinLengthToFontSizeRatio = 1;

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
int GetMessageIDSinglePermission(PermissionName name, bool granted) {
  if (name == PermissionName::VIDEO_CAPTURE) {
    return granted ? IDS_PERMISSION_REQUEST_CAMERA_ALLOWED
                   : IDS_PERMISSION_REQUEST_CAMERA;
  }

  if (name == PermissionName::AUDIO_CAPTURE) {
    return granted ? IDS_PERMISSION_REQUEST_MICROPHONE_ALLOWED
                   : IDS_PERMISSION_REQUEST_MICROPHONE;
  }

  if (name == PermissionName::GEOLOCATION) {
    return granted ? IDS_PERMISSION_REQUEST_GEOLOCATION_ALLOWED
                   : IDS_PERMISSION_REQUEST_GEOLOCATION;
  }

  return 0;
}

// Helper to get permission text resource ID for the given map which has
// multiple elements. Currently we only support "camera microphone" grouped
// permissions.
int GetMessageIDMultiplePermissions(bool granted) {
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

float ContrastBetweenColorAndBackgroundColor(const ComputedStyle* style) {
  return color_utils::GetContrastRatio(
      style->VisitedDependentColor(GetCSSPropertyColor()).toSkColor4f(),
      style->VisitedDependentColor(GetCSSPropertyBackgroundColor())
          .toSkColor4f());
}

// Returns true if the 'color' or 'background-color' properties have the
// alphas set to anything else except fully opaque.
bool AreColorsNonOpaque(const ComputedStyle* style) {
  return style->VisitedDependentColor(GetCSSPropertyColor()).Alpha() != 1. ||
         style->VisitedDependentColor(GetCSSPropertyBackgroundColor())
                 .Alpha() != 1;
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

// Builds an expression that takes a |length| and bounds it either lower or
// higher with the provided |bound_expr|.
scoped_refptr<const CalculationExpressionNode> BuildLengthBoundExpr(
    const Length& length,
    scoped_refptr<const CalculationExpressionNode> bound_expr,
    bool is_lower_bound) {
  return CalculationExpressionOperationNode::CreateSimplified(
      CalculationExpressionOperationNode::Children(
          {bound_expr, length.AsCalculationValue()->GetOrCreateExpression()}),
      is_lower_bound ? CalculationOperator::kMax : CalculationOperator::kMin);
}

bool IsEventTrusted(const Event* event) {
  // TODO(crbug.com/333844641): verifying the top-level event should be
  // sufficient, but it's currently not. To be updated when the associated bug
  // is fixed.
  while (event) {
    if (!event->isTrusted()) {
      return false;
    }
    event = event->UnderlyingEvent();
  }

  return true;
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
  UseCounter::Count(document, WebFeature::kHTMLPermissionElement);
}

HTMLPermissionElement::~HTMLPermissionElement() = default;

const AtomicString& HTMLPermissionElement::GetType() const {
  return type_.IsNull() ? g_empty_atom : type_;
}

void HTMLPermissionElement::Trace(Visitor* visitor) const {
  visitor->Trace(permission_service_);
  visitor->Trace(permission_observer_receivers_);
  visitor->Trace(embedded_permission_control_receiver_);
  visitor->Trace(permission_text_span_);
  visitor->Trace(intersection_observer_);
  HTMLElement::Trace(visitor);
}

void HTMLPermissionElement::AttachLayoutTree(AttachContext& context) {
  Element::AttachLayoutTree(context);
  if (permission_descriptors_.empty()) {
    return;
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
  DisableClickingTemporarily(DisableReason::kRecentlyAttachedToDOM,
                             kDefaultDisableTimeout);
  if (embedded_permission_control_receiver_.is_bound()) {
    return;
  }
  mojo::PendingRemote<EmbeddedPermissionControlClient> client;
  embedded_permission_control_receiver_.Bind(
      client.InitWithNewPipeAndPassReceiver(), GetTaskRunner());
  GetPermissionService()->RegisterPageEmbeddedPermissionControl(
      mojo::Clone(permission_descriptors_), std::move(client));
}

void HTMLPermissionElement::DetachLayoutTree(bool performing_reattach) {
  Element::DetachLayoutTree(performing_reattach);
  embedded_permission_control_receiver_.reset();
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
                permission_descriptors_[0]->name, /*granted=*/false)));
        break;
      case 2:
        permission_text_span_->setInnerText(
            GetLocale().QueryString(IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE));
        break;
      default:
        NOTREACHED() << "Unexpected permissions size "
                     << permission_descriptors_.size();
    }
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
      builder.MinHeight(), builder.FontSize() * kMinLengthToFontSizeRatio,
      /*is_lower_bound=*/true,
      /*should_multiply_by_content_size=*/false));
  builder.SetMaxHeight(AdjustedBoundedLength(
      builder.MaxHeight(), builder.FontSize() * kMaxLengthToFontSizeRatio,
      /*is_lower_bound=*/false,
      /*should_multiply_by_content_size=*/false));
  builder.SetMinWidth(
      AdjustedBoundedLength(builder.MinWidth(), kMinLengthToFontSizeRatio,
                            /*is_lower_bound=*/true,
                            /*should_multiply_by_content_size=*/true));
  builder.SetMaxWidth(
      AdjustedBoundedLength(builder.MaxWidth(), kMaxLengthToFontSizeRatio,
                            /*is_lower_bound=*/false,
                            /*should_multiply_by_content_size=*/true));
}

void HTMLPermissionElement::DidRecalcStyle(const StyleRecalcChange change) {
  if (IsStyleValid()) {
    EnableClickingAfterDelay(DisableReason::kInvalidStyle,
                             kDefaultDisableTimeout);
  } else {
    DisableClickingIndefinitely(DisableReason::kInvalidStyle);
  }
}

void HTMLPermissionElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kDOMActivate) {
    event.SetDefaultHandled();
    if (IsEventTrusted(&event) ||
        RuntimeEnabledFeatures::DisablePepcSecurityForTestingEnabled()) {
      if (IsClickingEnabled()) {
        RequestPageEmbededPermissions();
      }
    } else {
      // For automated testing purposes this behavior can be overridden by
      // adding '--enable-features=DisablePepcSecurityForTesting' to the
      // command line when launching the browser.
      AddConsoleError(
          "The permission element can only be activated by actual user "
          "clicks.");
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
  if (permission_descriptors_.empty()) {
    return false;
  }

  if (!IsRegisteredInBrowserProcess()) {
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
  if (clicking_disabled_reasons_.Contains(reason)) {
    clicking_disabled_reasons_.Set(reason, base::TimeTicks::Now() + delay);
  }
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
  bool granted =
      base::ranges::all_of(permission_status_map_, [](const auto& status) {
        return status.value == MojoPermissionStatus::GRANTED;
      });

  int message_id = permission_status_map_.size() == 1
                       ? GetMessageIDSinglePermission(
                             permission_status_map_.begin()->key, granted)
                       : GetMessageIDMultiplePermissions(granted);

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

bool HTMLPermissionElement::IsStyleValid() {
  // No computed style when using `display: none`.
  if (!GetComputedStyle()) {
    return false;
  }

  if (AreColorsNonOpaque(GetComputedStyle())) {
    return false;
  }
  if (ContrastBetweenColorAndBackgroundColor(GetComputedStyle()) <
      kMinimumAllowedContrast) {
    return false;
  }

  if (GetComputedStyle()->ComputedFontSize() <
      FontSizeFunctions::FontSizeForKeyword(
          &GetDocument(), FontSizeFunctions::KeywordSize(CSSValueID::kSmall),
          GetComputedStyle()->GetFontDescription().IsMonospace())) {
    return false;
  }

  if (GetComputedStyle()->ComputedFontSize() >
      FontSizeFunctions::FontSizeForKeyword(
          &GetDocument(), FontSizeFunctions::KeywordSize(CSSValueID::kXxxLarge),
          GetComputedStyle()->GetFontDescription().IsMonospace())) {
    return false;
  }

  return true;
}

Length HTMLPermissionElement::AdjustedBoundedLength(
    const Length& length,
    float bound,
    bool is_lower_bound,
    bool should_multiply_by_content_size) {
  bool is_content_or_stretch =
      length.HasContentOrIntrinsic() || length.HasStretch();
  if (is_content_or_stretch && !length_console_error_sent_) {
    length_console_error_sent_ = true;
    AddConsoleError(
        "content, intrinsic, or stretch sizes are not supported as values for "
        "the min/max width and height of the permission element");
  }

  const Length& length_to_use =
      is_content_or_stretch || length.IsNone() ? Length::Auto() : length;

  // If the |length| is not supported and the |bound| is static, return a simple
  // fixed length.
  if (length_to_use.IsAuto() && !should_multiply_by_content_size) {
    return Length(bound, Length::Type::kFixed);
  }

  // If the |length| is supported and the |bound| is static, return a min|max
  // expression-type length.
  if (!should_multiply_by_content_size) {
    auto bound_expr =
        base::MakeRefCounted<blink::CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(bound));

    // expr = min|max(bound, length)
    auto expr = BuildLengthBoundExpr(length_to_use, bound_expr, is_lower_bound);
    return Length(CalculationValue::CreateSimplified(
        std::move(expr), Length::ValueRange::kNonNegative));
  }

  // bound_expr = size * bound.
  auto bound_expr = BuildFitContentExpr(bound);

  if (!length_to_use.IsAuto()) {
    // bound_expr = min|max(size * bound, length)
    bound_expr =
        BuildLengthBoundExpr(length_to_use, bound_expr, is_lower_bound);
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

}  // namespace blink
