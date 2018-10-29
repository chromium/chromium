/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/loader/link_loader.h"

#include "third_party/blink/public/platform/web_prerender.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/parser/sizes_attribute_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/link_rel_attribute.h"
#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_srcset_parser.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/importance_attribute.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/network_hints_interface.h"
#include "third_party/blink/renderer/core/loader/private/prerender_handle.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/resource/link_fetch_resource.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_finish_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/link_header.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/prerender.h"

namespace blink {

static unsigned PrerenderRelTypesFromRelAttribute(
    const LinkRelAttribute& rel_attribute,
    Document& document) {
  unsigned result = 0;
  if (rel_attribute.IsLinkPrerender()) {
    result |= kPrerenderRelTypePrerender;
    UseCounter::Count(document, WebFeature::kLinkRelPrerender);
  }
  if (rel_attribute.IsLinkNext()) {
    result |= kPrerenderRelTypeNext;
    UseCounter::Count(document, WebFeature::kLinkRelNext);
  }

  return result;
}

// TODO(domfarolino)
// Eventually we'll want to support an |importance| value on
// LinkHeaders. We can communicate a header's importance value
// to LinkLoadParameters here, likely after modifying the LinkHeader
// class. See https://crbug.com/821464 for info on Priority Hints.
LinkLoadParameters::LinkLoadParameters(const LinkHeader& header,
                                       const KURL& base_url)
    : rel(LinkRelAttribute(header.Rel())),
      cross_origin(GetCrossOriginAttributeValue(header.CrossOrigin())),
      type(header.MimeType()),
      as(header.As()),
      media(header.Media()),
      nonce(header.Nonce()),
      integrity(header.Integrity()),
      referrer_policy(kReferrerPolicyDefault),
      href(KURL(base_url, header.Url())),
      srcset(header.Srcset()),
      sizes(header.Imgsizes()) {}

class LinkLoader::FinishObserver final
    : public GarbageCollectedFinalized<ResourceFinishObserver>,
      public ResourceFinishObserver {
  USING_GARBAGE_COLLECTED_MIXIN(FinishObserver);
  USING_PRE_FINALIZER(FinishObserver, ClearResource);

 public:
  FinishObserver(LinkLoader* loader, Resource* resource)
      : loader_(loader), resource_(resource) {
    resource_->AddFinishObserver(
        this, loader_->client_->GetLoadingTaskRunner().get());
  }

  // ResourceFinishObserver implementation
  void NotifyFinished() override {
    if (!resource_)
      return;
    loader_->NotifyFinished();
    ClearResource();
  }
  String DebugName() const override {
    return "LinkLoader::ResourceFinishObserver";
  }

  Resource* GetResource() { return resource_; }
  void ClearResource() {
    if (!resource_)
      return;
    resource_->RemoveFinishObserver(this);
    resource_ = nullptr;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(loader_);
    visitor->Trace(resource_);
    blink::ResourceFinishObserver::Trace(visitor);
  }

 private:
  Member<LinkLoader> loader_;
  Member<Resource> resource_;
};

LinkLoader::LinkLoader(LinkLoaderClient* client,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : client_(client) {
  DCHECK(client_);
}

LinkLoader::~LinkLoader() = default;

void LinkLoader::NotifyFinished() {
  DCHECK(finish_observer_);
  Resource* resource = finish_observer_->GetResource();
  if (resource->ErrorOccurred())
    client_->LinkLoadingErrored();
  else
    client_->LinkLoaded();
}

// https://html.spec.whatwg.org/#link-type-modulepreload
void LinkLoader::NotifyModuleLoadFinished(ModuleScript* module) {
  // Step 11. "If result is null, fire an event named error at the link element,
  // and return." [spec text]
  // Step 12. "Fire an event named load at the link element." [spec text]
  if (!module)
    client_->LinkLoadingErrored();
  else
    client_->LinkLoaded();
}

void LinkLoader::DidStartPrerender() {
  client_->DidStartLinkPrerender();
}

void LinkLoader::DidStopPrerender() {
  client_->DidStopLinkPrerender();
}

void LinkLoader::DidSendLoadForPrerender() {
  client_->DidSendLoadForLinkPrerender();
}

void LinkLoader::DidSendDOMContentLoadedForPrerender() {
  client_->DidSendDOMContentLoadedForLinkPrerender();
}

enum LinkCaller {
  kLinkCalledFromHeader,
  kLinkCalledFromMarkup,
};

static void SendMessageToConsoleForPossiblyNullDocument(
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

static void DnsPrefetchIfNeeded(
    const LinkLoadParameters& params,
    Document* document,
    LocalFrame* frame,
    const NetworkHintsInterface& network_hints_interface,
    LinkCaller caller) {
  if (params.rel.IsDNSPrefetch()) {
    UseCounter::Count(frame, WebFeature::kLinkRelDnsPrefetch);
    if (caller == kLinkCalledFromHeader)
      UseCounter::Count(frame, WebFeature::kLinkHeaderDnsPrefetch);
    Settings* settings = frame ? frame->GetSettings() : nullptr;
    // FIXME: The href attribute of the link element can be in "//hostname"
    // form, and we shouldn't attempt to complete that as URL
    // <https://bugs.webkit.org/show_bug.cgi?id=48857>.
    if (settings && settings->GetDNSPrefetchingEnabled() &&
        params.href.IsValid() && !params.href.IsEmpty()) {
      if (settings->GetLogDnsPrefetchAndPreconnect()) {
        SendMessageToConsoleForPossiblyNullDocument(
            ConsoleMessage::Create(
                kOtherMessageSource, kVerboseMessageLevel,
                String("DNS prefetch triggered for " + params.href.Host())),
            document, frame);
      }
      network_hints_interface.DnsPrefetchHost(params.href.Host());
    }
  }
}

static void PreconnectIfNeeded(
    const LinkLoadParameters& params,
    Document* document,
    LocalFrame* frame,
    const NetworkHintsInterface& network_hints_interface,
    LinkCaller caller) {
  if (params.rel.IsPreconnect() && params.href.IsValid() &&
      params.href.ProtocolIsInHTTPFamily()) {
    UseCounter::Count(frame, WebFeature::kLinkRelPreconnect);
    if (caller == kLinkCalledFromHeader)
      UseCounter::Count(frame, WebFeature::kLinkHeaderPreconnect);
    Settings* settings = frame ? frame->GetSettings() : nullptr;
    if (settings && settings->GetLogDnsPrefetchAndPreconnect()) {
      SendMessageToConsoleForPossiblyNullDocument(
          ConsoleMessage::Create(
              kOtherMessageSource, kVerboseMessageLevel,
              String("Preconnect triggered for ") + params.href.GetString()),
          document, frame);
      if (params.cross_origin != kCrossOriginAttributeNotSet) {
        SendMessageToConsoleForPossiblyNullDocument(
            ConsoleMessage::Create(kOtherMessageSource, kVerboseMessageLevel,
                                   String("Preconnect CORS setting is ") +
                                       String((params.cross_origin ==
                                               kCrossOriginAttributeAnonymous)
                                                  ? "anonymous"
                                                  : "use-credentials")),
            document, frame);
      }
    }
    network_hints_interface.PreconnectHost(params.href, params.cross_origin);
  }
}

base::Optional<ResourceType> LinkLoader::GetResourceTypeFromAsAttribute(
    const String& as) {
  DCHECK_EQ(as.DeprecatedLower(), as);
  if (as == "image") {
    return ResourceType::kImage;
  } else if (as == "script") {
    return ResourceType::kScript;
  } else if (as == "style") {
    return ResourceType::kCSSStyleSheet;
  } else if (as == "video") {
    return ResourceType::kVideo;
  } else if (as == "audio") {
    return ResourceType::kAudio;
  } else if (as == "track") {
    return ResourceType::kTextTrack;
  } else if (as == "font") {
    return ResourceType::kFont;
  } else if (as == "fetch") {
    return ResourceType::kRaw;
  }
  return base::nullopt;
}

Resource* LinkLoader::GetResourceForTesting() {
  return finish_observer_ ? finish_observer_->GetResource() : nullptr;
}

static bool IsSupportedType(ResourceType resource_type,
                            const String& mime_type) {
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

static MediaValues* CreateMediaValues(
    Document& document,
    ViewportDescription* viewport_description) {
  MediaValues* media_values =
      MediaValues::CreateDynamicIfFrameExists(document.GetFrame());
  if (viewport_description) {
    media_values->OverrideViewportDimensions(
        viewport_description->max_width.GetFloatValue(),
        viewport_description->max_height.GetFloatValue());
  }
  return media_values;
}

static bool MediaMatches(const String& media, MediaValues* media_values) {
  scoped_refptr<MediaQuerySet> media_queries = MediaQuerySet::Create(media);
  MediaQueryEvaluator evaluator(*media_values);
  return evaluator.Eval(*media_queries);
}

// |base_url| is used in Link HTTP Header based preloads to resolve relative
// URLs in srcset, which should be based on the resource's URL, not the
// document's base URL. If |base_url| is a null URL, relative URLs are resolved
// using |document.CompleteURL()|.
static Resource* PreloadIfNeeded(const LinkLoadParameters& params,
                                 Document& document,
                                 const KURL& base_url,
                                 LinkCaller caller,
                                 ViewportDescription* viewport_description,
                                 ParserDisposition parser_disposition) {
  if (!document.Loader() || !params.rel.IsLinkPreload())
    return nullptr;

  base::Optional<ResourceType> resource_type =
      LinkLoader::GetResourceTypeFromAsAttribute(params.as);

  MediaValues* media_values = nullptr;
  KURL url;
  if (resource_type == ResourceType::kImage && !params.srcset.IsEmpty() &&
      RuntimeEnabledFeatures::PreloadImageSrcSetEnabled()) {
    media_values = CreateMediaValues(document, viewport_description);
    float source_size =
        SizesAttributeParser(media_values, params.sizes).length();
    ImageCandidate candidate = BestFitSourceForImageAttributes(
        media_values->DevicePixelRatio(), source_size, params.href,
        params.srcset);
    url = base_url.IsNull() ? document.CompleteURL(candidate.ToString())
                            : KURL(base_url, candidate.ToString());
  } else {
    url = params.href;
  }

  UseCounter::Count(document, WebFeature::kLinkRelPreload);
  if (!url.IsValid() || url.IsEmpty()) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        String("<link rel=preload> has an invalid `href` value")));
    return nullptr;
  }

  // Preload only if media matches
  if (!params.media.IsEmpty()) {
    if (!media_values)
      media_values = CreateMediaValues(document, viewport_description);
    if (!MediaMatches(params.media, media_values))
      return nullptr;
  }

  if (caller == kLinkCalledFromHeader)
    UseCounter::Count(document, WebFeature::kLinkHeaderPreload);
  if (resource_type == base::nullopt) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        String("<link rel=preload> must have a valid `as` value")));
    return nullptr;
  }

  if (!IsSupportedType(resource_type.value(), params.type)) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        String("<link rel=preload> has an unsupported `type` value")));
    return nullptr;
  }
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(ResourceFetcher::DetermineRequestContext(
      resource_type.value(), ResourceFetcher::kImageNotImageSet, false));

  resource_request.SetReferrerPolicy(params.referrer_policy);

  resource_request.SetFetchImportanceMode(
      GetFetchImportanceAttributeValue(params.importance));

  ResourceLoaderOptions options;
  options.initiator_info.name = FetchInitiatorTypeNames::link;
  options.parser_disposition = parser_disposition;
  FetchParameters link_fetch_params(resource_request, options);
  link_fetch_params.SetCharset(document.Encoding());

  if (params.cross_origin != kCrossOriginAttributeNotSet) {
    link_fetch_params.SetCrossOriginAccessControl(document.GetSecurityOrigin(),
                                                  params.cross_origin);
  }
  link_fetch_params.SetContentSecurityPolicyNonce(params.nonce);
  Settings* settings = document.GetSettings();
  if (settings && settings->GetLogPreload()) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kVerboseMessageLevel,
        String("Preload triggered for " + url.Host() + url.GetPath())));
  }
  link_fetch_params.SetLinkPreload(true);
  return document.Loader()->StartPreload(resource_type.value(),
                                         link_fetch_params);
}

