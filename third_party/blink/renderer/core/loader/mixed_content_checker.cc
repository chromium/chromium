/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/public/platform/web_mixed_content.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_fetch_context.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_settings.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// When a frame is local, use its full URL to represent the main resource. When
// the frame is remote, the full URL isn't accessible, so use the origin. This
// function is used, for example, to determine the URL to show in console
// messages about mixed content.
KURL MainResourceUrlForFrame(Frame* frame) {
  if (frame->IsRemoteFrame()) {
    return KURL(NullURL(),
                frame->GetSecurityContext()->GetSecurityOrigin()->ToString());
  }
  return To<LocalFrame>(frame)->GetDocument()->Url();
}

const char* RequestContextName(mojom::RequestContextType context) {
  switch (context) {
    case mojom::RequestContextType::AUDIO:
      return "audio file";
    case mojom::RequestContextType::BEACON:
      return "Beacon endpoint";
    case mojom::RequestContextType::CSP_REPORT:
      return "Content Security Policy reporting endpoint";
    case mojom::RequestContextType::DOWNLOAD:
      return "download";
    case mojom::RequestContextType::EMBED:
      return "plugin resource";
    case mojom::RequestContextType::EVENT_SOURCE:
      return "EventSource endpoint";
    case mojom::RequestContextType::FAVICON:
      return "favicon";
    case mojom::RequestContextType::FETCH:
      return "resource";
    case mojom::RequestContextType::FONT:
      return "font";
    case mojom::RequestContextType::FORM:
      return "form action";
    case mojom::RequestContextType::FRAME:
      return "frame";
    case mojom::RequestContextType::HYPERLINK:
      return "resource";
    case mojom::RequestContextType::IFRAME:
      return "frame";
    case mojom::RequestContextType::IMAGE:
      return "image";
    case mojom::RequestContextType::IMAGE_SET:
      return "image";
    case mojom::RequestContextType::IMPORT:
      return "HTML Import";
    case mojom::RequestContextType::INTERNAL:
      return "resource";
    case mojom::RequestContextType::LOCATION:
      return "resource";
    case mojom::RequestContextType::MANIFEST:
      return "manifest";
    case mojom::RequestContextType::OBJECT:
      return "plugin resource";
    case mojom::RequestContextType::PING:
      return "hyperlink auditing endpoint";
    case mojom::RequestContextType::PLUGIN:
      return "plugin data";
    case mojom::RequestContextType::PREFETCH:
      return "prefetch resource";
    case mojom::RequestContextType::SCRIPT:
      return "script";
    case mojom::RequestContextType::SERVICE_WORKER:
      return "Service Worker script";
    case mojom::RequestContextType::SHARED_WORKER:
      return "Shared Worker script";
    case mojom::RequestContextType::STYLE:
      return "stylesheet";
    case mojom::RequestContextType::SUBRESOURCE:
      return "resource";
    case mojom::RequestContextType::TRACK:
      return "Text Track";
    case mojom::RequestContextType::UNSPECIFIED:
      return "resource";
    case mojom::RequestContextType::VIDEO:
      return "video";
    case mojom::RequestContextType::WORKER:
      return "Worker script";
    case mojom::RequestContextType::XML_HTTP_REQUEST:
      return "XMLHttpRequest endpoint";
    case mojom::RequestContextType::XSLT:
      return "XSLT";
  }
  NOTREACHED();
  return "resource";
}

// Currently we have two slightly different versions, because
// in frames SecurityContext is the source of CSP/InsecureRequestPolicy,
// especially where FetchContext and SecurityContext come from different
// frames (e.g. in nested frames), while in
// workers we should totally rely on FetchContext's FetchClientSettingsObject
// to avoid confusion around off-the-main-thread fetch.
// TODO(hiroshige): Consider merging them once FetchClientSettingsObject
// becomes the source of CSP/InsecureRequestPolicy also in frames.
bool IsWebSocketAllowedInFrame(const BaseFetchContext& fetch_context,
                               SecurityContext* security_context,
                               Settings* settings,
                               const KURL& url) {
  fetch_context.CountUsage(WebFeature::kMixedContentPresent);
  fetch_context.CountUsage(WebFeature::kMixedContentWebSocket);
  if (ContentSecurityPolicy* policy =
          security_context->GetContentSecurityPolicy()) {
    policy->ReportMixedContent(url,
                               ResourceRequest::RedirectStatus::kNoRedirect);
  }

  // If we're in strict mode, we'll automagically fail everything, and
  // intentionally skip the client checks in order to prevent degrading the
  // site's security UI.
  bool strict_mode =
      security_context->GetInsecureRequestPolicy() & kBlockAllMixedContent ||
      settings->GetStrictMixedContentChecking();
  if (strict_mode)
    return false;
  return settings && settings->GetAllowRunningOfInsecureContent();
}

