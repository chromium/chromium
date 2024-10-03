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

#include <optional>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/security_context/insecure_request_policy.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_fetch_context.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_settings.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/mixed_content.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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

const char* RequestContextName(mojom::blink::RequestContextType context) {
  switch (context) {
    case mojom::blink::RequestContextType::ATTRIBUTION_SRC:
      return "attribution src endpoint";
    case mojom::blink::RequestContextType::AUDIO:
      return "audio file";
    case mojom::blink::RequestContextType::BEACON:
      return "Beacon endpoint";
    case mojom::blink::RequestContextType::CSP_REPORT:
      return "Content Security Policy reporting endpoint";
    case mojom::blink::RequestContextType::DOWNLOAD:
      return "download";
    case mojom::blink::RequestContextType::EMBED:
      return "plugin resource";
    case mojom::blink::RequestContextType::EVENT_SOURCE:
      return "EventSource endpoint";
    case mojom::blink::RequestContextType::FAVICON:
      return "favicon";
    case mojom::blink::RequestContextType::FETCH:
      return "resource";
    case mojom::blink::RequestContextType::FONT:
      return "font";
    case mojom::blink::RequestContextType::FORM:
      return "form action";
    case mojom::blink::RequestContextType::FRAME:
      return "frame";
    case mojom::blink::RequestContextType::HYPERLINK:
      return "resource";
    case mojom::blink::RequestContextType::IFRAME:
      return "frame";
    case mojom::blink::RequestContextType::IMAGE:
      return "image";
    case mojom::blink::RequestContextType::IMAGE_SET:
      return "image";
    case mojom::blink::RequestContextType::INTERNAL:
      return "resource";
    case mojom::blink::RequestContextType::LOCATION:
      return "resource";
    case mojom::blink::RequestContextType::JSON:
      return "json";
    case mojom::blink::RequestContextType::MANIFEST:
      return "manifest";
    case mojom::blink::RequestContextType::OBJECT:
      return "plugin resource";
    case mojom::blink::RequestContextType::PING:
      return "hyperlink auditing endpoint";
    case mojom::blink::RequestContextType::PLUGIN:
      return "plugin data";
    case mojom::blink::RequestContextType::PREFETCH:
      return "prefetch resource";
    case mojom::blink::RequestContextType::SCRIPT:
      return "script";
    case mojom::blink::RequestContextType::SERVICE_WORKER:
      return "Service Worker script";
    case mojom::blink::RequestContextType::SHARED_WORKER:
      return "Shared Worker script";
    case mojom::blink::RequestContextType::SPECULATION_RULES:
      return "speculation rules";
    case mojom::blink::RequestContextType::STYLE:
      return "stylesheet";
    case mojom::blink::RequestContextType::SUBRESOURCE:
      return "resource";
    case mojom::blink::RequestContextType::SUBRESOURCE_WEBBUNDLE:
      return "webbundle";
    case mojom::blink::RequestContextType::TRACK:
      return "Text Track";
    case mojom::blink::RequestContextType::UNSPECIFIED:
      return "resource";
    case mojom::blink::RequestContextType::VIDEO:
      return "video";
    case mojom::blink::RequestContextType::WORKER:
      return "Worker script";
    case mojom::blink::RequestContextType::XML_HTTP_REQUEST:
      return "XMLHttpRequest endpoint";
    case mojom::blink::RequestContextType::XSLT:
      return "XSLT";
  }
  NOTREACHED_IN_MIGRATION();
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
                               const SecurityContext* security_context,
                               Settings* settings,
                               const KURL& url) {
  fetch_context.CountUsage(WebFeature::kMixedContentPresent);
  fetch_context.CountUsage(WebFeature::kMixedContentWebSocket);

  // If we're in strict mode, we'll automagically fail everything, and
  // intentionally skip the client checks in order to prevent degrading the
  // site's security UI.
  bool strict_mode =
      (security_context->GetInsecureRequestPolicy() &
       mojom::blink::InsecureRequestPolicy::kBlockAllMixedContent) !=
          mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone ||
      settings->GetStrictMixedContentChecking();
  if (strict_mode)
    return false;
  return settings && settings->GetAllowRunningOfInsecureContent();
}

