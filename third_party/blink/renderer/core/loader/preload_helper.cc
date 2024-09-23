// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/preload_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/parser/sizes_attribute_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/blocking_attribute.h"
#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_srcset_parser.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/alternate_signed_exchange_resource_info.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/fetch_priority_attribute.h"
#include "third_party/blink/renderer/core/loader/link_load_parameters.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/pending_link_preload.h"
#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/loader/resource/link_dictionary_resource.h"
#include "third_party/blink/renderer/core/loader/resource/link_prefetch_resource.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/link_header.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"

namespace blink {

namespace {

class LoadDictionaryWhenIdleTask final : public IdleTask {
 public:
  LoadDictionaryWhenIdleTask(FetchParameters fetch_params,
                             ResourceFetcher* fetcher,
                             PendingLinkPreload* pending_preload)
      : fetch_params_(std::move(fetch_params)),
        resource_fetcher_(fetcher),
        pending_preload_(pending_preload) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resource_fetcher_);
    visitor->Trace(pending_preload_);
    visitor->Trace(fetch_params_);
    IdleTask::Trace(visitor);
  }

 private:
  void invoke(IdleDeadline* deadline) override {
    Resource* resource =
        LinkDictionaryResource::Fetch(fetch_params_, resource_fetcher_);
    if (pending_preload_) {
      pending_preload_->AddResource(resource);
    }
  }

  FetchParameters fetch_params_;
  Member<ResourceFetcher> resource_fetcher_;
  Member<PendingLinkPreload> pending_preload_;
};

void SendMessageToConsoleForPossiblyNullDocument(
    ConsoleMessage* console_message,
    Document* document,
    LocalFrame* frame) {
  DCHECK(document || frame);
  DCHECK(!document || document->GetFrame() == frame);
  // Route the console message through Document if possible, so that script line
  // numbers can be included. Otherwise, route directly to the FrameConsole, to
  // ensure we never drop a message.
  if (document)
    document->AddConsoleMessage(console_message);
  else
    frame->Console().AddMessage(console_message);
}

bool IsSupportedType(ResourceType resource_type, const String& mime_type) {
  if (mime_type.empty())
    return true;
  switch (resource_type) {
    case ResourceType::kImage:
      return MIMETypeRegistry::IsSupportedImagePrefixedMIMEType(mime_type);
    case ResourceType::kScript:
      return MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type);
    case ResourceType::kCSSStyleSheet:
      return MIMETypeRegistry::IsSupportedStyleSheetMIMEType(mime_type);
    case ResourceType::kFont:
      return MIMETypeRegistry::IsSupportedFontMIMEType(mime_type);
    case ResourceType::kAudio:
    case ResourceType::kVideo:
      return MIMETypeRegistry::IsSupportedMediaMIMEType(mime_type, String());
    case ResourceType::kTextTrack:
      return MIMETypeRegistry::IsSupportedTextTrackMIMEType(mime_type);
    case ResourceType::kRaw:
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return false;
}

MediaValuesCached* CreateMediaValues(
    Document& document,
    const ViewportDescription* viewport_description) {
  MediaValuesCached* media_values =
      MakeGarbageCollected<MediaValuesCached>(document);
  if (viewport_description) {
    gfx::SizeF initial_viewport(media_values->DeviceWidth(),
                                media_values->DeviceHeight());
    PageScaleConstraints constraints = viewport_description->Resolve(
        initial_viewport, document.GetViewportData().ViewportDefaultMinWidth());
    media_values->OverrideViewportDimensions(constraints.layout_size.width(),
                                             constraints.layout_size.height());
  }
  return media_values;
}

bool MediaMatches(const String& media,
                  MediaValues* media_values,
                  const ExecutionContext* execution_context) {
  MediaQuerySet* media_queries =
      MediaQuerySet::Create(media, execution_context);
  MediaQueryEvaluator* evaluator =
      MakeGarbageCollected<MediaQueryEvaluator>(media_values);
  return evaluator->Eval(*media_queries);
}

KURL GetBestFitImageURL(const Document& document,
                        const KURL& base_url,
                        MediaValues* media_values,
                        const KURL& href,
                        const String& image_srcset,
                        const String& image_sizes) {
  float source_size = SizesAttributeParser(media_values, image_sizes,
                                           document.GetExecutionContext())
                          .Size();
  ImageCandidate candidate = BestFitSourceForImageAttributes(
      media_values->DevicePixelRatio(), source_size, href, image_srcset);
  return base_url.IsNull() ? document.CompleteURL(candidate.ToString())
                           : KURL(base_url, candidate.ToString());
}