bool IsWebSocketAllowedInWorker(const BaseFetchContext& fetch_context,
                                WorkerSettings* settings,
                                const KURL& url) {
  fetch_context.CountUsage(WebFeature::kMixedContentPresent);
  fetch_context.CountUsage(WebFeature::kMixedContentWebSocket);
  if (const ContentSecurityPolicy* policy =
          fetch_context.GetContentSecurityPolicy()) {
    policy->ReportMixedContent(url,
                               ResourceRequest::RedirectStatus::kNoRedirect);
  }

  // If we're in strict mode, we'll automagically fail everything, and
  // intentionally skip the client checks in order to prevent degrading the
  // site's security UI.
  bool strict_mode = fetch_context.GetResourceFetcherProperties()
                             .GetFetchClientSettingsObject()
                             .GetInsecureRequestsPolicy() &
                         kBlockAllMixedContent ||
                     settings->GetStrictMixedContentChecking();
  if (strict_mode)
    return false;
  return settings && settings->GetAllowRunningOfInsecureContent();
}

}  // namespace

static void MeasureStricterVersionOfIsMixedContent(Frame& frame,
                                                   const KURL& url,
                                                   const LocalFrame* source) {
  // We're currently only checking for mixed content in `https://*` contexts.
  // What about other "secure" contexts the SchemeRegistry knows about? We'll
  // use this method to measure the occurrence of non-webby mixed content to
  // make sure we're not breaking the world without realizing it.
  const SecurityOrigin* origin =
      frame.GetSecurityContext()->GetSecurityOrigin();
  if (MixedContentChecker::IsMixedContent(origin, url)) {
    if (origin->Protocol() != "https") {
      UseCounter::Count(
          source->GetDocument(),
          WebFeature::kMixedContentInNonHTTPSFrameThatRestrictsMixedContent);
    }
  } else if (!SecurityOrigin::IsSecure(url) &&
             SchemeRegistry::ShouldTreatURLSchemeAsSecure(origin->Protocol())) {
    UseCounter::Count(
        source->GetDocument(),
        WebFeature::kMixedContentInSecureFrameThatDoesNotRestrictMixedContent);
  }
}

bool RequestIsSubframeSubresource(Frame* frame) {
  return frame && frame != frame->Tree().Top();
}

static bool IsInsecureUrl(const KURL& url) {
  // |url| is mixed content if its origin is not potentially trustworthy nor
  // secure. We do a quick check against `SecurityOrigin::IsSecure` to catch
  // things like `about:blank`, which cannot be sanely passed into
  // `SecurityOrigin::Create` (as their origin depends on their context).
  // blob: and filesystem: URLs never hit the network, and access is restricted
  // to same-origin contexts, so they are not blocked either.
  bool is_allowed = url.ProtocolIs("blob") || url.ProtocolIs("filesystem") ||
                    SecurityOrigin::IsSecure(url) ||
                    SecurityOrigin::Create(url)->IsPotentiallyTrustworthy();
  return !is_allowed;
}

// static
bool MixedContentChecker::IsMixedContent(const SecurityOrigin* security_origin,
                                         const KURL& url) {
  if (!SchemeRegistry::ShouldTreatURLSchemeAsRestrictingMixedContent(
          security_origin->Protocol()))
    return false;

  return IsInsecureUrl(url);
}

// static
bool MixedContentChecker::IsMixedContent(
    const FetchClientSettingsObject& settings,
    const KURL& url) {
  switch (settings.GetHttpsState()) {
    case HttpsState::kNone:
      return false;

    case HttpsState::kModern:
      return IsInsecureUrl(url);
  }
}