bool IsWebSocketAllowedInWorker(const WorkerFetchContext& fetch_context,
                                WorkerSettings* settings,
                                const KURL& url) {
  fetch_context.CountUsage(WebFeature::kMixedContentPresent);
  fetch_context.CountUsage(WebFeature::kMixedContentWebSocket);
  if (ContentSecurityPolicy* policy =
          fetch_context.GetContentSecurityPolicy()) {
    policy->ReportMixedContent(url,
                               ResourceRequest::RedirectStatus::kNoRedirect);
  }

  // If we're in strict mode, we'll automagically fail everything, and
  // intentionally skip the client checks in order to prevent degrading the
  // site's security UI.
  bool strict_mode =
      (fetch_context.GetResourceFetcherProperties()
           .GetFetchClientSettingsObject()
           .GetInsecureRequestsPolicy() &
       mojom::blink::InsecureRequestPolicy::kBlockAllMixedContent) !=
          mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone ||
      settings->GetStrictMixedContentChecking();
  if (strict_mode)
    return false;
  return settings && settings->GetAllowRunningOfInsecureContent();
}

bool IsUrlPotentiallyTrustworthy(const KURL& url) {
  // This saves a copy of the url, which can be expensive for large data URLs.
  // TODO(crbug.com/1322100): Remove this logic once
  // network::IsUrlPotentiallyTrustworthy() doesn't copy the URL.
  if (url.ProtocolIsData()) {
    DCHECK(network::IsUrlPotentiallyTrustworthy(GURL(url)));
    return true;
  }
  return network::IsUrlPotentiallyTrustworthy(GURL(url));
}

}  // namespace

static bool IsInsecureUrl(const KURL& url) {
  // |url| is mixed content if it is not a potentially trustworthy URL.
  // See https://w3c.github.io/webappsec-mixed-content/#should-block-response
  return !IsUrlPotentiallyTrustworthy(url);
}

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
  } else if (!IsUrlPotentiallyTrustworthy(url) &&
             base::Contains(url::GetSecureSchemes(),
                            origin->Protocol().Ascii())) {
    UseCounter::Count(
        source->GetDocument(),
        WebFeature::kMixedContentInSecureFrameThatDoesNotRestrictMixedContent);
  }
}

bool RequestIsSubframeSubresource(Frame* frame) {
  return frame && frame != frame->Tree().Top();
}

// static
bool MixedContentChecker::IsMixedContent(const SecurityOrigin* security_origin,
                                         const KURL& url) {
  return IsMixedContent(
      security_origin->GetOriginOrPrecursorOriginIfOpaque()->Protocol(), url);
}

// static
bool MixedContentChecker::IsMixedContent(const String& origin_protocol,
                                         const KURL& url) {
  if (!SchemeRegistry::ShouldTreatURLSchemeAsRestrictingMixedContent(
          origin_protocol))
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
    mojom::blink::RequestContextType request_context,
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
    return MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity, message_level, message,
        std::move(source_location));
  }
  return MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity, message_level, message);
}