// https://html.spec.whatwg.org/multipage/links.html#link-type-modulepreload
static void ModulePreloadIfNeeded(const LinkLoadParameters& params,
                                  Document& document,
                                  ViewportDescription* viewport_description,
                                  LinkLoader* link_loader) {
  if (!document.Loader() || !params.rel.IsModulePreload())
    return;

  UseCounter::Count(document, WebFeature::kLinkRelModulePreload);

  // Step 1. "If the href attribute's value is the empty string, then return."
  // [spec text]
  if (params.href.IsEmpty()) {
    document.AddConsoleMessage(
        ConsoleMessage::Create(kOtherMessageSource, kWarningMessageLevel,
                               "<link rel=modulepreload> has no `href` value"));
    return;
  }

  // Step 2. "Let destination be the current state of the as attribute (a
  // destination), or "script" if it is in no state." [spec text]
  // Step 3. "If destination is not script-like, then queue a task on the
  // networking task source to fire an event named error at the link element,
  // and return." [spec text]
  // Currently we only support as="script".
  if (!params.as.IsEmpty() && params.as != "script") {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        String("<link rel=modulepreload> has an invalid `as` value " +
               params.as)));
    if (link_loader)
      link_loader->DispatchLinkLoadingErroredAsync();
    return;
  }
  mojom::RequestContextType destination = mojom::RequestContextType::SCRIPT;

  // Step 4. "Parse the URL given by the href attribute, relative to the
  // element's node document. If that fails, then return. Otherwise, let url be
  // the resulting URL record." [spec text]
  // |href| is already resolved in caller side.
  if (!params.href.IsValid()) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        "<link rel=modulepreload> has an invalid `href` value " +
            params.href.GetString()));
    return;
  }

  // Preload only if media matches.
  // https://html.spec.whatwg.org/#processing-the-media-attribute
  if (!params.media.IsEmpty()) {
    MediaValues* media_values =
        CreateMediaValues(document, viewport_description);
    if (!MediaMatches(params.media, media_values))
      return;
  }

  // Step 5. "Let settings object be the link element's node document's relevant
  // settings object." [spec text]
  // |document| is the node document here, and its context document is the
  // relevant settings object.
  Document* context_document = document.ContextDocument();
  auto* settings_object =
      context_document->CreateFetchClientSettingsObjectSnapshot();

  Modulator* modulator =
      Modulator::From(ToScriptStateForMainWorld(context_document->GetFrame()));
  DCHECK(modulator);
  if (!modulator)
    return;

  // Step 6. "Let credentials mode be the module script credentials mode for the
  // crossorigin attribute." [spec text]
  network::mojom::FetchCredentialsMode credentials_mode =
      ScriptLoader::ModuleScriptCredentialsMode(params.cross_origin);

  // Step 7. "Let cryptographic nonce be the value of the nonce attribute, if it
  // is specified, or the empty string otherwise." [spec text]
  // |nonce| parameter is the value of the nonce attribute.

  // Step 8. "Let integrity metadata be the value of the integrity attribute, if
  // it is specified, or the empty string otherwise." [spec text]
  IntegrityMetadataSet integrity_metadata;
  if (!params.integrity.IsEmpty()) {
    SubresourceIntegrity::IntegrityFeatures integrity_features =
        SubresourceIntegrityHelper::GetFeatures(&document);
    SubresourceIntegrity::ReportInfo report_info;
    SubresourceIntegrity::ParseIntegrityAttribute(
        params.integrity, integrity_features, integrity_metadata, &report_info);
    SubresourceIntegrityHelper::DoReport(document, report_info);
  }

  // Step 9. "Let referrer policy be the current state of the element's
  // referrerpolicy attribute." [spec text]
  // |referrer_policy| parameter is the value of the referrerpolicy attribute.

  // Step 10. "Let options be a script fetch options whose cryptographic nonce
  // is cryptographic nonce, integrity metadata is integrity metadata, parser
  // metadata is "not-parser-inserted", credentials mode is credentials mode,
  // and referrer policy is referrer policy." [spec text]
  ModuleScriptFetchRequest request(
      params.href, destination,
      ScriptFetchOptions(params.nonce, integrity_metadata, params.integrity,
                         kNotParserInserted, credentials_mode,
                         params.referrer_policy),
      Referrer::NoReferrer(), TextPosition::MinimumPosition());

  // Step 11. "Fetch a single module script given url, settings object,
  // destination, options, settings object, "client", and with the top-level
  // module fetch flag set. Wait until algorithm asynchronously completes with
  // result." [spec text]
  modulator->FetchSingle(request, settings_object,
                         ModuleGraphLevel::kDependentModuleFetch,
                         ModuleScriptCustomFetchType::kNone, link_loader);

  Settings* settings = document.GetSettings();
  if (settings && settings->GetLogPreload()) {
    document.AddConsoleMessage(
        ConsoleMessage::Create(kOtherMessageSource, kVerboseMessageLevel,
                               "Module preload triggered for " +
                                   params.href.Host() + params.href.GetPath()));
  }

  // Asynchronously continue processing after
  // LinkLoader::NotifyModuleLoadFinished() is called.
}