// static
Frame* MixedContentChecker::InWhichFrameIsContentMixed(LocalFrame* frame,
                                                       const KURL& url) {
  // Frameless requests cannot be mixed content.
  if (!frame)
    return nullptr;

  // Check the top frame first.
  Frame& top = frame->Tree().Top();
  MeasureStricterVersionOfIsMixedContent(top, url, frame);
  if (IsMixedContent(top.GetSecurityContext()->GetSecurityOrigin(), url))
    return &top;

  MeasureStricterVersionOfIsMixedContent(*frame, url, frame);
  if (IsMixedContent(frame->GetSecurityContext()->GetSecurityOrigin(), url))
    return frame;

  // No mixed content, no problem.
  return nullptr;
}

// static
ConsoleMessage* MixedContentChecker::CreateConsoleMessageAboutFetch(
    const KURL& main_resource_url,
    const KURL& url,
    mojom::RequestContextType request_context,
    bool allowed,
    std::unique_ptr<SourceLocation> source_location) {
  String message = String::Format(
      "Mixed Content: The page at '%s' was loaded over HTTPS, but requested an "
      "insecure %s '%s'. %s",
      main_resource_url.ElidedString().Utf8().c_str(),
      RequestContextName(request_context), url.ElidedString().Utf8().c_str(),
      allowed ? "This content should also be served over HTTPS."
              : "This request has been blocked; the content must be served "
                "over HTTPS.");
  mojom::ConsoleMessageLevel message_level =
      allowed ? mojom::ConsoleMessageLevel::kWarning
              : mojom::ConsoleMessageLevel::kError;
  if (source_location) {
    return ConsoleMessage::Create(mojom::ConsoleMessageSource::kSecurity,
                                  message_level, message,
                                  std::move(source_location));
  }
  return ConsoleMessage::Create(mojom::ConsoleMessageSource::kSecurity,
                                message_level, message);
}

// static
void MixedContentChecker::Count(Frame* frame,
                                mojom::RequestContextType request_context,
                                const LocalFrame* source) {
  UseCounter::Count(source->GetDocument(), WebFeature::kMixedContentPresent);

  // Roll blockable content up into a single counter, count unblocked types
  // individually so we can determine when they can be safely moved to the
  // blockable category:
  WebMixedContentContextType context_type =
      WebMixedContent::ContextTypeFromRequestContext(
          request_context,
          frame->GetSettings()->GetStrictMixedContentCheckingForPlugin());
  if (context_type == WebMixedContentContextType::kBlockable) {
    UseCounter::Count(source->GetDocument(),
                      WebFeature::kMixedContentBlockable);
    return;
  }

  WebFeature feature;
  switch (request_context) {
    case mojom::RequestContextType::AUDIO:
      feature = WebFeature::kMixedContentAudio;
      break;
    case mojom::RequestContextType::DOWNLOAD:
      feature = WebFeature::kMixedContentDownload;
      break;
    case mojom::RequestContextType::FAVICON:
      feature = WebFeature::kMixedContentFavicon;
      break;
    case mojom::RequestContextType::IMAGE:
      feature = WebFeature::kMixedContentImage;
      break;
    case mojom::RequestContextType::INTERNAL:
      feature = WebFeature::kMixedContentInternal;
      break;
    case mojom::RequestContextType::PLUGIN:
      feature = WebFeature::kMixedContentPlugin;
      break;
    case mojom::RequestContextType::PREFETCH:
      feature = WebFeature::kMixedContentPrefetch;
      break;
    case mojom::RequestContextType::VIDEO:
      feature = WebFeature::kMixedContentVideo;
      break;

    default:
      NOTREACHED();
      return;
  }
  UseCounter::Count(source->GetDocument(), feature);
}