// Check whether the `as` attribute is valid according to the spec, even if we
// don't currently support it yet.
bool IsValidButUnsupportedAsAttribute(const String& as) {
  DCHECK(as != "fetch" && as != "image" && as != "font" && as != "script" &&
         as != "style" && as != "track");
  return as == "audio" || as == "audioworklet" || as == "document" ||
         as == "embed" || as == "manifest" || as == "object" ||
         as == "paintworklet" || as == "report" || as == "sharedworker" ||
         as == "video" || as == "worker" || as == "xslt";
}

bool IsNetworkHintAllowed(PreloadHelper::LoadLinksFromHeaderMode mode) {
  switch (mode) {
    case PreloadHelper::LoadLinksFromHeaderMode::kDocumentBeforeCommit:
      return true;
    case PreloadHelper::LoadLinksFromHeaderMode::
        kDocumentAfterCommitWithoutViewport:
      return false;
    case PreloadHelper::LoadLinksFromHeaderMode::
        kDocumentAfterCommitWithViewport:
      return false;
    case PreloadHelper::LoadLinksFromHeaderMode::kDocumentAfterLoadCompleted:
      return false;
    case PreloadHelper::LoadLinksFromHeaderMode::kSubresourceFromMemoryCache:
      return true;
    case PreloadHelper::LoadLinksFromHeaderMode::kSubresourceNotFromMemoryCache:
      return true;
  }
}

bool IsResourceLoadAllowed(PreloadHelper::LoadLinksFromHeaderMode mode,
                           bool is_viewport_dependent) {
  switch (mode) {
    case PreloadHelper::LoadLinksFromHeaderMode::kDocumentBeforeCommit:
      return false;
    case PreloadHelper::LoadLinksFromHeaderMode::
        kDocumentAfterCommitWithoutViewport:
      return !is_viewport_dependent;
    case PreloadHelper::LoadLinksFromHeaderMode::
        kDocumentAfterCommitWithViewport:
      return is_viewport_dependent;
    case PreloadHelper::LoadLinksFromHeaderMode::kDocumentAfterLoadCompleted:
      return false;
    case PreloadHelper::LoadLinksFromHeaderMode::kSubresourceFromMemoryCache:
      return false;
    case PreloadHelper::LoadLinksFromHeaderMode::kSubresourceNotFromMemoryCache:
      return true;
  }
}

bool IsCompressionDictionaryLoadAllowed(
    PreloadHelper::LoadLinksFromHeaderMode mode) {
  // Document header can trigger dictionary load after the page load completes.
  // Subresources header can trigger dictionary load if it is not from the
  // memory cache.
  switch (mode) {
    case PreloadHelper::LoadLinksFromHeaderMode::kDocumentBeforeCommit:
      return false;
    case PreloadHelper::LoadLinksFromHeaderMode::
        kDocumentAfterCommitWithoutViewport:
      return false;
    case PreloadHelper::LoadLinksFromHeaderMode::
        kDocumentAfterCommitWithViewport:
      return false;
    case PreloadHelper::LoadLinksFromHeaderMode::kDocumentAfterLoadCompleted:
      return true;
    case PreloadHelper::LoadLinksFromHeaderMode::kSubresourceFromMemoryCache:
      return false;
    case PreloadHelper::LoadLinksFromHeaderMode::kSubresourceNotFromMemoryCache:
      return true;
  }
}

}  // namespace

void PreloadHelper::DnsPrefetchIfNeeded(
    const LinkLoadParameters& params,
    Document* document,
    LocalFrame* frame,
    LinkCaller caller) {
  if (document && document->Loader() && document->Loader()->Archive()) {
    return;
  }
  if (params.rel.IsDNSPrefetch()) {
    UseCounter::Count(document, WebFeature::kLinkRelDnsPrefetch);
    if (caller == kLinkCalledFromHeader)
      UseCounter::Count(document, WebFeature::kLinkHeaderDnsPrefetch);
    Settings* settings = frame ? frame->GetSettings() : nullptr;
    // FIXME: The href attribute of the link element can be in "//hostname"
    // form, and we shouldn't attempt to complete that as URL
    // <https://bugs.webkit.org/show_bug.cgi?id=48857>.
    if (settings && settings->GetDNSPrefetchingEnabled() &&
        params.href.IsValid() && !params.href.IsEmpty()) {
      if (settings->GetLogDnsPrefetchAndPreconnect()) {
        SendMessageToConsoleForPossiblyNullDocument(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kVerbose,
                String("DNS prefetch triggered for " + params.href.Host())),
            document, frame);
      }
      WebPrescientNetworking* web_prescient_networking =
          frame ? frame->PrescientNetworking() : nullptr;
      if (web_prescient_networking) {
        web_prescient_networking->PrefetchDNS(params.href);
      }
    }
  }
}