// static
void MixedContentChecker::Count(
    Frame* frame,
    mojom::blink::RequestContextType request_context,
    const LocalFrame* source) {
  UseCounter::Count(source->GetDocument(), WebFeature::kMixedContentPresent);

  // Roll blockable content up into a single counter, count unblocked types
  // individually so we can determine when they can be safely moved to the
  // blockable category:
  mojom::blink::MixedContentContextType context_type =
      MixedContent::ContextTypeFromRequestContext(
          request_context, DecideCheckModeForPlugin(frame->GetSettings()));
  if (context_type == mojom::blink::MixedContentContextType::kBlockable) {
    UseCounter::Count(source->GetDocument(),
                      WebFeature::kMixedContentBlockable);
    return;
  }

  WebFeature feature;
  switch (request_context) {
    case mojom::blink::RequestContextType::AUDIO:
      feature = WebFeature::kMixedContentAudio;
      break;
    case mojom::blink::RequestContextType::DOWNLOAD:
      feature = WebFeature::kMixedContentDownload;
      break;
    case mojom::blink::RequestContextType::FAVICON:
      feature = WebFeature::kMixedContentFavicon;
      break;
    case mojom::blink::RequestContextType::IMAGE:
      feature = WebFeature::kMixedContentImage;
      break;
    case mojom::blink::RequestContextType::INTERNAL:
      feature = WebFeature::kMixedContentInternal;
      break;
    case mojom::blink::RequestContextType::PLUGIN:
      feature = WebFeature::kMixedContentPlugin;
      break;
    case mojom::blink::RequestContextType::PREFETCH:
      feature = WebFeature::kMixedContentPrefetch;
      break;
    case mojom::blink::RequestContextType::VIDEO:
      feature = WebFeature::kMixedContentVideo;
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  UseCounter::Count(source->GetDocument(), feature);
}

// static
bool MixedContentChecker::ShouldBlockFetch(
    LocalFrame* frame,
    mojom::blink::RequestContextType request_context,
    network::mojom::blink::IPAddressSpace target_address_space,
    const KURL& url_before_redirects,
    ResourceRequest::RedirectStatus redirect_status,
    const KURL& url,
    const String& devtools_id,
    ReportingDisposition reporting_disposition,
    mojom::blink::ContentSecurityNotifier& notifier) {
  Frame* mixed_frame = InWhichFrameIsContentMixed(frame, url);
  if (!mixed_frame)
    return false;

  // Exempt non-webby schemes from mixed content treatment. For subresources,
  // these will be blocked anyway as net::ERR_UNKNOWN_URL_SCHEME, so there's no
  // need to present a security warning. Non-webby main resources (including
  // subframes) are handled in the browser process's mixed content checking,
  // where the URL will be allowed to load, but not treated as mixed content
  // because it can't return data to the browser. See https://crbug.com/621131.
  //
  // TODO(https://crbug.com/1030307): decide whether CORS-enabled is really the
  // right way to draw this distinction.
  if (!SchemeRegistry::ShouldTreatURLSchemeAsCorsEnabled(url.Protocol())) {
    // Record non-webby mixed content to see if it is rare enough that it can be
    // gated behind an enterprise policy. This excludes URLs that are considered
    // potentially-secure such as blob: and filesystem:, which are special-cased
    // in IsInsecureUrl() and cause an early-return because of the
    // InWhichFrameIsContentMixed() check above.
    UseCounter::Count(frame->GetDocument(), WebFeature::kNonWebbyMixedContent);
    return false;
  }

  MixedContentChecker::Count(mixed_frame, request_context, frame);
  if (ContentSecurityPolicy* policy =
          frame->DomWindow()->GetContentSecurityPolicy())
    policy->ReportMixedContent(url_before_redirects, redirect_status);

  Settings* settings = mixed_frame->GetSettings();
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
      (mixed_frame->GetSecurityContext()->GetInsecureRequestPolicy() &
       mojom::blink::InsecureRequestPolicy::kBlockAllMixedContent) !=
          mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone ||
      settings->GetStrictMixedContentChecking();

  mojom::blink::MixedContentContextType context_type =
      MixedContent::ContextTypeFromRequestContext(
          request_context, DecideCheckModeForPlugin(settings));

  switch (context_type) {
    case mojom::blink::MixedContentContextType::kOptionallyBlockable:

#if (BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)) && \
    BUILDFLAG(ENABLE_CAST_RECEIVER)
      // Fuchsia WebEngine can be configured to allow loading Mixed Content from
      // an insecure IP address. This is a workaround to revert Fuchsia Cast
      // Receivers to the behavior before crrev.com/c/4032146.
      // TODO(crbug.com/1434440): Remove this workaround when there is a better
      // way to disable blocking Mixed Content with an IP address.
      allowed = !strict_mode;
#else
      allowed = !strict_mode && !GURL(url).HostIsIPAddress();
#endif  // (BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)) &&
        // BUILDFLAG(ENABLE_CAST_RECEIVER)

      if (allowed) {
        if (content_settings_client)
          content_settings_client->PassiveInsecureContentFound(url);
        // Only notify embedder about loads that would create CSP reports (i.e.
        // filter out preloads).
        if (reporting_disposition == ReportingDisposition::kReport)
          local_frame_host.DidDisplayInsecureContent();
      }
      break;

    case mojom::blink::MixedContentContextType::kBlockable: {
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
        // Only notify embedder about loads that would create CSP reports (i.e.
        // filter out preloads).
        if (reporting_disposition == ReportingDisposition::kReport) {
          notifier.NotifyInsecureContentRan(KURL(security_origin->ToString()),
                                            url);
        }
        UseCounter::Count(frame->GetDocument(),
                          WebFeature::kMixedContentBlockableAllowed);
      }
      break;
    }

    case mojom::blink::MixedContentContextType::kShouldBeBlockable:
      allowed = !strict_mode;
      if (allowed && reporting_disposition == ReportingDisposition::kReport)
        local_frame_host.DidDisplayInsecureContent();
      break;
    case mojom::blink::MixedContentContextType::kNotMixedContent:
      NOTREACHED_IN_MIGRATION();
      break;
  };

  // Skip mixed content check for private and local targets.
  // `target_address_space` here is private/local only when resource request
  // has explicitly set `targetAddressSpace` fetch option.
  // TODO(lyf): check the IP address space for initiator, only skip when the
  // initiator is more public.
  if (base::FeatureList::IsEnabled(
          network::features::kPrivateNetworkAccessPermissionPrompt) &&
      RuntimeEnabledFeatures::PrivateNetworkAccessPermissionPromptEnabled(
          frame->DomWindow())) {
    // TODO(crbug.com/323583084): Re-enable PNA permission prompt for documents
    // fetched via service worker.
    if (!frame->Loader()
             .GetDocumentLoader()
             ->GetResponse()
             .WasFetchedViaServiceWorker() &&
        (target_address_space ==
             network::mojom::blink::IPAddressSpace::kPrivate ||
         target_address_space ==
             network::mojom::blink::IPAddressSpace::kLocal)) {
      UseCounter::Count(frame->GetDocument(),
                        WebFeature::kPrivateNetworkAccessPermissionPrompt);
      allowed = true;
    }
  }

  if (reporting_disposition == ReportingDisposition::kReport) {
    frame->GetDocument()->AddConsoleMessage(
        CreateConsoleMessageAboutFetch(MainResourceUrlForFrame(mixed_frame),
                                       url, request_context, allowed, nullptr));
  }
  // Issue is created even when reporting disposition is false i.e. for
  // speculative prefetches. Otherwise the DevTools frontend would not
  // receive an issue with a devtools_id which it can match to a request.
  AuditsIssue::ReportMixedContentIssue(
      MainResourceUrlForFrame(mixed_frame), url, request_context, frame,
      allowed ? MixedContentResolutionStatus::kMixedContentWarning
              : MixedContentResolutionStatus::kMixedContentBlocked,
      devtools_id);
  return !allowed;
}

