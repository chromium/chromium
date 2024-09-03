// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

#include "base/metrics/histogram_macros.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/fenced_frame/document_fenced_frames.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_ad_sizes.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

PhysicalRect ToPhysicalRect(const DOMRectReadOnly& rect) {
  return PhysicalRect(LayoutUnit::FromDoubleRound(rect.x()),
                      LayoutUnit::FromDoubleRound(rect.y()),
                      LayoutUnit::FromDoubleRound(rect.width()),
                      LayoutUnit::FromDoubleRound(rect.height()));
}

String DeprecatedFencedFrameModeToString(
    blink::FencedFrame::DeprecatedFencedFrameMode mode) {
  switch (mode) {
    case blink::FencedFrame::DeprecatedFencedFrameMode::kDefault:
      return "default";
    case blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds:
      return "opaque-ads";
  }

  NOTREACHED_IN_MIGRATION();
  return "";
}

// Helper function that returns whether the mode of the parent tree is different
// than the mode given to the function. Note that this function will return
// false if there is no mode set in the parent tree (i.e. not in a fenced frame
// tree).
bool ParentModeIsDifferent(
    blink::FencedFrame::DeprecatedFencedFrameMode current_mode,
    LocalFrame& frame) {
  Page* ancestor_page = frame.GetPage();
  return ancestor_page->IsMainFrameFencedFrameRoot() &&
         ancestor_page->DeprecatedFencedFrameMode() != current_mode;
}

bool HasDifferentModeThanParent(HTMLFencedFrameElement& outer_element) {
  return ParentModeIsDifferent(outer_element.GetDeprecatedMode(),
                               *(outer_element.GetDocument().GetFrame()));
}

// Returns whether `requested_size` is exactly the same size as `allowed_size`.
// `requested_size` and `allowed_size` should both be in CSS pixel units.
bool SizeMatchesExactly(const PhysicalSize& requested_size,
                        const gfx::Size& allowed_size) {
  // The comparison must be performed as a `PhysicalSize`, in order to use
  // its fixed point representation and get exact results.
  return requested_size == PhysicalSize(allowed_size);
}

// Returns a loss score (higher is worse) comparing the fit between
// `requested_size` and `allowed_size`.
// Both sizes should be in CSS pixel units.
double ComputeSizeLossFunction(const PhysicalSize& requested_size,
                               const gfx::Size& allowed_size) {
  const double requested_width = requested_size.width.ToDouble();
  const double requested_height = requested_size.height.ToDouble();

  const double allowed_width = allowed_size.width();
  const double allowed_height = allowed_size.height();

  const double allowed_area = allowed_width * allowed_height;
  const double requested_area = requested_width * requested_height;

  // Calculate the fraction of the outer container that is wasted when the
  // allowed inner frame size is scaled to fit inside of it.
  const double scale_x = allowed_width / requested_width;
  const double scale_y = allowed_height / requested_height;

  const double wasted_area =
      scale_x < scale_y
          ? allowed_width * (allowed_height - (scale_x * requested_height))
          : allowed_height * (allowed_width - (scale_y * requested_width));

  const double wasted_area_fraction = wasted_area / allowed_area;

  // Calculate a penalty to tie-break between allowed sizes with the same
  // aspect ratio in favor of resolutions closer to the requested one.
  const double resolution_penalty =
      std::abs(1 - std::min(requested_area, allowed_area) /
                       std::max(requested_area, allowed_area));

  return wasted_area_fraction + resolution_penalty;
}

std::optional<WTF::AtomicString> ConvertEventTypeToFencedEventType(
    const WTF::String& event_type) {
  if (!CanNotifyEventTypeAcrossFence(event_type.Ascii())) {
    return std::nullopt;
  }

  return event_type_names::kFencedtreeclick;
}

}  // namespace

HTMLFencedFrameElement::HTMLFencedFrameElement(Document& document)
    : HTMLFrameOwnerElement(html_names::kFencedframeTag, document),
      sandbox_(MakeGarbageCollected<HTMLIFrameElementSandbox>(this)) {
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(GetExecutionContext()));
  UseCounter::Count(document, WebFeature::kHTMLFencedFrameElement);
  StartResizeObserver();
}

HTMLFencedFrameElement::~HTMLFencedFrameElement() = default;