void PreloadHelper::PreconnectIfNeeded(
    const LinkLoadParameters& params,
    Document* document,
    LocalFrame* frame,
    LinkCaller caller) {
  if (document && document->Loader() && document->Loader()->Archive()) {
    return;
  }
  if (params.rel.IsPreconnect() && params.href.IsValid() &&
      params.href.ProtocolIsInHTTPFamily()) {
    UseCounter::Count(document, WebFeature::kLinkRelPreconnect);
    if (caller == kLinkCalledFromHeader)
      UseCounter::Count(document, WebFeature::kLinkHeaderPreconnect);
    Settings* settings = frame ? frame->GetSettings() : nullptr;
    if (settings && settings->GetLogDnsPrefetchAndPreconnect()) {
      SendMessageToConsoleForPossiblyNullDocument(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kVerbose,
              String("Preconnect triggered for ") + params.href.GetString()),
          document, frame);
      if (params.cross_origin != kCrossOriginAttributeNotSet) {
        SendMessageToConsoleForPossiblyNullDocument(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kVerbose,
                String("Preconnect CORS setting is ") +
                    String(
                        (params.cross_origin == kCrossOriginAttributeAnonymous)
                            ? "anonymous"
                            : "use-credentials")),
            document, frame);
      }
    }
    WebPrescientNetworking* web_prescient_networking =
        frame ? frame->PrescientNetworking() : nullptr;
    if (web_prescient_networking) {
      web_prescient_networking->Preconnect(
          params.href, params.cross_origin != kCrossOriginAttributeAnonymous);
    }
  }
}

// Until the preload cache is defined in terms of range requests and media
// fetches we can't reliably preload audio/video content and expect it to be
// served from the cache correctly. Until
// https://github.com/w3c/preload/issues/97 is resolved and implemented we need
// to disable these preloads.
std::optional<ResourceType> PreloadHelper::GetResourceTypeFromAsAttribute(
    const String& as) {
  DCHECK_EQ(as.DeprecatedLower(), as);
  if (as == "image")
    return ResourceType::kImage;
  if (as == "script")
    return ResourceType::kScript;
  if (as == "style")
    return ResourceType::kCSSStyleSheet;
  if (as == "track")
    return ResourceType::kTextTrack;
  if (as == "font")
    return ResourceType::kFont;
  if (as == "fetch")
    return ResourceType::kRaw;
  return std::nullopt;
}