// static
bool MixedContentChecker::ShouldBlockFetchOnWorker(
    WorkerFetchContext& worker_fetch_context,
    mojom::blink::RequestContextType request_context,
    const KURL& url_before_redirects,
    ResourceRequest::RedirectStatus redirect_status,
    const KURL& url,
    ReportingDisposition reporting_disposition,
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
    policy->ReportMixedContent(url_before_redirects, redirect_status);

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
        (fetch_client_settings_object.GetInsecureRequestsPolicy() &
         mojom::blink::InsecureRequestPolicy::kBlockAllMixedContent) !=
            mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone ||
        settings->GetStrictMixedContentChecking();
    bool should_ask_embedder =
        !strict_mode && (!settings->GetStrictlyBlockBlockableMixedContent() ||
                         settings->GetAllowRunningOfInsecureContent());
    allowed = should_ask_embedder &&
              worker_fetch_context.AllowRunningInsecureContent(
                  settings->GetAllowRunningOfInsecureContent(), url);
    if (allowed) {
      worker_fetch_context.GetContentSecurityNotifier()
          .NotifyInsecureContentRan(
              KURL(
                  fetch_client_settings_object.GetSecurityOrigin()->ToString()),
              url);
      worker_fetch_context.CountUsage(
          WebFeature::kMixedContentBlockableAllowed);
    }
  }

  if (reporting_disposition == ReportingDisposition::kReport) {
    worker_fetch_context.GetDetachableConsoleLogger().AddConsoleMessage(
        CreateConsoleMessageAboutFetch(worker_fetch_context.Url(), url,
                                       request_context, allowed, nullptr));
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
  return MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity, message_level, message);
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
  const SecurityContext* security_context = mixed_frame->GetSecurityContext();
  const SecurityOrigin* security_origin = security_context->GetSecurityOrigin();

  if (ContentSecurityPolicy* policy =
          frame->DomWindow()->GetContentSecurityPolicy()) {
    policy->ReportMixedContent(url,
                               ResourceRequest::RedirectStatus::kNoRedirect);
  }
  bool allowed = IsWebSocketAllowedInFrame(frame_fetch_context,
                                           security_context, settings, url);
  if (content_settings_client) {
    allowed =
        content_settings_client->AllowRunningInsecureContent(allowed, url);
  }

  if (allowed) {
    frame_fetch_context.GetContentSecurityNotifier().NotifyInsecureContentRan(
        KURL(security_origin->ToString()), url);
  }

  frame->GetDocument()->AddConsoleMessage(CreateConsoleMessageAboutWebSocket(
      MainResourceUrlForFrame(mixed_frame), url, allowed));
  AuditsIssue::ReportMixedContentIssue(
      MainResourceUrlForFrame(mixed_frame), url,

      mojom::blink::RequestContextType::FETCH, frame,
      allowed ? MixedContentResolutionStatus::kMixedContentWarning
              : MixedContentResolutionStatus::kMixedContentBlocked,
      String());
  return allowed;
}