void HTMLFencedFrameElement::Trace(Visitor* visitor) const {
  HTMLFrameOwnerElement::Trace(visitor);
  visitor->Trace(frame_delegate_);
  visitor->Trace(resize_observer_);
  visitor->Trace(config_);
  visitor->Trace(sandbox_);
}

DOMTokenList* HTMLFencedFrameElement::sandbox() const {
  return sandbox_.Get();
}

void HTMLFencedFrameElement::DisconnectContentFrame() {
  DCHECK(!GetDocument().IsPrerendering());

  // The `frame_delegate_` will not exist if the element was not allowed to
  // create its underlying frame at insertion-time.
  if (frame_delegate_) {
    frame_delegate_->Dispose();
  }
  frame_delegate_ = nullptr;

  HTMLFrameOwnerElement::DisconnectContentFrame();
}

ParsedPermissionsPolicy HTMLFencedFrameElement::ConstructContainerPolicy()
    const {
  if (!GetExecutionContext()) {
    return ParsedPermissionsPolicy();
  }

  scoped_refptr<const SecurityOrigin> src_origin =
      GetOriginForPermissionsPolicy();
  scoped_refptr<const SecurityOrigin> self_origin =
      GetExecutionContext()->GetSecurityOrigin();

  PolicyParserMessageBuffer logger;

  ParsedPermissionsPolicy container_policy =
      PermissionsPolicyParser::ParseAttribute(allow_, self_origin, src_origin,
                                              logger, GetExecutionContext());

  for (const auto& message : logger.GetMessages()) {
    GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther, message.level,
            message.content),
        /* discard_duplicates */ true);
  }

  return container_policy;
}

void HTMLFencedFrameElement::SetCollapsed(bool collapse) {
  if (collapsed_by_client_ == collapse) {
    return;
  }

  collapsed_by_client_ = collapse;

  // This is always called in response to an IPC, so should not happen in the
  // middle of a style recalc.
  DCHECK(!GetDocument().InStyleRecalc());

  // Trigger style recalc to trigger layout tree re-attachment.
  SetNeedsStyleRecalc(kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                             style_change_reason::kFrame));
}

void HTMLFencedFrameElement::DidChangeContainerPolicy() {
  // Don't notify about updates if frame_delegate_ is null, for example when
  // the delegate hasn't been created yet.
  if (frame_delegate_) {
    frame_delegate_->DidChangeFramePolicy(GetFramePolicy());
  }
}

HTMLIFrameElement* HTMLFencedFrameElement::InnerIFrameElement() const {
  if (const ShadowRoot* root = UserAgentShadowRoot())
    return To<HTMLIFrameElement>(root->lastChild());
  return nullptr;
}

void HTMLFencedFrameElement::setConfig(FencedFrameConfig* config) {
  config_ = config;

  if (config_) {
    NavigateToConfig();
  } else {
    Navigate(BlankURL());
  }
}

// static
bool HTMLFencedFrameElement::canLoadOpaqueURL(ScriptState* script_state) {
  if (!script_state->ContextIsValid())
    return false;

  LocalDOMWindow::From(script_state)
      ->document()
      ->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "HTMLFencedFrameElement.canLoadOpaqueURL() is deprecated and will be "
          "removed. Please use navigator.canLoadAdAuctionFencedFrame() "
          "instead."));

  UseCounter::Count(LocalDOMWindow::From(script_state)->document(),
                    WebFeature::kFencedFrameCanLoadOpaqueURL);

  LocalFrame* frame_to_check = LocalDOMWindow::From(script_state)->GetFrame();
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(frame_to_check && context);

  // "A fenced frame tree of one mode cannot contain a child fenced frame of
  // another mode."
  // See: https://github.com/WICG/fenced-frame/blob/master/explainer/modes.md
  // TODO(lbrady) Link to spec once it's written.
  if (ParentModeIsDifferent(
          blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds,
          *frame_to_check)) {
    return false;
  }

  if (!context->IsSecureContext())
    return false;

  // Check that the flags specified in kFencedFrameMandatoryUnsandboxedFlags
  // are not set in this context. Fenced frames loaded in a sandboxed document
  // require these flags to remain unsandboxed.
  if (context->IsSandboxed(kFencedFrameMandatoryUnsandboxedFlags))
    return false;

  // Check the results of the browser checks for the current frame.
  // If the embedding frame is an iframe with CSPEE set, or any ancestor
  // iframes has CSPEE set, the fenced frame will not be allowed to load.
  // The renderer has no knowledge of CSPEE up the ancestor chain, so we defer
  // to the browser to determine the existence of CSPEE outside of the scope
  // we can see here.
  if (frame_to_check->AncestorOrSelfHasCSPEE())
    return false;

  // Ensure that if any CSP headers are set that will affect a fenced frame,
  // they allow all https urls to load. Opaque-ads fenced frames do not support
  // allowing/disallowing specific hosts, as that could reveal information to
  // a fenced frame about its embedding page. See design doc for more info:
  // https://github.com/WICG/fenced-frame/blob/master/explainer/interaction_with_content_security_policy.md
  // This is being checked in the renderer because processing of <meta> tags
  // (including CSP) happen in the renderer after navigation commit, so we can't
  // piggy-back off of the ancestor_or_self_has_cspee bit being sent from the
  // browser (which is sent at commit time) since it doesn't know about all the
  // CSP headers yet.
  ContentSecurityPolicy* csp = context->GetContentSecurityPolicy();
  DCHECK(csp);
  if (!csp->AllowFencedFrameOpaqueURL()) {
    return false;
  }

  return true;
}