// |base_url| is used in Link HTTP Header based preloads to resolve relative
// URLs in srcset, which should be based on the resource's URL, not the
// document's base URL. If |base_url| is a null URL, relative URLs are resolved
// using |document.CompleteURL()|.
void PreloadHelper::PreloadIfNeeded(
    const LinkLoadParameters& params,
    Document& document,
    const KURL& base_url,
    LinkCaller caller,
    const ViewportDescription* viewport_description,
    ParserDisposition parser_disposition,
    PendingLinkPreload* pending_preload) {
  if (!document.Loader() || !params.rel.IsLinkPreload())
    return;

  std::optional<ResourceType> resource_type =
      PreloadHelper::GetResourceTypeFromAsAttribute(params.as);

  MediaValuesCached* media_values = nullptr;
  KURL url;
  if (resource_type == ResourceType::kImage && !params.image_srcset.empty()) {
    UseCounter::Count(document, WebFeature::kLinkRelPreloadImageSrcset);
    media_values = CreateMediaValues(document, viewport_description);
    url = GetBestFitImageURL(document, base_url, media_values, params.href,
                             params.image_srcset, params.image_sizes);
  } else {
    url = params.href;
  }

  UseCounter::Count(document, WebFeature::kLinkRelPreload);
  if (!url.IsValid() || url.IsEmpty()) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        String("<link rel=preload> has an invalid `href` value")));
    return;
  }

  bool media_matches = true;

  if (!params.media.empty()) {
    if (!media_values)
      media_values = CreateMediaValues(document, viewport_description);
    media_matches = MediaMatches(params.media, media_values,
                                 document.GetExecutionContext());
  }

  DCHECK(pending_preload);

  if (params.reason == LinkLoadParameters::Reason::kMediaChange) {
    if (!media_matches) {
      // Media attribute does not match environment, abort existing preload.
      pending_preload->Dispose();
    } else if (pending_preload->MatchesMedia()) {
      // Media still matches, no need to re-fetch.
      return;
    }
  }

  pending_preload->SetMatchesMedia(media_matches);

  // Preload only if media matches
  if (!media_matches)
    return;

  if (caller == kLinkCalledFromHeader)
    UseCounter::Count(document, WebFeature::kLinkHeaderPreload);
  if (resource_type == std::nullopt) {
    String message;
    if (IsValidButUnsupportedAsAttribute(params.as)) {
      message = String("<link rel=preload> uses an unsupported `as` value");
    } else {
      message = String("<link rel=preload> must have a valid `as` value");
    }
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning, message));
    return;
  }
  if (!IsSupportedType(resource_type.value(), params.type)) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        String("<link rel=preload> has an unsupported `type` value")));
    return;
  }
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(ResourceFetcher::DetermineRequestContext(
      resource_type.value(), ResourceFetcher::kImageNotImageSet));
  resource_request.SetRequestDestination(
      ResourceFetcher::DetermineRequestDestination(resource_type.value()));

  resource_request.SetReferrerPolicy(params.referrer_policy);

  resource_request.SetFetchPriorityHint(
      GetFetchPriorityAttributeValue(params.fetch_priority_hint));

  ResourceLoaderOptions options(
      document.GetExecutionContext()->GetCurrentWorld());

  options.initiator_info.name = fetch_initiator_type_names::kLink;
  options.parser_disposition = parser_disposition;
  FetchParameters link_fetch_params(std::move(resource_request), options);
  link_fetch_params.SetCharset(document.Encoding());

  if (params.cross_origin != kCrossOriginAttributeNotSet) {
    link_fetch_params.SetCrossOriginAccessControl(
        document.GetExecutionContext()->GetSecurityOrigin(),
        params.cross_origin);
  }

  const String& integrity_attr = params.integrity;
  // A corresponding check for the preload-scanner code path is in
  // TokenPreloadScanner::StartTagScanner::CreatePreloadRequest().
  // TODO(crbug.com/981419): Honor the integrity attribute value for all
  // supported preload destinations, not just the destinations that support SRI
  // in the first place.
  if (resource_type == ResourceType::kScript ||
      resource_type == ResourceType::kCSSStyleSheet ||
      resource_type == ResourceType::kFont) {
    if (!integrity_attr.empty()) {
      IntegrityMetadataSet metadata_set;
      SubresourceIntegrity::ParseIntegrityAttribute(
          integrity_attr,
          SubresourceIntegrityHelper::GetFeatures(
              document.GetExecutionContext()),
          metadata_set);
      link_fetch_params.SetIntegrityMetadata(metadata_set);
      link_fetch_params.MutableResourceRequest().SetFetchIntegrity(
          integrity_attr);
    }
  } else {
    if (!integrity_attr.empty()) {
      document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kWarning,
          String("The `integrity` attribute is currently ignored for preload "
                 "destinations that do not support subresource integrity. See "
                 "https://crbug.com/981419 for more information")));
    }
  }

  link_fetch_params.SetContentSecurityPolicyNonce(params.nonce);
  Settings* settings = document.GetSettings();
  if (settings && settings->GetLogPreload()) {
    String message = "Preload triggered for " + url.Host() + url.GetPath();
    String fetch_priority_message;
    if (!params.fetch_priority_hint.empty()) {
      mojom::blink::FetchPriorityHint hint =
          GetFetchPriorityAttributeValue(params.fetch_priority_hint);
      switch (hint) {
        case mojom::blink::FetchPriorityHint::kLow:
          fetch_priority_message = " with fetchpriority hint 'low'";
          break;
        case mojom::blink::FetchPriorityHint::kHigh:
          fetch_priority_message = " with fetchpriority hint 'high'";
          break;
        case mojom::blink::FetchPriorityHint::kAuto:
          fetch_priority_message = " with fetchpriority hint 'auto'";
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    }
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kVerbose,
        message + fetch_priority_message));
  }
  link_fetch_params.SetLinkPreload(true);
  link_fetch_params.SetRenderBlockingBehavior(
      RenderBlockingBehavior::kNonBlocking);
  if (pending_preload) {
    if (RenderBlockingResourceManager* manager =
            document.GetRenderBlockingResourceManager()) {
      if (EqualIgnoringASCIICase(params.as, "font")) {
        manager->AddPendingFontPreload(*pending_preload);
      }
    }
  }

  Resource* resource = PreloadHelper::StartPreload(resource_type.value(),
                                                   link_fetch_params, document);
  if (pending_preload)
    pending_preload->AddResource(resource);
}