// static
bool MixedContentChecker::IsWebSocketAllowed(
    WorkerFetchContext& worker_fetch_context,
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
    worker_fetch_context.GetContentSecurityNotifier().NotifyInsecureContentRan(
        KURL(security_origin->ToString()), url);
  }

  worker_fetch_context.GetDetachableConsoleLogger().AddConsoleMessage(
      CreateConsoleMessageAboutWebSocket(worker_fetch_context.Url(), url,
                                         allowed));

  return allowed;
}

bool MixedContentChecker::IsMixedFormAction(
    LocalFrame* frame,
    const KURL& url,
    ReportingDisposition reporting_disposition) {
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

  if (reporting_disposition == ReportingDisposition::kReport) {
    String message = String::Format(
        "Mixed Content: The page at '%s' was loaded over a secure connection, "
        "but contains a form that targets an insecure endpoint '%s'. This "
        "endpoint should be made available over a secure connection.",
        MainResourceUrlForFrame(mixed_frame).ElidedString().Utf8().c_str(),
        url.ElidedString().Utf8().c_str());
    frame->GetDocument()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kSecurity,
            mojom::ConsoleMessageLevel::kWarning, message));
  }
  // Issue is created even when reporting disposition is false i.e. for
  // speculative prefetches. Otherwise the DevTools frontend would not
  // receive an issue with a devtools_id which it can match to a request.
  AuditsIssue::ReportMixedContentIssue(
      MainResourceUrlForFrame(mixed_frame), url,

      mojom::blink::RequestContextType::FORM, frame,
      MixedContentResolutionStatus::kMixedContentWarning, String());

  return true;
}

bool MixedContentChecker::ShouldAutoupgrade(
    const FetchClientSettingsObject* fetch_client_settings_object,
    mojom::blink::RequestContextType type,
    WebContentSettingsClient* settings_client,
    const ResourceRequest& resource_request,
    ExecutionContext* execution_context_for_logging) {
  const HttpsState https_state = fetch_client_settings_object->GetHttpsState();
  const KURL& request_url = resource_request.Url();
  // We are currently not autoupgrading plugin loaded content, which is why
  // check_mode_for_plugin is hardcoded to kStrict.
  if (!base::FeatureList::IsEnabled(
          blink::features::kMixedContentAutoupgrade) ||
      https_state == HttpsState::kNone ||
      MixedContent::ContextTypeFromRequestContext(
          type, MixedContent::CheckModeForPlugin::kStrict) !=
          mojom::blink::MixedContentContextType::kOptionallyBlockable) {
    return false;
  }
  if (settings_client && !settings_client->ShouldAutoupgradeMixedContent()) {
    return false;
  }

  // If the content we are trying to load is an IP address, we do not
  // autoupgrade because it might not make sense to request a certificate for
  // an IP address.
  if (GURL(request_url).HostIsIPAddress()) {
    if (!request_url.ProtocolIs("https")) {
      if (auto* window =
              DynamicTo<LocalDOMWindow>(execution_context_for_logging)) {
        window->AddConsoleMessage(
            MixedContentChecker::
                CreateConsoleMessageAboutFetchIPAddressNoAutoupgrade(
                    fetch_client_settings_object->GlobalObjectUrl(),
                    request_url));
        AuditsIssue::ReportMixedContentIssue(
            fetch_client_settings_object->GlobalObjectUrl(),
            resource_request.Url(), resource_request.GetRequestContext(),
            window->document()->GetFrame(),
            MixedContentResolutionStatus::kMixedContentWarning,
            resource_request.GetDevToolsId());
      }
    }
    return false;
  }
  return true;
}