Node::InsertionNotificationRequest HTMLFencedFrameElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLFrameOwnerElement::InsertedInto(insertion_point);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLFencedFrameElement::DidNotifySubtreeInsertionsToDocument() {
  CreateDelegateAndNavigate();
}

void HTMLFencedFrameElement::RemovedFrom(ContainerNode& node) {
  // Verify that the underlying frame has already been disconnected via
  // `DisconnectContentFrame()`. This is only relevant for the MPArch
  // implementation.
  DCHECK_EQ(ContentFrame(), nullptr);
  HTMLFrameOwnerElement::RemovedFrom(node);
}

void HTMLFencedFrameElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kSandboxAttr) {
    sandbox_->DidUpdateAttributeValue(params.old_value, params.new_value);

    network::mojom::blink::WebSandboxFlags current_flags =
        network::mojom::blink::WebSandboxFlags::kNone;
    if (!params.new_value.IsNull()) {
      using network::mojom::blink::WebSandboxFlags;
      auto parsed = network::ParseWebSandboxPolicy(sandbox_->value().Utf8(),
                                                   WebSandboxFlags::kNone);
      current_flags = parsed.flags;
      if (!parsed.error_message.empty()) {
        GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Error while parsing the 'sandbox' attribute: " +
                String::FromUTF8(parsed.error_message)));
      }
    }
    SetSandboxFlags(current_flags);
    UseCounter::Count(GetDocument(), WebFeature::kSandboxViaFencedFrame);
  } else if (params.name == html_names::kAllowAttr) {
    if (allow_ != params.new_value) {
      allow_ = params.new_value;
      if (!params.new_value.empty()) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kFeaturePolicyAllowAttribute);
      }
    }
  } else {
    HTMLFrameOwnerElement::ParseAttribute(params);
  }
}

bool HTMLFencedFrameElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kWidthAttr || name == html_names::kHeightAttr)
    return true;
  return HTMLFrameOwnerElement::IsPresentationAttribute(name);
}

void HTMLFencedFrameElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kWidthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value);
  } else if (name == html_names::kHeightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, value);
  } else {
    HTMLFrameOwnerElement::CollectStyleForPresentationAttribute(name, value,
                                                                style);
  }
}