// https://html.spec.whatwg.org/C/#link-type-modulepreload
void PreloadHelper::ModulePreloadIfNeeded(
    const LinkLoadParameters& params,
    Document& document,
    const ViewportDescription* viewport_description,
    PendingLinkPreload* client) {
  if (!document.Loader() || !params.rel.IsModulePreload())
    return;

  UseCounter::Count(document, WebFeature::kLinkRelModulePreload);

  // Step 1. "If the href attribute's value is the empty string, then return."
  // [spec text]
  if (params.href.IsEmpty()) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "<link rel=modulepreload> has no `href` value"));
    return;
  }

  // Step 5. "Let settings object be the link element's node document's relevant
  // settings object." [spec text]
  // |document| is the node document here, and its context document is the
  // relevant settings object.
  LocalDOMWindow* window = To<LocalDOMWindow>(document.GetExecutionContext());
  Modulator* modulator =
      Modulator::From(ToScriptStateForMainWorld(window->GetFrame()));
  DCHECK(modulator);
  if (!modulator)
    return;

  // Step 2. "Let destination be the current state of the as attribute (a
  // destination), or "script" if it is in no state." [spec text]
  // Step 3. "If destination is not script-like, then queue a task on the
  // networking task source to fire an event named error at the link element,
  // and return." [spec text]
  // Currently we only support as="script".
  if (!params.as.empty() && params.as != "script") {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        String("<link rel=modulepreload> has an invalid `as` value " +
               params.as)));
    // This triggers the same logic as Step 11 asynchronously, which will fire
    // the error event.
    if (client) {
      modulator->TaskRunner()->PostTask(
          FROM_HERE,
          WTF::BindOnce(&SingleModuleClient::NotifyModuleLoadFinished,
                        WrapPersistent(client), nullptr));
    }
    return;
  }
  mojom::blink::RequestContextType context_type =
      mojom::blink::RequestContextType::SCRIPT;
  network::mojom::RequestDestination destination =
      network::mojom::RequestDestination::kScript;

  // Step 4. "Parse the URL given by the href attribute, relative to the
  // element's node document. If that fails, then return. Otherwise, let url be
  // the resulting URL record." [spec text]
  // |href| is already resolved in caller side.
  if (!params.href.IsValid()) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "<link rel=modulepreload> has an invalid `href` value " +
            params.href.GetString()));
    return;
  }

  // Preload only if media matches.
  // https://html.spec.whatwg.org/C/#processing-the-media-attribute
  if (!params.media.empty()) {
    MediaValuesCached* media_values =
        CreateMediaValues(document, viewport_description);
    if (!MediaMatches(params.media, media_values,
                      document.GetExecutionContext()))
      return;
  }

  // Step 6. "Let credentials mode be the module script credentials mode for the
  // crossorigin attribute." [spec text]
  network::mojom::CredentialsMode credentials_mode =
      ScriptLoader::ModuleScriptCredentialsMode(params.cross_origin);

  // Step 7. "Let cryptographic nonce be the value of the nonce attribute, if it
  // is specified, or the empty string otherwise." [spec text]
  // |nonce| parameter is the value of the nonce attribute.

  // Step 9. "Let integrity metadata be the value of the integrity attribute, if
  // it is specified, or the empty string otherwise." [spec text]
  IntegrityMetadataSet integrity_metadata;
  String integrity_value = params.integrity;
  if (!integrity_value.empty()) {
    SubresourceIntegrity::IntegrityFeatures integrity_features =
        SubresourceIntegrityHelper::GetFeatures(document.GetExecutionContext());
    SubresourceIntegrity::ReportInfo report_info;
    SubresourceIntegrity::ParseIntegrityAttribute(
        params.integrity, integrity_features, integrity_metadata, &report_info);
    SubresourceIntegrityHelper::DoReport(*document.GetExecutionContext(),
                                         report_info);
  } else if (integrity_value.IsNull()) {
    // Step 10. "If el does not have an integrity attribute, then set integrity
    // metadata to the result of resolving a module integrity metadata with url
    // and settings object." [spec text]
    integrity_value = modulator->GetIntegrityMetadataString(params.href);
    integrity_metadata = modulator->GetIntegrityMetadata(params.href);
  }

  // Step 11. "Let referrer policy be the current state of the element's
  // referrerpolicy attribute." [spec text]
  // |referrer_policy| parameter is the value of the referrerpolicy attribute.

  // Step 12. "Let options be a script fetch options whose cryptographic nonce
  // is cryptographic nonce, integrity metadata is integrity metadata, parser
  // metadata is "not-parser-inserted", credentials mode is credentials mode,
  // and referrer policy is referrer policy." [spec text]
  ModuleScriptFetchRequest request(
      params.href, ModuleType::kJavaScript, context_type, destination,
      ScriptFetchOptions(params.nonce, integrity_metadata, integrity_value,
                         kNotParserInserted, credentials_mode,
                         params.referrer_policy,
                         mojom::blink::FetchPriorityHint::kAuto,
                         RenderBlockingBehavior::kNonBlocking),
      Referrer::NoReferrer(), TextPosition::MinimumPosition());

  // Step 13. "Fetch a modulepreload module script graph given url, destination,
  // settings object, and options. Wait until the algorithm asynchronously
  // completes with result." [spec text]
  //
  // https://wicg.github.io/import-maps/#wait-for-import-maps
  modulator->SetAcquiringImportMapsState(
      Modulator::AcquiringImportMapsState::kAfterModuleScriptLoad);
  modulator->FetchSingle(request, window->Fetcher(),
                         ModuleGraphLevel::kDependentModuleFetch,
                         ModuleScriptCustomFetchType::kNone, client);

  Settings* settings = document.GetSettings();
  if (settings && settings->GetLogPreload()) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kVerbose,
        "Module preload triggered for " + params.href.Host() +
            params.href.GetPath()));
  }

  // Asynchronously continue processing after
  // client->NotifyModuleLoadFinished() is called.
}