void MixedContentChecker::HandleCertificateError(
    const ResourceResponse& response,
    mojom::blink::RequestContextType request_context,
    MixedContent::CheckModeForPlugin check_mode_for_plugin,
    mojom::blink::ContentSecurityNotifier& notifier) {
  mojom::blink::MixedContentContextType context_type =
      MixedContent::ContextTypeFromRequestContext(request_context,
                                                  check_mode_for_plugin);
  if (context_type == mojom::blink::MixedContentContextType::kBlockable) {
    notifier.NotifyContentWithCertificateErrorsRan();
  } else {
    // contextTypeFromRequestContext() never returns NotMixedContent (it
    // computes the type of mixed content, given that the content is mixed).
    DCHECK_NE(context_type,
              mojom::blink::MixedContentContextType::kNotMixedContent);
    notifier.NotifyContentWithCertificateErrorsDisplayed();
  }
}

// static
void MixedContentChecker::MixedContentFound(
    LocalFrame* frame,
    const KURL& main_resource_url,
    const KURL& mixed_content_url,
    mojom::blink::RequestContextType request_context,
    bool was_allowed,
    const KURL& url_before_redirects,
    bool had_redirect,
    std::unique_ptr<SourceLocation> source_location) {
  // Logs to the frame console.
  frame->GetDocument()->AddConsoleMessage(CreateConsoleMessageAboutFetch(
      main_resource_url, mixed_content_url, request_context, was_allowed,
      std::move(source_location)));

  AuditsIssue::ReportMixedContentIssue(
      main_resource_url, mixed_content_url, request_context, frame,
      was_allowed ? MixedContentResolutionStatus::kMixedContentWarning
                  : MixedContentResolutionStatus::kMixedContentBlocked,
      String());
  // Reports to the CSP policy.
  ContentSecurityPolicy* policy =
      frame->DomWindow()->GetContentSecurityPolicy();
  if (policy) {
    policy->ReportMixedContent(
        url_before_redirects,
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
      "https://blog.chromium.org/2019/10/"
      "no-more-mixed-messages-about-https.html",
      main_resource_url.ElidedString().Utf8().c_str(),
      mixed_content_url.ElidedString().Utf8().c_str());
  return MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kWarning, message);
}

// static
ConsoleMessage*
MixedContentChecker::CreateConsoleMessageAboutFetchIPAddressNoAutoupgrade(
    const KURL& main_resource_url,
    const KURL& mixed_content_url) {
  String message = String::Format(
      "Mixed Content: The page at '%s' was loaded over HTTPS, but requested an "
      "insecure element '%s'. This request was "
      "not upgraded to HTTPS because its URL's host is an IP address.",
      main_resource_url.ElidedString().Utf8().c_str(),
      mixed_content_url.ElidedString().Utf8().c_str());
  return MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kWarning, message);
}

mojom::blink::MixedContentContextType
MixedContentChecker::ContextTypeForInspector(LocalFrame* frame,
                                             const ResourceRequest& request) {
  Frame* mixed_frame = InWhichFrameIsContentMixed(frame, request.Url());
  if (!mixed_frame)
    return mojom::blink::MixedContentContextType::kNotMixedContent;
  return MixedContent::ContextTypeFromRequestContext(
      request.GetRequestContext(),
      DecideCheckModeForPlugin(mixed_frame->GetSettings()));
}