// static
bool MixedContentChecker::ShouldBlockFetch(
    LocalFrame* frame,
    mojom::RequestContextType request_context,
    ResourceRequest::RedirectStatus redirect_status,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) {
  Frame* mixed_frame = InWhichFrameIsContentMixed(frame, url);
  if (!mixed_frame)
    return false;

  MixedContentChecker::Count(mixed_frame, request_context, frame);
  if (ContentSecurityPolicy* policy =
          frame->GetSecurityContext()->GetContentSecurityPolicy())
    policy->ReportMixedContent(url, redirect_status);

  Settings* settings = mixed_frame->GetSettings();
  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  LocalFrameClient* client = frame->Client();
  auto& local_frame_host = frame->GetLocalFrameHostRemote();
  WebContentSettingsClient* content_settings_client =
      frame->GetContentSettingsClient();
  const SecurityOrigin* security_origin =
      mixed_frame->GetSecurityContext()->GetSecurityOrigin();
  bool allowed = false;

  // If we're in strict mode, we'll automagically fail everything, and
  // intentionally skip the client checks in order to prevent degrading the
  // site's security UI.
  bool strict_mode =
      mixed_frame->GetSecurityContext()->GetInsecureRequestPolicy() &
          kBlockAllMixedContent ||
      settings->GetStrictMixedContentChecking();

  WebMixedContentContextType context_type =
      WebMixedContent::ContextTypeFromRequestContext(
          request_context, settings->GetStrictMixedContentCheckingForPlugin());

  // If we're loading the main resource of a subframe, we need to take a close
  // look at the loaded URL. If we're dealing with a CORS-enabled scheme, then
  // block mixed frames as active content. Otherwise, treat frames as passive
  // content.
  switch (context_type) {
    case WebMixedContentContextType::kOptionallyBlockable:
      allowed = !strict_mode;
      if (allowed) {
        if (content_settings_client)
          content_settings_client->PassiveInsecureContentFound(url);
        local_frame_host.DidDisplayInsecureContent();
      }
      break;

    case WebMixedContentContextType::kBlockable: {
      // Strictly block subresources that are mixed with respect to their
      // subframes, unless all insecure content is allowed. This is to avoid the
      // following situation: https://a.com embeds https://b.com, which loads a
      // script over insecure HTTP. The user opts to allow the insecure content,
      // thinking that they are allowing an insecure script to run on
      // https://a.com and not realizing that they are in fact allowing an
      // insecure script on https://b.com.
      if (!settings->GetAllowRunningOfInsecureContent() &&
          RequestIsSubframeSubresource(frame) &&
          IsMixedContent(frame->GetSecurityContext()->GetSecurityOrigin(),
                         url)) {
        UseCounter::Count(frame->GetDocument(),
                          WebFeature::kBlockableMixedContentInSubframeBlocked);
        allowed = false;
        break;
      }

      bool should_ask_embedder =
          !strict_mode && settings &&
          (!settings->GetStrictlyBlockBlockableMixedContent() ||
           settings->GetAllowRunningOfInsecureContent());
      if (should_ask_embedder) {
        allowed = settings && settings->GetAllowRunningOfInsecureContent();
        if (content_settings_client) {
          allowed = content_settings_client->AllowRunningInsecureContent(
              allowed, url);
        }
      }
      if (allowed) {
        client->DidRunInsecureContent(security_origin, url);
        UseCounter::Count(frame->GetDocument(),
                          WebFeature::kMixedContentBlockableAllowed);
      }
      break;
    }

    case WebMixedContentContextType::kShouldBeBlockable:
      allowed = !strict_mode;
      if (allowed)
        local_frame_host.DidDisplayInsecureContent();
      break;
    case WebMixedContentContextType::kNotMixedContent:
      NOTREACHED();
      break;
  };

  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    frame->GetDocument()->AddConsoleMessage(
        CreateConsoleMessageAboutFetch(MainResourceUrlForFrame(mixed_frame),
                                       url, request_context, allowed, nullptr));
  }
  return !allowed;
}