static Resource* PrefetchIfNeeded(const LinkLoadParameters& params,
                                  Document& document) {
  if (params.rel.IsLinkPrefetch() && params.href.IsValid() &&
      document.GetFrame()) {
    UseCounter::Count(document, WebFeature::kLinkRelPrefetch);

    ResourceRequest resource_request(params.href);
    resource_request.SetReferrerPolicy(params.referrer_policy);
    resource_request.SetFetchImportanceMode(
        GetFetchImportanceAttributeValue(params.importance));

    ResourceLoaderOptions options;
    options.initiator_info.name = FetchInitiatorTypeNames::link;

    FetchParameters link_fetch_params(resource_request, options);
    if (params.cross_origin != kCrossOriginAttributeNotSet) {
      link_fetch_params.SetCrossOriginAccessControl(
          document.GetSecurityOrigin(), params.cross_origin);
    }
    return LinkFetchResource::Fetch(ResourceType::kLinkPrefetch,
                                    link_fetch_params, document.Fetcher());
  }
  return nullptr;
}

void LinkLoader::LoadLinksFromHeader(
    const String& header_value,
    const KURL& base_url,
    LocalFrame& frame,
    Document* document,
    const NetworkHintsInterface& network_hints_interface,
    CanLoadResources can_load_resources,
    MediaPreloadPolicy media_policy,
    ViewportDescriptionWrapper* viewport_description_wrapper) {
  if (header_value.IsEmpty())
    return;
  LinkHeaderSet header_set(header_value);
  for (auto& header : header_set) {
    if (!header.Valid() || header.Url().IsEmpty() || header.Rel().IsEmpty())
      continue;

    if (media_policy == kOnlyLoadMedia && header.Media().IsEmpty())
      continue;
    if (media_policy == kOnlyLoadNonMedia && !header.Media().IsEmpty())
      continue;

    const LinkLoadParameters params(header, base_url);
    // Sanity check to avoid re-entrancy here.
    if (params.href == base_url)
      continue;
    if (can_load_resources != kOnlyLoadResources) {
      DnsPrefetchIfNeeded(params, document, &frame, network_hints_interface,
                          kLinkCalledFromHeader);

      PreconnectIfNeeded(params, document, &frame, network_hints_interface,
                         kLinkCalledFromHeader);
    }
    if (can_load_resources != kDoNotLoadResources) {
      DCHECK(document);
      ViewportDescription* viewport_description =
          (viewport_description_wrapper && viewport_description_wrapper->set)
              ? &(viewport_description_wrapper->description)
              : nullptr;

      PreloadIfNeeded(params, *document, base_url, kLinkCalledFromHeader,
                      viewport_description, kNotParserInserted);
      PrefetchIfNeeded(params, *document);
      ModulePreloadIfNeeded(params, *document, viewport_description, nullptr);
    }
    if (params.rel.IsServiceWorker()) {
      UseCounter::Count(&frame, WebFeature::kLinkHeaderServiceWorker);
    }
    // TODO(yoav): Add more supported headers as needed.
  }
}