void HTMLFencedFrameElement::Navigate(
    const KURL& url,
    std::optional<bool> deprecated_should_freeze_initial_size,
    std::optional<gfx::Size> container_size,
    std::optional<gfx::Size> content_size,
    String embedder_shared_storage_context) {
  TRACE_EVENT0("navigation", "HTMLFencedFrameElement::Navigate");
  if (!isConnected())
    return;

  // Please see `FencedFrameDelegate::Create` for a list of conditions which
  // could result in not having a frame delegate at this point, one of which is
  // prerendering. If this function is called while prerendering we won't have a
  // delegate and will bail early, but this should still be correct since,
  // post-activation, CreateDelegateAndNavigate will be run which will navigate
  // to the most current config.
  if (!frame_delegate_)
    return;

  if (url.IsEmpty())
    return;

  if (!GetExecutionContext()->IsSecureContext()) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A fenced frame was not loaded because the page is not in a secure "
        "context."));
    RecordFencedFrameCreationOutcome(
        FencedFrameCreationOutcome::kInsecureContext);
    return;
  }

  if (IsValidUrnUuidURL(GURL(url))) {
    mode_ = blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds;
  } else if (IsValidFencedFrameURL(GURL(url))) {
    mode_ = blink::FencedFrame::DeprecatedFencedFrameMode::kDefault;
  } else {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A fenced frame must be navigated to an \"https\" URL, an \"http\" "
        "localhost URL,"
        " \"about:blank\", or a \"urn:uuid\"."));
    RecordFencedFrameCreationOutcome(
        FencedFrameCreationOutcome::kIncompatibleURLDefault);
    return;
  }

  if (HasDifferentModeThanParent(*this)) {
    blink::FencedFrame::DeprecatedFencedFrameMode parent_mode =
        GetDocument().GetPage()->DeprecatedFencedFrameMode();

    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "Cannot create a fenced frame with mode '" +
            DeprecatedFencedFrameModeToString(GetDeprecatedMode()) +
            "' nested in a fenced frame with mode '" +
            DeprecatedFencedFrameModeToString(parent_mode) + "'."));
    RecordFencedFrameCreationOutcome(
        FencedFrameCreationOutcome::kIncompatibleMode);
    return;
  }

  // Cannot perform an embedder-initiated navigation in a fenced frame when the
  // sandbox attribute restricts any of the mandatory unsandboxed features.
  if (static_cast<int>(GetFramePolicy().sandbox_flags) &
      static_cast<int>(blink::kFencedFrameMandatoryUnsandboxedFlags)) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "Can't navigate the fenced frame. A sandboxed fenced frame can "
        "only be navigated by its embedder when all of the following "
        "flags are set: allow-same-origin, allow-forms, allow-scripts, "
        "allow-popups, allow-popups-to-escape-sandbox, and "
        "allow-top-navigation-by-user-activation."));
    RecordFencedFrameCreationOutcome(
        FencedFrameCreationOutcome::kSandboxFlagsNotSet);
    RecordFencedFrameUnsandboxedFlags(GetFramePolicy().sandbox_flags);
    return;
  }

  UpdateContainerPolicy();

  frame_delegate_->Navigate(url, embedder_shared_storage_context);

  RecordFencedFrameCreationOutcome(
      mode_ == blink::FencedFrame::DeprecatedFencedFrameMode::kDefault
          ? FencedFrameCreationOutcome::kSuccessDefault
          : FencedFrameCreationOutcome::kSuccessOpaque);

  // Inherit the container size from the FencedFrameConfig, if one is present.
  if (container_size.has_value()) {
    SetContainerSize(*container_size);
  }

  // Handle size freezing.
  // This isn't strictly correct, because the size is frozen on navigation
  // start rather than navigation commit (i.e. if the navigation fails, the
  // size will still be frozen). This is unavoidable in our current
  // implementation, where the embedder freezes the size (because the embedder
  // doesn't/shouldn't know when/if the config navigation commits). This
  // inconsistency should be resolved when we make the browser responsible for
  // size freezing, rather than the embedder.
  if (content_size.has_value()) {
    // Check if the config has a content size specified inside it. If so, we
    // should freeze to that size rather than check the current size.
    // It is nonsensical to ask for the old size freezing behavior (freeze the
    // initial size) while also specifying a content size.
    CHECK(deprecated_should_freeze_initial_size.has_value() &&
          !deprecated_should_freeze_initial_size.value());
    PhysicalSize converted_size(LayoutUnit(content_size->width()),
                                LayoutUnit(content_size->height()));
    FreezeFrameSize(converted_size, /*should_coerce_size=*/false);
  } else {
    if ((!deprecated_should_freeze_initial_size.has_value() &&
         IsValidUrnUuidURL(GURL(url))) ||
        (deprecated_should_freeze_initial_size.has_value() &&
         *deprecated_should_freeze_initial_size)) {
      // If we are using a urn, or if the config is still using the deprecated
      // API, freeze the current size at navigation start (or soon after).
      FreezeCurrentFrameSize();
    } else {
      // Otherwise, make sure the frame size isn't frozen.
      UnfreezeFrameSize();
    }
  }
}

