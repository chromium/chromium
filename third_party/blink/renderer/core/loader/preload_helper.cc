// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/preload_helper.h"

#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/parser/sizes_attribute_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_srcset_parser.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/alternate_signed_exchange_resource_info.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/importance_attribute.h"
#include "third_party/blink/renderer/core/loader/link_load_parameters.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/loader/resource/link_prefetch_resource.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
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
  if (mime_type.IsEmpty())
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
      NOTREACHED();
  }
  return false;
}

MediaValues* CreateMediaValues(
    Document& document,
    const ViewportDescription* viewport_description) {
  MediaValues* media_values =
      MediaValues::CreateDynamicIfFrameExists(document.GetFrame());
  if (viewport_description) {
    FloatSize initial_viewport(media_values->DeviceWidth(),
                               media_values->DeviceHeight());
    PageScaleConstraints constraints = viewport_description->Resolve(
        initial_viewport, document.GetViewportData().ViewportDefaultMinWidth());
    media_values->OverrideViewportDimensions(constraints.layout_size.Width(),
                                             constraints.layout_size.Height());
  }
  return media_values;
}

bool MediaMatches(const String& media,
                  MediaValues* media_values,
                  const ExecutionContext* execution_context) {
  scoped_refptr<MediaQuerySet> media_queries =
      MediaQuerySet::Create(media, execution_context);
  MediaQueryEvaluator evaluator(*media_values);
  return evaluator.Eval(*media_queries);
}

KURL GetBestFitImageURL(const Document& document,
                        const KURL& base_url,
                        MediaValues* media_values,
                        const KURL& href,
                        const String& image_srcset,
                        const String& image_sizes) {
  float source_size = SizesAttributeParser(media_values, image_sizes,
                                           document.GetExecutionContext())
                          .length();
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
                mojom::ConsoleMessageSource::kOther,
                mojom::ConsoleMessageLevel::kVerbose,
                String("DNS prefetch triggered for " + params.href.Host())),
            document, frame);
      }
      WebPrescientNetworking* web_prescient_networking =
          frame ? frame->PrescientNetworking() : nullptr;
      if (web_prescient_networking) {
        web_prescient_networking->PrefetchDNS(params.href.Host());
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
              mojom::ConsoleMessageSource::kOther,
              mojom::ConsoleMessageLevel::kVerbose,
              String("Preconnect triggered for ") + params.href.GetString()),
          document, frame);
      if (params.cross_origin != kCrossOriginAttributeNotSet) {
        SendMessageToConsoleForPossiblyNullDocument(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::ConsoleMessageSource::kOther,
                mojom::ConsoleMessageLevel::kVerbose,
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
absl::optional<ResourceType> PreloadHelper::GetResourceTypeFromAsAttribute(
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
  return absl::nullopt;
}

// |base_url| is used in Link HTTP Header based preloads to resolve relative
// URLs in srcset, which should be based on the resource's URL, not the
// document's base URL. If |base_url| is a null URL, relative URLs are resolved
// using |document.CompleteURL()|.
Resource* PreloadHelper::PreloadIfNeeded(
    const LinkLoadParameters& params,
    Document& document,
    const KURL& base_url,
    LinkCaller caller,
    const ViewportDescription* viewport_description,
    ParserDisposition parser_disposition) {
  if (!document.Loader() || !params.rel.IsLinkPreload())
    return nullptr;

  absl::optional<ResourceType> resource_type =
      PreloadHelper::GetResourceTypeFromAsAttribute(params.as);

  MediaValues* media_values = nullptr;
  KURL url;
  if (resource_type == ResourceType::kImage && !params.image_srcset.IsEmpty()) {
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
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kWarning,
        String("<link rel=preload> has an invalid `href` value")));
    return nullptr;
  }

  // Preload only if media matches
  if (!params.media.IsEmpty()) {
    if (!media_values)
      media_values = CreateMediaValues(document, viewport_description);
    if (!MediaMatches(params.media, media_values,
                      document.GetExecutionContext()))
      return nullptr;
  }

  if (caller == kLinkCalledFromHeader)
    UseCounter::Count(document, WebFeature::kLinkHeaderPreload);
  if (resource_type == absl::nullopt) {
    String message;
    if (IsValidButUnsupportedAsAttribute(params.as)) {
      message = String("<link rel=preload> uses an unsupported `as` value");
    } else {
      message = String("<link rel=preload> must have a valid `as` value");
    }
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning, message));
    return nullptr;
  }
  if (!IsSupportedType(resource_type.value(), params.type)) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kWarning,
        String("<link rel=preload> has an unsupported `type` value")));
    return nullptr;
  }
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(ResourceFetcher::DetermineRequestContext(
      resource_type.value(), ResourceFetcher::kImageNotImageSet));
  resource_request.SetRequestDestination(
      ResourceFetcher::DetermineRequestDestination(resource_type.value()));

  resource_request.SetReferrerPolicy(params.referrer_policy);

  resource_request.SetFetchImportanceMode(
      GetFetchImportanceAttributeValue(params.importance));

  ResourceLoaderOptions options(
      document.GetExecutionContext()->GetCurrentWorld());

  options.initiator_info.name = fetch_initiator_type_names::kLink;
  options.parser_disposition = parser_disposition;
  FetchParameters link_fetch_params(std::move(resource_request), options);
  link_fetch_params.SetCharset(document.Encoding());
  link_fetch_params.SetRenderBlockingBehavior(
      RenderBlockingBehavior::kNonBlocking);

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
      resource_type == ResourceType::kCSSStyleSheet) {
    if (!integrity_attr.IsEmpty()) {
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
    if (!integrity_attr.IsEmpty()) {
      document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kOther,
          mojom::ConsoleMessageLevel::kWarning,
          String("The `integrity` attribute is currently ignored for preload "
                 "destinations that do not support subresource integrity. See "
                 "https://crbug.com/981419 for more information")));
    }
  }

  link_fetch_params.SetContentSecurityPolicyNonce(params.nonce);
  Settings* settings = document.GetSettings();
  if (settings && settings->GetLogPreload()) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kVerbose,
        String("Preload triggered for " + url.Host() + url.GetPath())));
  }
  link_fetch_params.SetLinkPreload(true);
  return PreloadHelper::StartPreload(resource_type.value(), link_fetch_params,
                                     document);
}