bool LinkLoader::LoadLink(
    const LinkLoadParameters& params,
    Document& document,
    const NetworkHintsInterface& network_hints_interface) {
  // If any loading process is in progress, abort it.
  Abort();

  if (!client_->ShouldLoadLink())
    return false;

  DnsPrefetchIfNeeded(params, &document, document.GetFrame(),
                      network_hints_interface, kLinkCalledFromMarkup);

  PreconnectIfNeeded(params, &document, document.GetFrame(),
                     network_hints_interface, kLinkCalledFromMarkup);

  Resource* resource = PreloadIfNeeded(
      params, document, NullURL(), kLinkCalledFromMarkup, nullptr,
      client_->IsLinkCreatedByParser() ? kParserInserted : kNotParserInserted);
  if (!resource) {
    resource = PrefetchIfNeeded(params, document);
  }
  if (resource)
    finish_observer_ = new FinishObserver(this, resource);

  ModulePreloadIfNeeded(params, document, nullptr, this);

  if (const unsigned prerender_rel_types =
          PrerenderRelTypesFromRelAttribute(params.rel, document)) {
    if (!prerender_) {
      prerender_ = PrerenderHandle::Create(document, this, params.href,
                                           prerender_rel_types);
    } else if (prerender_->Url() != params.href) {
      prerender_->Cancel();
      prerender_ = PrerenderHandle::Create(document, this, params.href,
                                           prerender_rel_types);
    }
    // TODO(gavinp): Handle changes to rel types of existing prerenders.
  } else if (prerender_) {
    prerender_->Cancel();
    prerender_.Clear();
  }
  return true;
}