void PreloadHelper::PrefetchIfNeeded(const LinkLoadParameters& params,
                                     Document& document,
                                     PendingLinkPreload* pending_preload) {
  if (document.Loader() && document.Loader()->Archive())
    return;

  if (!params.rel.IsLinkPrefetch() || !params.href.IsValid() ||
      !document.GetFrame())
    return;
  UseCounter::Count(document, WebFeature::kLinkRelPrefetch);

  ResourceRequest resource_request(params.href);

  bool as_document = EqualIgnoringASCIICase(params.as, "document");

  // If this corresponds to a preload that we promoted to a prefetch, and the
  // preload had `as="document"`, don't proceed because the original preload
  // statement was invalid.
  if (as_document && params.recursive_prefetch_token) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        String("Link header with rel=preload and as=document is unsupported")));
    return;
  }

  // Later a security check is done asserting that the initiator of a
  // cross-origin prefetch request is same-origin with the origin that the
  // browser process is aware of. However, since opaque request initiators are
  // always cross-origin with every other origin, we must not request
  // cross-origin prefetches from opaque requestors.
  if (as_document &&
      !document.GetExecutionContext()->GetSecurityOrigin()->IsOpaque()) {
    resource_request.SetPrefetchMaybeForTopLevelNavigation(true);

    bool is_same_origin =
        document.GetExecutionContext()->GetSecurityOrigin()->IsSameOriginWith(
            SecurityOrigin::Create(params.href).get());
    UseCounter::Count(document,
                      is_same_origin
                          ? WebFeature::kLinkRelPrefetchAsDocumentSameOrigin
                          : WebFeature::kLinkRelPrefetchAsDocumentCrossOrigin);
  }

  // This request could have originally been a preload header on a prefetch
  // response, that was promoted to a prefetch request by LoadLinksFromHeader.
  // In that case, it may have a recursive prefetch token used by the browser
  // process to ensure this request is cached correctly. Propagate it.
  resource_request.SetRecursivePrefetchToken(params.recursive_prefetch_token);

  resource_request.SetReferrerPolicy(params.referrer_policy);
  resource_request.SetFetchPriorityHint(
      GetFetchPriorityAttributeValue(params.fetch_priority_hint));

  if (base::FeatureList::IsEnabled(features::kPrefetchPrivacyChanges)) {
    resource_request.SetRedirectMode(network::mojom::RedirectMode::kError);
    resource_request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
    // TODO(domfarolino): Implement more privacy-preserving prefetch changes.
    // See crbug.com/988956.
  }

  ResourceLoaderOptions options(
      document.GetExecutionContext()->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kLink;

  FetchParameters link_fetch_params(std::move(resource_request), options);
  if (params.cross_origin != kCrossOriginAttributeNotSet) {
    link_fetch_params.SetCrossOriginAccessControl(
        document.GetExecutionContext()->GetSecurityOrigin(),
        params.cross_origin);
  }
  Resource* resource =
      LinkPrefetchResource::Fetch(link_fetch_params, document.Fetcher());
  if (pending_preload)
    pending_preload->AddResource(resource);
}