void HTMLFencedFrameElement::NavigateToConfig() {
  CHECK(config_);

  // Prioritize navigating to `config_`'s internal URN if it exists. If so, that
  // means it was created by information from the browser process, and the URN
  // is stored in the `FencedFrameURLMapping`. Otherwise, `config_` was
  // constructed from script and has a user-supplied URL that `this` will
  // navigate to instead.
  KURL url;
  if (config_->urn_uuid(PassKey())) {
    url = config_->urn_uuid(PassKey()).value();
    CHECK(IsValidUrnUuidURL(GURL(url)));
  } else {
    CHECK(config_->url());
    url =
        config_
            ->GetValueIgnoringVisibility<FencedFrameConfig::Attribute::kURL>();
  }
  Navigate(url, config_->deprecated_should_freeze_initial_size(PassKey()),
           config_->container_size(PassKey()), config_->content_size(PassKey()),
           config_->GetSharedStorageContext());
}

void HTMLFencedFrameElement::CreateDelegateAndNavigate() {
  TRACE_EVENT0("navigation",
               "HTMLFencedFrameElement::CreateDelegateAndNavigate");
  // We may queue up several calls to CreateDelegateAndNavigate while
  // prerendering, but we should only actually create the delegate once. Note,
  // this will also mean that we skip calling Navigate() again, but the result
  // should still be correct since the first Navigate call will use the
  // up-to-date config.
  if (frame_delegate_)
    return;
  if (GetDocument().IsPrerendering()) {
    GetDocument().AddPostPrerenderingActivationStep(
        WTF::BindOnce(&HTMLFencedFrameElement::CreateDelegateAndNavigate,
                      WrapWeakPersistent(this)));
    return;
  }

  frame_delegate_ = FencedFrameDelegate::Create(this);

  if (config_) {
    NavigateToConfig();
  }
}

void HTMLFencedFrameElement::AttachLayoutTree(AttachContext& context) {
  HTMLFrameOwnerElement::AttachLayoutTree(context);
  if (frame_delegate_)
    frame_delegate_->AttachLayoutTree();
}

bool HTMLFencedFrameElement::LayoutObjectIsNeeded(
    const DisplayStyle& style) const {
  return !collapsed_by_client_ &&
         HTMLFrameOwnerElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLFencedFrameElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutIFrame>(this);
}

FocusableState HTMLFencedFrameElement::SupportsFocus(UpdateBehavior) const {
  return (frame_delegate_ && frame_delegate_->SupportsFocus())
             ? FocusableState::kFocusable
             : FocusableState::kNotFocusable;
}

PhysicalSize HTMLFencedFrameElement::CoerceFrameSize(
    const PhysicalSize& requested_size) {
  // Only top-level opaque-ads fenced frames are restricted to a list of sizes.
  // TODO(crbug.com/1123606): Later, we will change the size restriction design
  // such that the size is a property bound to opaque URLs, rather than the
  // mode. When that happens, much of this function will need to change.
  // Remember to remove the following includes:
  // #include
  // "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_ad_sizes.h"
  // #include "third_party/blink/renderer/core/frame/local_dom_window.h"
  // #include "third_party/blink/renderer/core/frame/screen.h"
  if (GetDeprecatedMode() !=
          blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds ||
      GetDocument().GetFrame()->IsInFencedFrameTree()) {
    return requested_size;
  }

  // If the requested size is degenerate, return the first allowed ad size.
  if (requested_size.width.ToDouble() <
          std::numeric_limits<double>::epsilon() ||
      requested_size.height.ToDouble() <
          std::numeric_limits<double>::epsilon()) {
    return PhysicalSize(kAllowedAdSizes[0]);
  }

  // If the requested size has an exact match on the allow list, allow it.
  static_assert(kAllowedAdSizes.size() > 0UL);
  for (const gfx::Size& allowed_size : kAllowedAdSizes) {
    if (SizeMatchesExactly(requested_size, allowed_size)) {
      RecordOpaqueFencedFrameSizeCoercion(false);
      return requested_size;
    }
  }

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1123606): For now, only allow screen-width ads on Android.
  // We will improve this condition in the future, to account for all cases
  // e.g. split screen, desktop mode, WebView.
  Document& document = GetDocument();
  int width_for_scaling = document.domWindow() && document.domWindow()->screen()
                              ? document.domWindow()->screen()->availWidth()
                              : 0;

  // If scaling based on screen width is allowed, check for exact matches
  // with the list of heights and aspect ratios.
  if (width_for_scaling > 0) {
    static_assert(kAllowedAdHeights.size() > 0UL);
    for (const int allowed_height : kAllowedAdHeights) {
      if (SizeMatchesExactly(requested_size,
                             {width_for_scaling, allowed_height})) {
        return requested_size;
      }
    }

    static_assert(kAllowedAdAspectRatios.size() > 0UL);
    for (const gfx::Size& allowed_aspect_ratio : kAllowedAdAspectRatios) {
      if (SizeMatchesExactly(
              requested_size,
              {width_for_scaling,
               (width_for_scaling * allowed_aspect_ratio.height()) /
                   allowed_aspect_ratio.width()})) {
        return requested_size;
      }
    }
  }