void LinkLoader::LoadStylesheet(const LinkLoadParameters& params,
                                const AtomicString& local_name,
                                const WTF::TextEncoding& charset,
                                FetchParameters::DeferOption defer_option,
                                Document& document,
                                ResourceClient* link_client) {
  ResourceRequest resource_request(document.CompleteURL(params.href));
  resource_request.SetReferrerPolicy(params.referrer_policy);

  mojom::FetchImportanceMode importance_mode =
      GetFetchImportanceAttributeValue(params.importance);
  DCHECK(importance_mode == mojom::FetchImportanceMode::kImportanceAuto ||
         RuntimeEnabledFeatures::PriorityHintsEnabled());
  resource_request.SetFetchImportanceMode(importance_mode);

  ResourceLoaderOptions options;
  options.initiator_info.name = local_name;
  FetchParameters link_fetch_params(resource_request, options);
  link_fetch_params.SetCharset(charset);

  link_fetch_params.SetDefer(defer_option);

  link_fetch_params.SetContentSecurityPolicyNonce(params.nonce);

  CrossOriginAttributeValue cross_origin = params.cross_origin;
  if (cross_origin != kCrossOriginAttributeNotSet) {
    link_fetch_params.SetCrossOriginAccessControl(document.GetSecurityOrigin(),
                                                  cross_origin);
  }

  String integrity_attr = params.integrity;
  if (!integrity_attr.IsEmpty()) {
    IntegrityMetadataSet metadata_set;
    SubresourceIntegrity::ParseIntegrityAttribute(
        integrity_attr, SubresourceIntegrityHelper::GetFeatures(&document),
        metadata_set);
    link_fetch_params.SetIntegrityMetadata(metadata_set);
    link_fetch_params.MutableResourceRequest().SetFetchIntegrity(
        integrity_attr);
  }

  CSSStyleSheetResource::Fetch(link_fetch_params, document.Fetcher(),
                               link_client);
}

void LinkLoader::DispatchLinkLoadingErroredAsync() {
  client_->GetLoadingTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&LinkLoaderClient::LinkLoadingErrored,
                           WrapPersistent(client_.Get())));
}

void LinkLoader::Abort() {
  if (prerender_) {
    prerender_->Cancel();
    prerender_.Clear();
  }
  if (finish_observer_) {
    finish_observer_->ClearResource();
    finish_observer_ = nullptr;
  }
}

void LinkLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(finish_observer_);
  visitor->Trace(client_);
  visitor->Trace(prerender_);
  SingleModuleClient::Trace(visitor);
  PrerenderClient::Trace(visitor);
}

}  // namespace blink