// https://html.spec.whatwg.org/C/#link-type-modulepreload
void PreloadHelper::ModulePreloadIfNeeded(
    const LinkLoadParameters& params,
    Document& document,
    const ViewportDescription* viewport_description,
    SingleModuleClient* client) {
  if (!document.Loader() || !params.rel.IsModulePreload())
    return;

  UseCounter::Count(document, WebFeature::kLinkRelModulePreload);

  // Step 1. "If the href attribute's value is the empty string, then return."
  // [spec text]
  if (params.href.IsEmpty()) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kWarning,
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
  if (!params.as.IsEmpty() && params.as != "script") {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kWarning,
        String("<link rel=modulepreload> has an invalid `as` value " +
               params.as)));
    // This triggers the same logic as Step 11 asynchronously, which will fire
    // the error event.
    if (client) {
      modulator->TaskRunner()->PostTask(
          FROM_HERE, WTF::Bind(&SingleModuleClient::NotifyModuleLoadFinished,
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
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kWarning,
        "<link rel=modulepreload> has an invalid `href` value " +
            params.href.GetString()));
    return;
  }

  // Preload only if media matches.
  // https://html.spec.whatwg.org/C/#processing-the-media-attribute
  if (!params.media.IsEmpty()) {
    MediaValues* media_values =
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

  // Step 8. "Let integrity metadata be the value of the integrity attribute, if
  // it is specified, or the empty string otherwise." [spec text]
  IntegrityMetadataSet integrity_metadata;
  if (!params.integrity.IsEmpty()) {
    SubresourceIntegrity::IntegrityFeatures integrity_features =
        SubresourceIntegrityHelper::GetFeatures(document.GetExecutionContext());
    SubresourceIntegrity::ReportInfo report_info;
    SubresourceIntegrity::ParseIntegrityAttribute(
        params.integrity, integrity_features, integrity_metadata, &report_info);
    SubresourceIntegrityHelper::DoReport(*document.GetExecutionContext(),
                                         report_info);
  }

  // Step 9. "Let referrer policy be the current state of the element's
  // referrerpolicy attribute." [spec text]
  // |referrer_policy| parameter is the value of the referrerpolicy attribute.

  // Step 10. "Let options be a script fetch options whose cryptographic nonce
  // is cryptographic nonce, integrity metadata is integrity metadata, parser
  // metadata is "not-parser-inserted", credentials mode is credentials mode,
  // and referrer policy is referrer policy." [spec text]
  ModuleScriptFetchRequest request(
      params.href, ModuleType::kJavaScript, context_type, destination,
      ScriptFetchOptions(params.nonce, integrity_metadata, params.integrity,
                         kNotParserInserted, credentials_mode,
                         params.referrer_policy,
                         mojom::blink::FetchImportanceMode::kImportanceAuto,
                         RenderBlockingBehavior::kNonBlocking),
      Referrer::NoReferrer(), TextPosition::MinimumPosition());

  // Step 11. "Fetch a single module script given url, settings object,
  // destination, options, settings object, "client", and with the top-level
  // module fetch flag set. Wait until algorithm asynchronously completes with
  // result." [spec text]
  modulator->FetchSingle(request, window->Fetcher(),
                         ModuleGraphLevel::kDependentModuleFetch,
                         ModuleScriptCustomFetchType::kNone, client);

  Settings* settings = document.GetSettings();
  if (settings && settings->GetLogPreload()) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kVerbose,
        "Module preload triggered for " + params.href.Host() +
            params.href.GetPath()));
  }

  // Asynchronously continue processing after
  // client->NotifyModuleLoadFinished() is called.
}