// static
void MixedContentChecker::UpgradeInsecureRequest(
    ResourceRequest& resource_request,
    const FetchClientSettingsObject* fetch_client_settings_object,
    ExecutionContext* execution_context_for_logging,
    mojom::RequestContextFrameType frame_type,
    WebContentSettingsClient* settings_client) {
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

  if ((fetch_client_settings_object->GetInsecureRequestsPolicy() &
       mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests) ==
      mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone) {
    mojom::blink::RequestContextType context =
        resource_request.GetRequestContext();
    if (context == mojom::blink::RequestContextType::UNSPECIFIED ||
        !MixedContentChecker::ShouldAutoupgrade(
            fetch_client_settings_object, context, settings_client,
            resource_request, execution_context_for_logging)) {
      return;
    }
    // We set the upgrade if insecure flag regardless of whether we autoupgrade
    // due to scheme not being http, so any redirects get upgraded.
    resource_request.SetUpgradeIfInsecure(true);
    if (resource_request.Url().ProtocolIs("http")) {
      if (auto* window =
              DynamicTo<LocalDOMWindow>(execution_context_for_logging)) {
        window->AddConsoleMessage(
            MixedContentChecker::CreateConsoleMessageAboutFetchAutoupgrade(
                fetch_client_settings_object->GlobalObjectUrl(),
                resource_request.Url()));
        resource_request.SetUkmSourceId(window->document()->UkmSourceID());
        AuditsIssue::ReportMixedContentIssue(
            fetch_client_settings_object->GlobalObjectUrl(),
            resource_request.Url(), context, window->document()->GetFrame(),
            MixedContentResolutionStatus::kMixedContentAutomaticallyUpgraded,
            resource_request.GetDevToolsId());
      }
      resource_request.SetIsAutomaticUpgrade(true);
    } else {
      return;
    }
  }

  // Nested frames are always upgraded on the browser process.
  if (frame_type == mojom::RequestContextFrameType::kNested)
    return;

  // We set the UpgradeIfInsecure flag even if the current request wasn't
  // upgraded (due to already being HTTPS), since we still need to upgrade
  // redirects if they are not to HTTPS URLs.
  resource_request.SetUpgradeIfInsecure(true);

  KURL url = resource_request.Url();

  if (!url.ProtocolIs("http") || IsUrlPotentiallyTrustworthy(url))
    return;

  if (frame_type == mojom::RequestContextFrameType::kNone ||
      resource_request.GetRequestContext() ==
          mojom::blink::RequestContextType::FORM ||
      (!url.Host().IsNull() &&
       fetch_client_settings_object->GetUpgradeInsecureNavigationsSet()
           .Contains(url.Host().ToString().Impl()->GetHash()))) {
    if (!resource_request.IsAutomaticUpgrade()) {
      // These UseCounters are specific for UpgradeInsecureRequests, don't log
      // for autoupgrades.
      mojom::blink::RequestContextType context =
          resource_request.GetRequestContext();
      if (context == mojom::blink::RequestContextType::UNSPECIFIED) {
        UseCounter::Count(
            execution_context_for_logging,
            WebFeature::kUpgradeInsecureRequestsUpgradedRequestUnknown);
      } else {
        mojom::blink::MixedContentContextType content_type =
            MixedContent::ContextTypeFromRequestContext(
                context, MixedContent::CheckModeForPlugin::kLax);
        switch (content_type) {
          case mojom::blink::MixedContentContextType::kOptionallyBlockable:
            UseCounter::Count(
                execution_context_for_logging,
                WebFeature::
                    kUpgradeInsecureRequestsUpgradedRequestOptionallyBlockable);
            break;
          case mojom::blink::MixedContentContextType::kBlockable:
          case mojom::blink::MixedContentContextType::kShouldBeBlockable:
            UseCounter::Count(
                execution_context_for_logging,
                WebFeature::kUpgradeInsecureRequestsUpgradedRequestBlockable);
            break;
          case mojom::blink::MixedContentContextType::kNotMixedContent:
            NOTREACHED_IN_MIGRATION();
        }
      }
    }
    url.SetProtocol("https");
    if (url.Port() == 80)
      url.SetPort(443);
    resource_request.SetUrl(url);
  }
}

// static
MixedContent::CheckModeForPlugin MixedContentChecker::DecideCheckModeForPlugin(
    Settings* settings) {
  if (settings && settings->GetStrictMixedContentCheckingForPlugin())
    return MixedContent::CheckModeForPlugin::kStrict;
  return MixedContent::CheckModeForPlugin::kLax;
}

}  // namespace blink