void PreloadHelper::LoadLinksFromHeader(
    const String& header_value,
    const KURL& base_url,
    LocalFrame& frame,
    Document* document,
    LoadLinksFromHeaderMode mode,
    const ViewportDescription* viewport_description,
    std::unique_ptr<AlternateSignedExchangeResourceInfo>
        alternate_resource_info,
    const base::UnguessableToken* recursive_prefetch_token) {
  if (header_value.empty())
    return;
  LinkHeaderSet header_set(header_value);
  for (auto& header : header_set) {
    if (!header.Valid() || header.Url().empty() || header.Rel().empty()) {
      continue;
    }
    bool is_network_hint_allowed = IsNetworkHintAllowed(mode);
    bool is_resource_load_allowed =
        IsResourceLoadAllowed(mode, header.IsViewportDependent());
    bool is_compression_dictionary_load_allowed =
        IsCompressionDictionaryLoadAllowed(mode);
    if (!is_network_hint_allowed && !is_resource_load_allowed &&
        !is_compression_dictionary_load_allowed) {
      continue;
    }

    LinkLoadParameters params(header, base_url);
    bool change_rel_to_prefetch = false;

    if (params.rel.IsLinkPreload() && recursive_prefetch_token) {
      // Only preload headers are expected to have a recursive prefetch token
      // In response to that token's existence, we treat the request as a
      // prefetch.
      params.recursive_prefetch_token = *recursive_prefetch_token;
      change_rel_to_prefetch = true;
    }

    if (alternate_resource_info && params.rel.IsLinkPreload()) {
      DCHECK(document);
      KURL url = params.href;
      std::optional<ResourceType> resource_type =
          PreloadHelper::GetResourceTypeFromAsAttribute(params.as);
      if (resource_type == ResourceType::kImage &&
          !params.image_srcset.empty()) {
        // |media_values| is created based on the viewport dimensions of the
        // current page that prefetched SXGs, not on the viewport of the SXG
        // content.
        // TODO(crbug/935267): Consider supporting Viewport HTTP response
        // header. https://discourse.wicg.io/t/proposal-viewport-http-header/
        MediaValuesCached* media_values =
            CreateMediaValues(*document, viewport_description);
        url = GetBestFitImageURL(*document, base_url, media_values, params.href,
                                 params.image_srcset, params.image_sizes);
      }
      const auto* alternative_resource =
          alternate_resource_info->FindMatchingEntry(
              url, resource_type, frame.DomWindow()->navigator()->languages());
      if (alternative_resource &&
          alternative_resource->alternative_url().IsValid()) {
        UseCounter::Count(document,
                          WebFeature::kSignedExchangeSubresourcePrefetch);
        params.href = alternative_resource->alternative_url();
        // Change the rel to "prefetch" to trigger the prefetch logic. This
        // request will be handled by a PrefetchURLLoader in the browser
        // process. Note that this is triggered only during prefetch of the
        // parent resource
        //
        // The prefetched signed exchange will be stored in the browser process.
        // It will be passed to the renderer process in the next navigation, and
        // the header integrity and the inner URL will be checked before
        // processing the inner response. This renderer process can't add a new,
        // undesirable alternative resource association that affects the next
        // navigation, but can only populate things in the cache that can be
        // used by the next navigation only when they requested the same URL
        // with the same association mapping.
        change_rel_to_prefetch = true;
        // Prefetch requests for alternate SXG should be made with a
        // corsAttributeState of Anonymous, regardless of the crossorigin
        // attribute of Link:rel=preload header that triggered the prefetch. See
        // step 19.6.8 of
        // https://wicg.github.io/webpackage/loading.html#mp-link-type-prefetch.
        params.cross_origin = kCrossOriginAttributeAnonymous;
      }
    }

    if (change_rel_to_prefetch) {
      params.rel = LinkRelAttribute("prefetch");
    }

    // Sanity check to avoid re-entrancy here.
    if (params.href == base_url) {
      continue;
    }
    if (is_network_hint_allowed) {
      DnsPrefetchIfNeeded(params, document, &frame, kLinkCalledFromHeader);

      PreconnectIfNeeded(params, document, &frame, kLinkCalledFromHeader);
    }
    if (is_resource_load_allowed || is_compression_dictionary_load_allowed) {
      DCHECK(document);
      PendingLinkPreload* pending_preload =
          MakeGarbageCollected<PendingLinkPreload>(*document,
                                                   nullptr /* LinkLoader */);
      document->AddPendingLinkHeaderPreload(*pending_preload);
      if (is_resource_load_allowed) {
        PreloadIfNeeded(params, *document, base_url, kLinkCalledFromHeader,
                        viewport_description, kNotParserInserted,
                        pending_preload);
        PrefetchIfNeeded(params, *document, pending_preload);
        ModulePreloadIfNeeded(params, *document, viewport_description,
                              pending_preload);
      }
      if (is_compression_dictionary_load_allowed) {
        FetchCompressionDictionaryIfNeeded(params, *document, pending_preload);
      }
    }
    if (params.rel.IsServiceWorker()) {
      UseCounter::Count(document, WebFeature::kLinkHeaderServiceWorker);
    }
    // TODO(yoav): Add more supported headers as needed.
  }
}