#endif

  // If the requested size isn't allowed, we will freeze the inner frame
  // element with the nearest available size (the best fit according to our
  // size loss function).
  GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kRendering,
      mojom::blink::ConsoleMessageLevel::kWarning,
      "A fenced frame in opaque-ads mode attempted to load with an "
      "unsupported size, and was therefore rounded to the nearest supported "
      "size."));
  RecordOpaqueFencedFrameSizeCoercion(true);

  // The best size so far, and its loss. A lower loss represents
  // a better fit, so we will find the size that minimizes it, i.e.
  // the least bad size.
  gfx::Size best_size = kAllowedAdSizes[0];
  double best_size_loss = std::numeric_limits<double>::infinity();

  for (const gfx::Size& allowed_size : kAllowedAdSizes) {
    double size_loss = ComputeSizeLossFunction(requested_size, allowed_size);
    if (size_loss < best_size_loss) {
      best_size_loss = size_loss;
      best_size = allowed_size;
    }
  }

#if BUILDFLAG(IS_ANDROID)
  if (width_for_scaling > 0) {
    for (const int allowed_height : kAllowedAdHeights) {
      const gfx::Size allowed_size = {width_for_scaling, allowed_height};
      double size_loss = ComputeSizeLossFunction(requested_size, allowed_size);
      if (size_loss < best_size_loss) {
        best_size_loss = size_loss;
        best_size = allowed_size;
      }
    }

    for (const gfx::Size& allowed_aspect_ratio : kAllowedAdAspectRatios) {
      const gfx::Size allowed_size = {
          width_for_scaling,
          (width_for_scaling * allowed_aspect_ratio.height()) /
              allowed_aspect_ratio.width()};
      double size_loss = ComputeSizeLossFunction(requested_size, allowed_size);
      if (size_loss < best_size_loss) {
        best_size_loss = size_loss;
        best_size = allowed_size;
      }
    }
  }
#endif

  return PhysicalSize(best_size);
}

const std::optional<PhysicalSize> HTMLFencedFrameElement::FrozenFrameSize()
    const {
  if (!frozen_frame_size_)
    return std::nullopt;
  const float ratio = GetDocument().DevicePixelRatio();
  return PhysicalSize(
      LayoutUnit::FromFloatRound(frozen_frame_size_->width * ratio),
      LayoutUnit::FromFloatRound(frozen_frame_size_->height * ratio));
}

void HTMLFencedFrameElement::UnfreezeFrameSize() {
  should_freeze_frame_size_on_next_layout_ = false;

  // If the frame was already unfrozen, we don't need to do anything.
  if (!frozen_frame_size_.has_value()) {
    return;
  }

  // Otherwise, the frame previously had a frozen size. Unfreeze it.
  frozen_frame_size_ = std::nullopt;
  frame_delegate_->MarkFrozenFrameSizeStale();
}

void HTMLFencedFrameElement::FreezeCurrentFrameSize() {
  should_freeze_frame_size_on_next_layout_ = false;

  // If the inner frame size is already frozen to the current outer frame size,
  // we don't need to do anything.
  if (frozen_frame_size_.has_value() && content_rect_.has_value() &&
      content_rect_->size == *frozen_frame_size_) {
    return;
  }

  // Otherwise, we need to change the frozen size of the frame.
  frozen_frame_size_ = std::nullopt;

  // If we know the current outer frame size, freeze the inner frame to it.
  if (content_rect_) {
    FreezeFrameSize(content_rect_->size, /*should_coerce_size=*/true);
    return;
  }

  // Otherwise, we need to wait for the next layout.
  should_freeze_frame_size_on_next_layout_ = true;
}