// static
bool MixedContentChecker::ShouldBlockFetchOnWorker(
    const WorkerFetchContext& worker_fetch_context,
    mojom::RequestContextType request_context,
    ResourceRequest::RedirectStatus redirect_status,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy,
    bool is_worklet_global_scope) {
  const FetchClientSettingsObject& fetch_client_settings_object =
      worker_fetch_context.GetResourceFetcherProperties()
          .GetFetchClientSettingsObject();
  if (!MixedContentChecker::IsMixedContent(fetch_client_settings_object, url)) {
    return false;
  }

  worker_fetch_context.CountUsage(WebFeature::kMixedContentPresent);
  worker_fetch_context.CountUsage(WebFeature::kMixedContentBlockable);
  if (auto* policy = worker_fetch_context.GetContentSecurityPolicy())
    policy->ReportMixedContent(url, redirect_status);

  // Blocks all mixed content request from worklets.
  // TODO(horo): Revise this when the spec is updated.
  // Worklets spec: https://www.w3.org/TR/worklets-1/#security-considerations
  // Spec issue: https://github.com/w3c/css-houdini-drafts/issues/92
  if (is_worklet_global_scope)
    return true;

  WorkerSettings* settings = worker_fetch_context.GetWorkerSettings();
  DCHECK(settings);
  bool allowed = false;
  if (!settings->GetAllowRunningOfInsecureContent() &&
      worker_fetch_context.GetWebWorkerFetchContext()->IsOnSubframe()) {
    worker_fetch_context.CountUsage(
        WebFeature::kBlockableMixedContentInSubframeBlocked);
    allowed = false;
  } else {
    bool strict_mode =
        fetch_client_settings_object.GetInsecureRequestsPolicy() &
            kBlockAllMixedContent ||
        settings->GetStrictMixedContentChecking();
    bool should_ask_embedder =
        !strict_mode && (!settings->GetStrictlyBlockBlockableMixedContent() ||
                         settings->GetAllowRunningOfInsecureContent());
    allowed = should_ask_embedder &&
              worker_fetch_context.AllowRunningInsecureContent(
                  settings->GetAllowRunningOfInsecureContent(), url);
    if (allowed) {
      worker_fetch_context.GetWebWorkerFetchContext()->DidRunInsecureContent(
          WebSecurityOrigin(fetch_client_settings_object.GetSecurityOrigin()),
          url);
      worker_fetch_context.CountUsage(
          WebFeature::kMixedContentBlockableAllowed);
    }
  }

  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    worker_fetch_context.AddConsoleMessage(CreateConsoleMessageAboutFetch(
        worker_fetch_context.Url(), url, request_context, allowed, nullptr));
  }
  return !allowed;
}

// static
ConsoleMessage* MixedContentChecker::CreateConsoleMessageAboutWebSocket(
    const KURL& main_resource_url,
    const KURL& url,
    bool allowed) {
  String message = String::Format(
      "Mixed Content: The page at '%s' was loaded over HTTPS, but attempted to "
      "connect to the insecure WebSocket endpoint '%s'. %s",
      main_resource_url.ElidedString().Utf8().c_str(),
      url.ElidedString().Utf8().c_str(),
      allowed ? "This endpoint should be available via WSS. Insecure access is "
                "deprecated."
              : "This request has been blocked; this endpoint must be "
                "available over WSS.");
  mojom::ConsoleMessageLevel message_level =
      allowed ? mojom::ConsoleMessageLevel::kWarning
              : mojom::ConsoleMessageLevel::kError;
  return ConsoleMessage::Create(mojom::ConsoleMessageSource::kSecurity,
                                message_level, message);
}

// static
bool MixedContentChecker::IsWebSocketAllowed(
    const FrameFetchContext& frame_fetch_context,
    LocalFrame* frame,
    const KURL& url) {
  Frame* mixed_frame = InWhichFrameIsContentMixed(frame, url);
  if (!mixed_frame)
    return true;

  Settings* settings = mixed_frame->GetSettings();
  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  WebContentSettingsClient* content_settings_client =
      frame->GetContentSettingsClient();
  SecurityContext* security_context = mixed_frame->GetSecurityContext();
  const SecurityOrigin* security_origin = security_context->GetSecurityOrigin();

  bool allowed = IsWebSocketAllowedInFrame(frame_fetch_context,
                                           security_context, settings, url);
  if (content_settings_client) {
    allowed =
        content_settings_client->AllowRunningInsecureContent(allowed, url);
  }

  if (allowed)
    frame->Client()->DidRunInsecureContent(security_origin, url);

  frame->GetDocument()->AddConsoleMessage(CreateConsoleMessageAboutWebSocket(
      MainResourceUrlForFrame(mixed_frame), url, allowed));

  return allowed;
}

