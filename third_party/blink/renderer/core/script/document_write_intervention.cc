// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/document_write_intervention.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"

namespace blink {

namespace {

void EmitWarningMayBeBlocked(const String& url, Document& document) {
  String message =
      "A parser-blocking, cross site (i.e. different eTLD+1) script, " + url +
      ", is invoked via document.write. The network request for this script "
      "MAY be blocked by the browser in this or a future page load due to poor "
      "network connectivity. If blocked in this page load, it will be "
      "confirmed in a subsequent console message. "
      "See https://www.chromestatus.com/feature/5718547946799104 "
      "for more details.";
  document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kWarning, message));
  DVLOG(1) << message.Utf8();
}

void EmitWarningNotBlocked(const String& url, Document& document) {
  String message =
      "The parser-blocking, cross site (i.e. different eTLD+1) script, " + url +
      ", invoked via document.write was NOT BLOCKED on this page load, but MAY "
      "be blocked by the browser in future page loads with poor network "
      "connectivity.";
  document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kWarning, message));
}

void EmitErrorBlocked(const String& url, Document& document) {
  String message =
      "Network request for the parser-blocking, cross site (i.e. different "
      "eTLD+1) script, " +
      url +
      ", invoked via document.write was BLOCKED by the browser due to poor "
      "network connectivity. ";
  document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kIntervention,
      mojom::ConsoleMessageLevel::kError, message));
}

void AddWarningHeader(FetchParameters* params) {
  params->MutableResourceRequest().AddHttpHeaderField(
      AtomicString("Intervention"),
      AtomicString("<https://www.chromestatus.com/feature/5718547946799104>; "
                   "level=\"warning\""));
}

void AddHeader(FetchParameters* params) {
  params->MutableResourceRequest().AddHttpHeaderField(
      AtomicString("Intervention"),
      AtomicString("<https://www.chromestatus.com/feature/5718547946799104>"));
}

bool IsConnectionEffectively2G(WebEffectiveConnectionType effective_type) {
  switch (effective_type) {
    case WebEffectiveConnectionType::kTypeSlow2G:
    case WebEffectiveConnectionType::kType2G:
      return true;
    case WebEffectiveConnectionType::kType3G:
    case WebEffectiveConnectionType::kType4G:
    case WebEffectiveConnectionType::kTypeUnknown:
    case WebEffectiveConnectionType::kTypeOffline:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool ShouldDisallowFetch(Settings* settings,
                         WebConnectionType connection_type,
                         WebEffectiveConnectionType effective_connection) {
  if (settings->GetDisallowFetchForDocWrittenScriptsInMainFrame())
    return true;
  if (settings
          ->GetDisallowFetchForDocWrittenScriptsInMainFrameOnSlowConnections() &&
      connection_type == kWebConnectionTypeCellular2G)
    return true;
  if (settings
          ->GetDisallowFetchForDocWrittenScriptsInMainFrameIfEffectively2G() &&
      IsConnectionEffectively2G(effective_connection))
    return true;
  return false;
}

}  // namespace

bool MaybeDisallowFetchForDocWrittenScript(FetchParameters& params,
                                           Document& document) {
  // Only scripts inserted via document.write are candidates for having their
  // fetch disallowed.
  if (!document.IsInDocumentWrite())
    return false;

  Settings* settings = document.GetSettings();
  if (!settings)
    return false;

  if (!document.IsInOutermostMainFrame())
    return false;

  // Only block synchronously loaded (parser blocking) scripts.
  if (params.Defer() != FetchParameters::kNoDefer)
    return false;

  probe::DocumentWriteFetchScript(&document);

  if (!params.Url().ProtocolIsInHTTPFamily())
    return false;

  // Avoid blocking same origin scripts, as they may be used to render main
  // page content, whereas cross-origin scripts inserted via document.write
  // are likely to be third party content.
  StringView request_host = params.Url().Host();
  String document_host = document.domWindow()->GetSecurityOrigin()->Domain();
  if (request_host == document_host) {
    return false;
  }

  // If the hosts didn't match, then see if the domains match. For example, if
  // a script is served from static.example.com for a document served from
  // www.example.com, we consider that a first party script and allow it.
  String request_domain = network_utils::GetDomainAndRegistry(
      request_host, network_utils::kIncludePrivateRegistries);
  String document_domain = network_utils::GetDomainAndRegistry(
      document_host, network_utils::kIncludePrivateRegistries);
  // getDomainAndRegistry will return the empty string for domains that are
  // already top-level, such as localhost. Thus we only compare domains if we
  // get non-empty results back from getDomainAndRegistry.
  if (!request_domain.empty() && !document_domain.empty() &&
      request_domain == document_domain) {
    return false;
  }

  EmitWarningMayBeBlocked(params.Url().GetString(), document);

  // Do not block scripts if it is a page reload. This is to enable pages to
  // recover if blocking of a script is leading to a page break and the user
  // reloads the page.
  const WebFrameLoadType load_type = document.Loader()->LoadType();
  if (IsReloadLoadType(load_type)) {
    AddWarningHeader(&params);
    return false;
  }

  // Add the metadata that this page has scripts inserted via document.write
  // that are eligible for blocking. Note that if there are multiple scripts
  // the flag will be conveyed to the browser process only once.
  document.Loader()->DidObserveLoadingBehavior(
      LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlock);

  if (!ShouldDisallowFetch(settings, GetNetworkStateNotifier().ConnectionType(),
                           GetNetworkStateNotifier().EffectiveType())) {
    AddWarningHeader(&params);
    return false;
  }

  AddWarningHeader(&params);

  params.MutableResourceRequest().SetCacheMode(
      mojom::FetchCacheMode::kOnlyIfCached);

  return true;
}

void PossiblyFetchBlockedDocWriteScript(
    const Resource* resource,
    Document& element_document,
    const ScriptFetchOptions& options,
    CrossOriginAttributeValue cross_origin) {
  if (!resource->ErrorOccurred()) {
    EmitWarningNotBlocked(resource->Url(), element_document);
    return;
  }

  // Due to dependency violation, not able to check the exact error to be
  // ERR_CACHE_MISS but other errors are rare with
  // mojom::FetchCacheMode::kOnlyIfCached.

  EmitErrorBlocked(resource->Url(), element_document);

  ExecutionContext* context = element_document.GetExecutionContext();
  FetchParameters params(options.CreateFetchParameters(
      resource->Url(), context->GetSecurityOrigin(), context->GetCurrentWorld(),
      cross_origin, resource->Encoding(), FetchParameters::kIdleLoad));
  params.SetRenderBlockingBehavior(RenderBlockingBehavior::kNonBlocking);
  AddHeader(&params);

  // If streaming is not allowed, no compile hints are needed either.
  constexpr v8_compile_hints::V8CrowdsourcedCompileHintsProducer*
      kNoCompileHintsProducer = nullptr;
  constexpr v8_compile_hints::V8CrowdsourcedCompileHintsConsumer*
      kNoCompileHintsConsumer = nullptr;
  constexpr bool kNoV8CompileHintsMagicCommentRuntimeEnabledFeature = false;
  ScriptResource::Fetch(params, element_document.Fetcher(), nullptr,
                        context->GetIsolate(), ScriptResource::kNoStreaming,
                        kNoCompileHintsProducer, kNoCompileHintsConsumer,
                        kNoV8CompileHintsMagicCommentRuntimeEnabledFeature);
}

}  // namespace blink