void HTMLFencedFrameElement::SetContainerSize(const gfx::Size& size) {
  setAttribute(html_names::kWidthAttr, String::Format("%dpx", size.width()),
               ASSERT_NO_EXCEPTION);
  setAttribute(html_names::kHeightAttr, String::Format("%dpx", size.height()),
               ASSERT_NO_EXCEPTION);

  frame_delegate_->MarkContainerSizeStale();
}

void HTMLFencedFrameElement::FreezeFrameSize(const PhysicalSize& size,
                                             bool should_coerce_size) {
  frozen_frame_size_ = size;
  if (should_coerce_size) {
    frozen_frame_size_ = CoerceFrameSize(size);
  }

  frame_delegate_->MarkFrozenFrameSizeStale();
}

void HTMLFencedFrameElement::StartResizeObserver() {
  DCHECK(!resize_observer_);
  resize_observer_ =
      ResizeObserver::Create(GetDocument().domWindow(),
                             MakeGarbageCollected<ResizeObserverDelegate>());
  resize_observer_->observe(this);
}

void HTMLFencedFrameElement::ResizeObserverDelegate::OnResize(
    const HeapVector<Member<ResizeObserverEntry>>& entries) {
  if (entries.empty())
    return;
  const Member<ResizeObserverEntry>& entry = entries.back();
  auto* element = To<HTMLFencedFrameElement>(entry->target());
  const DOMRectReadOnly* content_rect = entry->contentRect();
  element->OnResize(ToPhysicalRect(*content_rect));
}

void HTMLFencedFrameElement::OnResize(const PhysicalRect& content_rect) {
  // If we don't have a delegate, then we won't have a frame, so no reason to
  // freeze.
  if (!frame_delegate_)
    return;
  if (frozen_frame_size_.has_value() && !size_set_after_freeze_) {
    // Only log this once per fenced frame.
    RecordFencedFrameResizedAfterSizeFrozen();
    size_set_after_freeze_ = true;
  }
  content_rect_ = content_rect;

  // If we postponed freezing the frame size until the next layout (in
  // `FreezeCurrentFrameSize`), do it now.
  if (should_freeze_frame_size_on_next_layout_) {
    should_freeze_frame_size_on_next_layout_ = false;
    DCHECK(!frozen_frame_size_);
    FreezeFrameSize(content_rect_->size, /*should_coerce_size=*/true);
  }
}

void HTMLFencedFrameElement::DispatchFencedEvent(
    const WTF::String& event_type) {
  std::optional<WTF::AtomicString> fenced_event_type =
      ConvertEventTypeToFencedEventType(event_type);
  CHECK(fenced_event_type.has_value());
  // Note: This method sets isTrusted = true on the event object, to indicate
  // that the event was dispatched by the browser.
  DispatchEvent(*Event::CreateFenced(*fenced_event_type));
}

// START HTMLFencedFrameElement::FencedFrameDelegate

// static
HTMLFencedFrameElement::FencedFrameDelegate*
HTMLFencedFrameElement::FencedFrameDelegate::Create(
    HTMLFencedFrameElement* outer_element) {
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(
      outer_element->GetExecutionContext()));

  // If the frame embedding a fenced frame is a detached frame, the execution
  // context will be null. That makes it impossible to check the sandbox flags,
  // so delegate creation is stopped if that is the case.
  if (!outer_element->GetExecutionContext()) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame in a detached frame."));
    return nullptr;
  }

  // If the element has been disconnected by the time we attempt to create the
  // delegate (eg, due to deferral while prerendering), we should not create the
  // delegate.
  //
  // NB: this check should remain at the beginning of this function so that the
  // remainder of the function can safely assume the frame is connected.
  if (!outer_element->isConnected()) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame when disconnected."));
    return nullptr;
  }

  if (outer_element->GetExecutionContext()->IsSandboxed(
          kFencedFrameMandatoryUnsandboxedFlags)) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame. A sandboxed document can load fenced "
            "frames only when all of the following permissions are set: "
            "allow-same-origin, allow-forms, allow-scripts, allow-popups, "
            "allow-popups-to-escape-sandbox and "
            "allow-top-navigation-by-user-activation."));
    RecordFencedFrameCreationOutcome(
        FencedFrameCreationOutcome::kSandboxFlagsNotSet);
    RecordFencedFrameUnsandboxedFlags(
        outer_element->GetExecutionContext()->GetSandboxFlags());
    RecordFencedFrameFailedSandboxLoadInTopLevelFrame(
        outer_element->GetDocument().IsInMainFrame());
    return nullptr;
  }

  if (!SubframeLoadingDisabler::CanLoadFrame(*outer_element)) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame. Subframe loading disabled."));
    return nullptr;
  }

  // The frame limit only needs to be checked on initial creation before
  // attempting to insert it into the DOM. This behavior matches how iframes
  // handles frame limits.
  if (!outer_element->IsCurrentlyWithinFrameLimit()) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame. Frame limit exceeded."));
    return nullptr;
  }

  // We must be connected at this point due to the isConnected check at the top
  // of this function.
  DCHECK(outer_element->GetDocument().GetFrame());

  return MakeGarbageCollected<FencedFrameDelegate>(outer_element);
}