// static
bool MixedContentChecker::IsWebSocketAllowed(
    const WorkerFetchContext& worker_fetch_context,
    const KURL& url) {
  const FetchClientSettingsObject& fetch_client_settings_object =
      worker_fetch_context.GetResourceFetcherProperties()
          .GetFetchClientSettingsObject();
  if (!MixedContentChecker::IsMixedContent(fetch_client_settings_object, url)) {
    return true;
  }

  WorkerSettings* settings = worker_fetch_context.GetWorkerSettings();
  const SecurityOrigin* security_origin =
      fetch_client_settings_object.GetSecurityOrigin();

  bool allowed =
      IsWebSocketAllowedInWorker(worker_fetch_context, settings, url);
  allowed = worker_fetch_context.AllowRunningInsecureContent(allowed, url);

  if (allowed) {
    worker_fetch_context.GetWebWorkerFetchContext()->DidRunInsecureContent(
        WebSecurityOrigin(security_origin), url);
  }

  worker_fetch_context.AddConsoleMessage(CreateConsoleMessageAboutWebSocket(
      worker_fetch_context.Url(), url, allowed));

  return allowed;
}

bool MixedContentChecker::IsMixedFormAction(
    LocalFrame* frame,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) {
  // For whatever reason, some folks handle forms via JavaScript, and submit to
  // `javascript:void(0)` rather than calling `preventDefault()`. We
  // special-case `javascript:` URLs here, as they don't introduce MixedContent
  // for form submissions.
  if (url.ProtocolIs("javascript"))
    return false;

  Frame* mixed_frame = InWhichFrameIsContentMixed(frame, url);
  if (!mixed_frame)
    return false;

  UseCounter::Count(frame->GetDocument(), WebFeature::kMixedContentPresent);

  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  frame->GetLocalFrameHostRemote().DidContainInsecureFormAction();

  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    String message = String::Format(
        "Mixed Content: The page at '%s' was loaded over a secure connection, "
        "but contains a form that targets an insecure endpoint '%s'. This "
        "endpoint should be made available over a secure connection.",
        MainResourceUrlForFrame(mixed_frame).ElidedString().Utf8().c_str(),
        url.ElidedString().Utf8().c_str());
    frame->GetDocument()->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kSecurity,
                               mojom::ConsoleMessageLevel::kWarning, message));
  }

  return true;
}

bool MixedContentChecker::ShouldAutoupgrade(HttpsState context_https_state,
                                            WebMixedContentContextType type) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kMixedContentAutoupgrade) ||
      context_https_state == HttpsState::kNone ||
      type == WebMixedContentContextType::kNotMixedContent) {
    return false;
  }

  auto autoupgrade_mode = base::GetFieldTrialParamValueByFeature(
      blink::features::kMixedContentAutoupgrade,
      blink::features::kMixedContentAutoupgradeModeParamName);

  if (autoupgrade_mode ==
      blink::features::kMixedContentAutoupgradeModeBlockable) {
    return type == WebMixedContentContextType::kBlockable ||
           type == WebMixedContentContextType::kShouldBeBlockable;
  }
  if (autoupgrade_mode ==
      blink::features::kMixedContentAutoupgradeModeOptionallyBlockable) {
    return type == WebMixedContentContextType::kOptionallyBlockable;
  }

  // Otherwise we default to autoupgrading all mixed content.
  return true;
}

void MixedContentChecker::CheckMixedPrivatePublic(
    LocalFrame* frame,
    const AtomicString& resource_ip_address) {
  if (!frame || !frame->GetDocument() || !frame->GetDocument()->Loader())
    return;

  // Just count these for the moment, don't block them.
  if (network_utils::IsReservedIPAddress(resource_ip_address) &&
      frame->GetDocument()->AddressSpace() ==
          network::mojom::IPAddressSpace::kPublic) {
    UseCounter::Count(frame->GetDocument(),
                      WebFeature::kMixedContentPrivateHostnameInPublicHostname);
    // We can simplify the IP checks here, as we've already verified that
    // |resourceIPAddress| is a reserved IP address, which means it's also a
    // valid IP address in a normalized form.
    if (resource_ip_address.StartsWith("127.0.0.") ||
        resource_ip_address == "[::1]") {
      UseCounter::Count(frame->GetDocument(),
                        frame->GetDocument()->IsSecureContext()
                            ? WebFeature::kLoopbackEmbeddedInSecureContext
                            : WebFeature::kLoopbackEmbeddedInNonSecureContext);
    }
  }
}