Resource* PreloadHelper::PrefetchIfNeeded(const LinkLoadParameters& params,
                                          Document& document) {
  if (document.Loader() && document.Loader()->Archive()) {
    return nullptr;
  }

  if (params.rel.IsLinkPrefetch() && params.href.IsValid() &&
      document.GetFrame()) {
    UseCounter::Count(document, WebFeature::kLinkRelPrefetch);

    ResourceRequest resource_request(params.href);

    // Later a security check is done asserting that the initiator of a
    // cross-origin prefetch request is same-origin with the origin that the
    // browser process is aware of. However, since opaque request initiators are
    // always cross-origin with every other origin, we must not request
    // cross-origin prefetches from opaque requestors.
    if (EqualIgnoringASCIICase(params.as, "document") &&
        !document.GetExecutionContext()->GetSecurityOrigin()->IsOpaque()) {
      resource_request.SetPrefetchMaybeForTopLevelNavigation(true);
    }

    // This request could have originally been a preload header on a prefetch
    // response, that was promoted to a prefetch request by LoadLinksFromHeader.
    // In that case, it may have a recursive prefetch token used by the browser
    // process to ensure this request is cached correctly. Propagate it.
    resource_request.SetRecursivePrefetchToken(params.recursive_prefetch_token);

    resource_request.SetReferrerPolicy(params.referrer_policy);
    resource_request.SetFetchImportanceMode(
        GetFetchImportanceAttributeValue(params.importance));

    if (base::FeatureList::IsEnabled(features::kPrefetchPrivacyChanges)) {
      resource_request.SetRedirectMode(network::mojom::RedirectMode::kError);
      resource_request.SetReferrerPolicy(
          network::mojom::ReferrerPolicy::kNever);
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
    link_fetch_params.SetSignedExchangePrefetchCacheEnabled(
        RuntimeEnabledFeatures::
            SignedExchangePrefetchCacheForNavigationsEnabled() ||
        RuntimeEnabledFeatures::SignedExchangeSubresourcePrefetchEnabled(
            document.GetExecutionContext()));
    return LinkPrefetchResource::Fetch(link_fetch_params, document.Fetcher());
  }
  return nullptr;
}

void PreloadHelper::LoadLinksFromHeader(
    const String& header_value,
    const KURL& base_url,
    LocalFrame& frame,
    Document* document,
    CanLoadResources can_load_resources,
    MediaPreloadPolicy media_policy,
    const ViewportDescription* viewport_description,
    std::unique_ptr<AlternateSignedExchangeResourceInfo>
        alternate_resource_info,
    const base::UnguessableToken* recursive_prefetch_token) {
  if (header_value.IsEmpty())
    return;
  LinkHeaderSet header_set(header_value);
  for (auto& header : header_set) {
    if (!header.Valid() || header.Url().IsEmpty() || header.Rel().IsEmpty())
      continue;

    if (media_policy == kOnlyLoadMedia && !header.IsViewportDependent())
      continue;
    if (media_policy == kOnlyLoadNonMedia && header.IsViewportDependent())
      continue;

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
      DCHECK(RuntimeEnabledFeatures::SignedExchangeSubresourcePrefetchEnabled(
          document->GetExecutionContext()));
      KURL url = params.href;
      absl::optional<ResourceType> resource_type =
          PreloadHelper::GetResourceTypeFromAsAttribute(params.as);
      if (resource_type == ResourceType::kImage &&
          !params.image_srcset.IsEmpty()) {
        // |media_values| is created based on the viewport dimensions of the
        // current page that prefetched SXGs, not on the viewport of the SXG
        // content.
        // TODO(crbug/935267): Consider supporting Viewport HTTP response
        // header. https://discourse.wicg.io/t/proposal-viewport-http-header/
        MediaValues* media_values =
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
      }
    }

    if (change_rel_to_prefetch)
      params.rel = LinkRelAttribute("prefetch");

    // Sanity check to avoid re-entrancy here.
    if (params.href == base_url)
      continue;
    if (can_load_resources != kOnlyLoadResources) {
      DnsPrefetchIfNeeded(params, document, &frame, kLinkCalledFromHeader);

      PreconnectIfNeeded(params, document, &frame, kLinkCalledFromHeader);
    }
    if (can_load_resources != kDoNotLoadResources) {
      DCHECK(document);
      PreloadIfNeeded(params, *document, base_url, kLinkCalledFromHeader,
                      viewport_description, kNotParserInserted);
      PrefetchIfNeeded(params, *document);
      ModulePreloadIfNeeded(params, *document, viewport_description, nullptr);
    }
    if (params.rel.IsServiceWorker()) {
      UseCounter::Count(document, WebFeature::kLinkHeaderServiceWorker);
    }
    // TODO(yoav): Add more supported headers as needed.
  }
}

Resource* PreloadHelper::StartPreload(ResourceType type,
                                      FetchParameters& params,
                                      Document& document) {
  ResourceFetcher* resource_fetcher = document.Fetcher();
  Resource* resource = nullptr;
  switch (type) {
    case ResourceType::kImage:
      resource = ImageResource::Fetch(params, resource_fetcher);
      break;
    case ResourceType::kScript:
      params.SetRequestContext(mojom::blink::RequestContextType::SCRIPT);
      params.SetRequestDestination(network::mojom::RequestDestination::kScript);
      resource = ScriptResource::Fetch(params, resource_fetcher, nullptr,
                                       ScriptResource::kAllowStreaming);
      break;
    case ResourceType::kCSSStyleSheet:
      resource =
          CSSStyleSheetResource::Fetch(params, resource_fetcher, nullptr);
      break;
    case ResourceType::kFont:
      resource = FontResource::Fetch(params, resource_fetcher, nullptr);
      document.GetFontPreloadManager().FontPreloadingStarted(
          To<FontResource>(resource));
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
      NOTREACHED();
  }

  return resource;
}

}  // namespace blink
