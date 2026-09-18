// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
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
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/fenced_frame/document_fenced_frames.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {



HTMLFencedFrameElement::HTMLFencedFrameElement(Document& document)
    : HTMLFrameOwnerElement(html_names::kFencedframeTag, document),
      sandbox_(MakeGarbageCollected<HTMLIFrameElementSandbox>(this)) {
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(GetExecutionContext()));
  Deprecation::CountDeprecation(GetExecutionContext(),
                                WebFeature::kHTMLFencedFrameElement);
}

HTMLFencedFrameElement::~HTMLFencedFrameElement() = default;

void HTMLFencedFrameElement::Trace(Visitor* visitor) const {
  HTMLFrameOwnerElement::Trace(visitor);
  visitor->Trace(frame_delegate_);
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

network::ParsedPermissionsPolicy
HTMLFencedFrameElement::ConstructContainerPolicy() const {
  if (!GetExecutionContext()) {
    return network::ParsedPermissionsPolicy();
  }

  scoped_refptr<const SecurityOrigin> src_origin =
      MakeOriginForPermissionsPolicy();
  const SecurityOrigin* self_origin =
      GetExecutionContext()->GetSecurityOrigin();

  PolicyParserMessageBuffer logger;

  network::ParsedPermissionsPolicy container_policy =
      PermissionsPolicyParser::ParseAttribute(allow_, *self_origin, *src_origin,
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
    Navigate(BlankUrl());
  }
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
            StrCat({"Error while parsing the 'sandbox' attribute: ",
                    String::FromUtf8(parsed.error_message)})));
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
    HeapVector<CSSPropertyValue, 8>& style) {
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
    std::optional<gfx::Size> container_size,
    std::optional<gfx::Size> content_size) {
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

  frame_delegate_->Navigate(url);

  RecordFencedFrameCreationOutcome(
      mode_ == blink::FencedFrame::DeprecatedFencedFrameMode::kDefault
          ? FencedFrameCreationOutcome::kSuccessDefault
          : FencedFrameCreationOutcome::kSuccessOpaque);

  // Inherit the container size from the FencedFrameConfig, if one is present.
  if (container_size.has_value()) {
    SetContainerSize(*container_size);
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
  Navigate(url, config_->container_size(PassKey()),
           config_->content_size(PassKey()));
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
        BindOnce(&HTMLFencedFrameElement::CreateDelegateAndNavigate,
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

void HTMLFencedFrameElement::SetContainerSize(const gfx::Size& size) {
  setAttribute(html_names::kWidthAttr,
               AtomicString(Format("{}px", size.width())));
  setAttribute(html_names::kHeightAttr,
               AtomicString(Format("{}px", size.height())));

  frame_delegate_->MarkContainerSizeStale();
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
}

void HTMLFencedFrameElement::FencedFrameDelegate::Navigate(const KURL& url) {
  // Navigation is disabled.
}

void HTMLFencedFrameElement::FencedFrameDelegate::Dispose() {
  if (remote_.is_bound()) {
    remote_.reset();
  }
  auto* fenced_frames = DocumentFencedFrames::Get(GetElement().GetDocument());
  if (fenced_frames) {
    fenced_frames->DeregisterFencedFrame(&GetElement());
  }
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
  // Size freezing is disabled.
}

void HTMLFencedFrameElement::FencedFrameDelegate::MarkContainerSizeStale() {
  if (auto* layout_object = GetElement().GetLayoutObject()) {
    layout_object->SetNeedsLayoutAndFullPaintInvalidation(
        "Stored fenced frame container size");
  }
}

void HTMLFencedFrameElement::FencedFrameDelegate::DidChangeFramePolicy(
    const FramePolicy& frame_policy) {
  // Policy changes are a no-op as the frame is a stub.
}

void HTMLFencedFrameElement::FencedFrameDelegate::Trace(
    Visitor* visitor) const {
  visitor->Trace(remote_);
  visitor->Trace(outer_element_);
}

// END HTMLFencedFrameElement::FencedFrameDelegate

}  // namespace blink