void MixedContentChecker::HandleCertificateError(
    LocalFrame* frame,
    const ResourceResponse& response,
    mojom::RequestContextType request_context) {
  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  LocalFrameClient* client = frame->Client();
  bool strict_mixed_content_checking_for_plugin =
      frame->GetSettings() &&
      frame->GetSettings()->GetStrictMixedContentCheckingForPlugin();
  WebMixedContentContextType context_type =
      WebMixedContent::ContextTypeFromRequestContext(
          request_context, strict_mixed_content_checking_for_plugin);
  if (context_type == WebMixedContentContextType::kBlockable) {
    client->DidRunContentWithCertificateErrors();
  } else {
    // contextTypeFromRequestContext() never returns NotMixedContent (it
    // computes the type of mixed content, given that the content is mixed).
    DCHECK_NE(context_type, WebMixedContentContextType::kNotMixedContent);
    client->DidDisplayContentWithCertificateErrors();
  }
}

// static
void MixedContentChecker::MixedContentFound(
    LocalFrame* frame,
    const KURL& main_resource_url,
    const KURL& mixed_content_url,
    mojom::RequestContextType request_context,
    bool was_allowed,
    bool had_redirect,
    std::unique_ptr<SourceLocation> source_location) {
  // Logs to the frame console.
  frame->GetDocument()->AddConsoleMessage(CreateConsoleMessageAboutFetch(
      main_resource_url, mixed_content_url, request_context, was_allowed,
      std::move(source_location)));
  // Reports to the CSP policy.
  ContentSecurityPolicy* policy =
      frame->GetSecurityContext()->GetContentSecurityPolicy();
  if (policy) {
    policy->ReportMixedContent(
        mixed_content_url,
        had_redirect ? ResourceRequest::RedirectStatus::kFollowedRedirect
                     : ResourceRequest::RedirectStatus::kNoRedirect);
  }
}

// static
ConsoleMessage* MixedContentChecker::CreateConsoleMessageAboutFetchAutoupgrade(
    const KURL& main_resource_url,
    const KURL& mixed_content_url) {
  String message = String::Format(
      "Mixed Content: The page at '%s' was loaded over HTTPS, but requested an "
      "insecure element '%s'. This request was "
      "automatically upgraded to HTTPS, For more information see "
      "https://chromium.googlesource.com/chromium/src/+/master/docs/security/"
      "autoupgrade-mixed.md",
      main_resource_url.ElidedString().Utf8().c_str(),
      mixed_content_url.ElidedString().Utf8().c_str());
  return ConsoleMessage::Create(mojom::ConsoleMessageSource::kSecurity,
                                mojom::ConsoleMessageLevel::kWarning, message);
}

// static
ConsoleMessage*
MixedContentChecker::CreateConsoleMessageAboutWebSocketAutoupgrade(
    const KURL& main_resource_url,
    const KURL& mixed_content_url) {
  String message = String::Format(
      "Mixed Content: The page at '%s' was loaded over HTTPS, but attempted "
      "to connect to the insecure WebSocket endpoint '%s'. "
      "This request was automatically upgraded to HTTPS, For more "
      "information see "
      "https://chromium.googlesource.com/chromium/src/+/master/docs/security/"
      "autoupgrade-mixed.md",
      main_resource_url.ElidedString().Utf8().c_str(),
      mixed_content_url.ElidedString().Utf8().c_str());
  return ConsoleMessage::Create(mojom::ConsoleMessageSource::kSecurity,
                                mojom::ConsoleMessageLevel::kWarning, message);
}

WebMixedContentContextType MixedContentChecker::ContextTypeForInspector(
    LocalFrame* frame,
    const ResourceRequest& request) {
  Frame* mixed_frame = InWhichFrameIsContentMixed(frame, request.Url());
  if (!mixed_frame)
    return WebMixedContentContextType::kNotMixedContent;

  bool strict_mixed_content_checking_for_plugin =
      mixed_frame->GetSettings() &&
      mixed_frame->GetSettings()->GetStrictMixedContentCheckingForPlugin();
  return WebMixedContent::ContextTypeFromRequestContext(
      request.GetRequestContext(), strict_mixed_content_checking_for_plugin);
}