// TODO(crbug.com/1413922):
// Always load the resource after the full document load completes
void PreloadHelper::FetchCompressionDictionaryIfNeeded(
    const LinkLoadParameters& params,
    Document& document,
    PendingLinkPreload* pending_preload) {
  if (!CompressionDictionaryTransportFullyEnabled(
          document.GetExecutionContext())) {
    return;
  }

  if (!document.Loader() || document.Loader()->Archive()) {
    return;
  }

  if (!params.rel.IsCompressionDictionary() || !params.href.IsValid() ||
      !document.GetFrame()) {
    return;
  }

  DVLOG(1) << "PreloadHelper::FetchCompressionDictionaryIfNeeded "
           << params.href.GetString().Utf8();
  ResourceRequest resource_request(params.href);

  resource_request.SetReferrerString(Referrer::NoReferrer());
  resource_request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
  resource_request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  resource_request.SetMode(network::mojom::RequestMode::kCors);
  resource_request.SetRequestDestination(
      network::mojom::RequestDestination::kDictionary);

  ResourceLoaderOptions options(
      document.GetExecutionContext()->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kLink;

  FetchParameters link_fetch_params(std::move(resource_request), options);
  IdleRequestOptions* idle_options = IdleRequestOptions::Create();
  ScriptedIdleTaskController::From(*document.GetExecutionContext())
      .RegisterCallback(MakeGarbageCollected<LoadDictionaryWhenIdleTask>(
                            std::move(link_fetch_params), document.Fetcher(),
                            pending_preload),
                        idle_options);
}

Resource* PreloadHelper::StartPreload(ResourceType type,
                                      FetchParameters& params,
                                      Document& document) {
  base::ElapsedTimer timer;

  ResourceFetcher* resource_fetcher = document.Fetcher();
  Resource* resource = nullptr;
  switch (type) {
    case ResourceType::kImage:
      resource = ImageResource::Fetch(params, resource_fetcher);
      break;
    case ResourceType::kScript: {
      Page* page = document.GetPage();
      v8_compile_hints::V8CrowdsourcedCompileHintsProducer*
          v8_compile_hints_producer = nullptr;
      v8_compile_hints::V8CrowdsourcedCompileHintsConsumer*
          v8_compile_hints_consumer = nullptr;
      if (page->MainFrame()->IsLocalFrame()) {
        v8_compile_hints_producer =
            &page->GetV8CrowdsourcedCompileHintsProducer();
        v8_compile_hints_consumer =
            &page->GetV8CrowdsourcedCompileHintsConsumer();
      }

      params.SetRequestContext(mojom::blink::RequestContextType::SCRIPT);
      params.SetRequestDestination(network::mojom::RequestDestination::kScript);
      const bool v8_compile_hints_magic_comment_runtime_enabled =
          RuntimeEnabledFeatures::JavaScriptCompileHintsMagicRuntimeEnabled(
              document.GetExecutionContext());

      resource = ScriptResource::Fetch(
          params, resource_fetcher, nullptr, document.GetAgent().isolate(),
          ScriptResource::kAllowStreaming, v8_compile_hints_producer,
          v8_compile_hints_consumer,
          v8_compile_hints_magic_comment_runtime_enabled);
      break;
    }
    case ResourceType::kCSSStyleSheet:
      resource =
          CSSStyleSheetResource::Fetch(params, resource_fetcher, nullptr);
      break;
    case ResourceType::kFont:
      resource = FontResource::Fetch(params, resource_fetcher, nullptr);
      if (document.GetRenderBlockingResourceManager()) {
        document.GetRenderBlockingResourceManager()
            ->EnsureStartFontPreloadMaxBlockingTimer();
      }
      document.CountUse(mojom::blink::WebFeature::kLinkRelPreloadAsFont);
      break;
    case ResourceType::kAudio:
    case ResourceType::kVideo:
      params.MutableResourceRequest().SetUseStreamOnResponse(true);
      params.MutableOptions().data_buffering_policy = kDoNotBufferData;
      resource = RawResource::FetchMedia(params, resource_fetcher, nullptr);
      break;
    case ResourceType::kTextTrack:
      params.MutableResourceRequest().SetUseStreamOnResponse(true);
      params.MutableOptions().data_buffering_policy = kDoNotBufferData;
      resource = RawResource::FetchTextTrack(params, resource_fetcher, nullptr);
      break;
    case ResourceType::kRaw:
      params.MutableResourceRequest().SetUseStreamOnResponse(true);
      params.MutableOptions().data_buffering_policy = kDoNotBufferData;
      resource = RawResource::Fetch(params, resource_fetcher, nullptr);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  base::UmaHistogramMicrosecondsTimes("Blink.PreloadRequestStartDuration",
                                      timer.Elapsed());

  return resource;
}

}  // namespace blink