HTMLFencedFrameElement::FencedFrameDelegate::FencedFrameDelegate(
    HTMLFencedFrameElement* outer_element)
    : outer_element_(outer_element),
      remote_(GetElement().GetDocument().GetExecutionContext()) {
  DocumentFencedFrames::GetOrCreate(GetElement().GetDocument())
      .RegisterFencedFrame(&GetElement());
  mojo::PendingAssociatedRemote<mojom::blink::FencedFrameOwnerHost> remote;
  mojo::PendingAssociatedReceiver<mojom::blink::FencedFrameOwnerHost> receiver =
      remote.InitWithNewEndpointAndPassReceiver();
  auto task_runner =
      GetElement().GetDocument().GetTaskRunner(TaskType::kInternalDefault);
  remote_.Bind(std::move(remote), task_runner);

  RemoteFrame* remote_frame =
      GetElement().GetDocument().GetFrame()->Client()->CreateFencedFrame(
          &GetElement(), std::move(receiver));
  DCHECK_EQ(remote_frame, GetElement().ContentFrame());
}

void HTMLFencedFrameElement::FencedFrameDelegate::Navigate(
    const KURL& url,
    const String& embedder_shared_storage_context) {
  DCHECK(remote_.get());
  const auto navigation_start_time = base::TimeTicks::Now();
  remote_->Navigate(url, navigation_start_time,
                    embedder_shared_storage_context);
}

void HTMLFencedFrameElement::FencedFrameDelegate::Dispose() {
  DCHECK(remote_.get());
  remote_.reset();
  auto* fenced_frames = DocumentFencedFrames::Get(GetElement().GetDocument());
  DCHECK(fenced_frames);
  fenced_frames->DeregisterFencedFrame(&GetElement());
}

void HTMLFencedFrameElement::FencedFrameDelegate::AttachLayoutTree() {
  if (GetElement().GetLayoutEmbeddedContent() && GetElement().ContentFrame()) {
    GetElement().SetEmbeddedContentView(GetElement().ContentFrame()->View());
  }
}

bool HTMLFencedFrameElement::FencedFrameDelegate::SupportsFocus() {
  return true;
}

void HTMLFencedFrameElement::FencedFrameDelegate::MarkFrozenFrameSizeStale() {
  RemoteFrameView* view =
      DynamicTo<RemoteFrameView>(GetElement().OwnedEmbeddedContentView());
  if (view) {
    view->ResetFrozenSize();
  }
  if (auto* layout_object = GetElement().GetLayoutObject()) {
    layout_object->SetNeedsLayoutAndFullPaintInvalidation(
        "Froze fenced frame content size");
  }
}

void HTMLFencedFrameElement::FencedFrameDelegate::MarkContainerSizeStale() {
  if (auto* layout_object = GetElement().GetLayoutObject()) {
    layout_object->SetNeedsLayoutAndFullPaintInvalidation(
        "Stored fenced frame container size");
  }
}

void HTMLFencedFrameElement::FencedFrameDelegate::DidChangeFramePolicy(
    const FramePolicy& frame_policy) {
  DCHECK(remote_.get());
  remote_->DidChangeFramePolicy(frame_policy);
}

void HTMLFencedFrameElement::FencedFrameDelegate::Trace(
    Visitor* visitor) const {
  visitor->Trace(remote_);
  visitor->Trace(outer_element_);
}

// END HTMLFencedFrameElement::FencedFrameDelegate

}  // namespace blink