// static
void MixedContentChecker::UpgradeInsecureRequest(
    ResourceRequest& resource_request,
    const FetchClientSettingsObject* fetch_client_settings_object,
    ExecutionContext* execution_context_for_logging,
    network::mojom::RequestContextFrameType frame_type) {
  // We always upgrade requests that meet any of the following criteria:
  //  1. Are for subresources.
  //  2. Are for nested frames.
  //  3. Are form submissions.
  //  4. Whose hosts are contained in the origin_context's upgrade insecure
  //     navigations set.

  // This happens for:
  // * Browser initiated main document loading. No upgrade required.
  // * Navigation initiated by a frame in another process. URL should have
  //   already been upgraded in the initiator's process.
  if (!execution_context_for_logging)
    return;

  DCHECK(fetch_client_settings_object);

  if (!(fetch_client_settings_object->GetInsecureRequestsPolicy() &
        kUpgradeInsecureRequests)) {
    mojom::RequestContextType context = resource_request.GetRequestContext();
    // TODO(carlosil): Handle strict_mixed_content_checking_for_plugin
    // correctly.
    if (context != mojom::RequestContextType::UNSPECIFIED &&
        resource_request.Url().ProtocolIs("http") &&
        !fetch_client_settings_object->GetMixedAutoUpgradeOptOut() &&
        MixedContentChecker::ShouldAutoupgrade(
            fetch_client_settings_object->GetHttpsState(),
            WebMixedContent::ContextTypeFromRequestContext(context, false))) {
      if (execution_context_for_logging->IsDocument()) {
        Document* document =
            static_cast<Document*>(execution_context_for_logging);
        document->AddConsoleMessage(
            MixedContentChecker::CreateConsoleMessageAboutFetchAutoupgrade(
                fetch_client_settings_object->GlobalObjectUrl(),
                resource_request.Url()));
        resource_request.SetUkmSourceId(document->UkmSourceID());
      }
      resource_request.SetIsAutomaticUpgrade(true);
    } else {
      return;
    }
  }

  // Nested frames are always upgraded on the browser process.
  if (frame_type == network::mojom::RequestContextFrameType::kNested)
    return;

  // We set the UpgradeIfInsecure flag even if the current request wasn't
  // upgraded (due to already being HTTPS), since we still need to upgrade
  // redirects if they are not to HTTPS URLs.
  resource_request.SetUpgradeIfInsecure(true);

  KURL url = resource_request.Url();

  if (!url.ProtocolIs("http") ||
      SecurityOrigin::Create(url)->IsPotentiallyTrustworthy()) {
    return;
  }

  if (frame_type == network::mojom::RequestContextFrameType::kNone ||
      resource_request.GetRequestContext() == mojom::RequestContextType::FORM ||
      (!url.Host().IsNull() &&
       fetch_client_settings_object->GetUpgradeInsecureNavigationsSet()
           .Contains(url.Host().Impl()->GetHash()))) {
    if (!resource_request.IsAutomaticUpgrade()) {
      // These UseCounters are specific for UpgradeInsecureRequests, don't log
      // for autoupgrades.
      mojom::RequestContextType context = resource_request.GetRequestContext();
      if (context == mojom::RequestContextType::UNSPECIFIED) {
        UseCounter::Count(
            execution_context_for_logging,
            WebFeature::kUpgradeInsecureRequestsUpgradedRequestUnknown);
      } else {
        WebMixedContentContextType content_type =
            WebMixedContent::ContextTypeFromRequestContext(context, false);
        switch (content_type) {
          case WebMixedContentContextType::kOptionallyBlockable:
            UseCounter::Count(
                execution_context_for_logging,
                WebFeature::
                    kUpgradeInsecureRequestsUpgradedRequestOptionallyBlockable);
            break;
          case WebMixedContentContextType::kBlockable:
          case WebMixedContentContextType::kShouldBeBlockable:
            UseCounter::Count(
                execution_context_for_logging,
                WebFeature::kUpgradeInsecureRequestsUpgradedRequestBlockable);
            break;
          case WebMixedContentContextType::kNotMixedContent:
            NOTREACHED();
        }
      }
    }
    url.SetProtocol("https");
    if (url.Port() == 80)
      url.SetPort(443);
    resource_request.SetUrl(url);
  }
}

}  // namespace blink
