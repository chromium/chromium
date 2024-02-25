// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_resource_content_loader.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"

namespace blink {

namespace {

bool ShouldSkipFetchingUrl(const KURL& url) {
  return !url.IsValid() || url.IsAboutBlankURL() || url.IsAboutSrcdocURL();
}

bool IsServiceWorkerPresent(Document* document) {
  DocumentLoader* loader = document->Loader();
  if (!loader)
    return false;

  if (loader->GetResponse().WasFetchedViaServiceWorker())
    return true;

  WebServiceWorkerNetworkProvider* provider =
      loader->GetServiceWorkerNetworkProvider();
  if (!provider)
    return false;

  return provider->ControllerServiceWorkerID() >= 0;
}

}  // namespace

// NOTE: While this is a RawResourceClient, it loads both raw and css stylesheet
// resources. Stylesheets can only safely use a RawResourceClient because it has
// no custom interface and simply uses the base ResourceClient.
class InspectorResourceContentLoader::ResourceClient final
    : public GarbageCollected<InspectorResourceContentLoader::ResourceClient>,
      private RawResourceClient {
 public:
  explicit ResourceClient(InspectorResourceContentLoader* loader)
      : loader_(loader) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(loader_);
    RawResourceClient::Trace(visitor);
  }

 private:
  Member<InspectorResourceContentLoader> loader_;

  void NotifyFinished(Resource* resource) override {
    if (loader_)
      loader_->ResourceFinished(this);
    ClearResource();
  }

  String DebugName() const override {
    return "InspectorResourceContentLoader::ResourceClient";
  }

  friend class InspectorResourceContentLoader;
};

InspectorResourceContentLoader::InspectorResourceContentLoader(
    LocalFrame* inspected_frame)
    : all_requests_started_(false),
      started_(false),
      inspected_frame_(inspected_frame),
      last_client_id_(0) {}

void InspectorResourceContentLoader::Start() {
  started_ = true;
  HeapVector<Member<Document>> documents;
  InspectedFrames* inspected_frames =
      MakeGarbageCollected<InspectedFrames>(inspected_frame_);
  for (LocalFrame* frame : *inspected_frames) {
    if (frame->GetDocument()->IsInitialEmptyDocument())
      continue;
    documents.push_back(frame->GetDocument());
  }
  for (Document* document : documents) {
    HashSet<String> urls_to_fetch;

    ResourceRequest resource_request;
    HistoryItem* item =
        document->Loader() ? document->Loader()->GetHistoryItem() : nullptr;
    if (item) {
      resource_request =
          item->GenerateResourceRequest(mojom::FetchCacheMode::kOnlyIfCached);
    } else {
      resource_request = ResourceRequest(document->Url());
      resource_request.SetCacheMode(mojom::FetchCacheMode::kOnlyIfCached);
    }
    resource_request.SetRequestContext(
        mojom::blink::RequestContextType::INTERNAL);

    if (IsServiceWorkerPresent(document)) {
      // If the request is going to be intercepted by a service worker, then
      // don't use only-if-cached. only-if-cached will cause the service worker
      // to throw an exception if it repeats the request, which is a problem:
      // crbug.com/823392 crbug.com/1098389
      resource_request.SetCacheMode(mojom::FetchCacheMode::kDefault);
    }

    ResourceFetcher* fetcher = document->Fetcher();

    const DOMWrapperWorld* world =
        document->GetExecutionContext()->GetCurrentWorld();
    if (!ShouldSkipFetchingUrl(resource_request.Url())) {
      urls_to_fetch.insert(resource_request.Url().GetString());
      ResourceLoaderOptions options(world);
      options.initiator_info.name = fetch_initiator_type_names::kInternal;
      FetchParameters params(std::move(resource_request), options);
      ResourceClient* resource_client =
          MakeGarbageCollected<ResourceClient>(this);
      // Prevent garbage collection by holding a reference to this resource.
      resources_.push_back(
          RawResource::Fetch(params, fetcher, resource_client));
      pending_resource_clients_.insert(resource_client);
    }

    HeapVector<Member<CSSStyleSheet>> style_sheets;
    InspectorCSSAgent::CollectAllDocumentStyleSheets(document, style_sheets);
    for (CSSStyleSheet* style_sheet : style_sheets) {
      if (style_sheet->IsInline() || !style_sheet->Contents()->LoadCompleted())
        continue;
      String url = style_sheet->href();
      if (ShouldSkipFetchingUrl(KURL(url)) || urls_to_fetch.Contains(url))
        continue;
      urls_to_fetch.insert(url);
      ResourceRequest style_sheet_resource_request(url);
      style_sheet_resource_request.SetRequestContext(
          mojom::blink::RequestContextType::INTERNAL);
      ResourceLoaderOptions options(world);
      options.initiator_info.name = fetch_initiator_type_names::kInternal;
      FetchParameters params(std::move(style_sheet_resource_request), options);
      ResourceClient* resource_client =
          MakeGarbageCollected<ResourceClient>(this);
      // Prevent garbage collection by holding a reference to this resource.
      resources_.push_back(
          CSSStyleSheetResource::Fetch(params, fetcher, resource_client));
      // A cache hit for a css stylesheet will complete synchronously. Don't
      // mark the client as pending if it already finished.
      if (resource_client->GetResource())
        pending_resource_clients_.insert(resource_client);
    }

    // Fetch app manifest if available.
    // TODO (alexrudenko): This code duplicates the code in manifest_manager.cc
    // and manifest_fetcher.cc. Move it to a shared place.
    HTMLLinkElement* link_element = document->LinkManifest();
    KURL link;
    if (link_element)
      link = link_element->Href();
    if (!ShouldSkipFetchingUrl(link)) {
      auto use_credentials = EqualIgnoringASCIICase(
          link_element->FastGetAttribute(html_names::kCrossoriginAttr),
          "use-credentials");
      ResourceRequest manifest_request(link);
      manifest_request.SetMode(network::mojom::RequestMode::kCors);
      manifest_request.SetTargetAddressSpace(
          network::mojom::IPAddressSpace::kUnknown);
      // See https://w3c.github.io/manifest/. Use "include" when use_credentials
      // is true, and "omit" otherwise.
      manifest_request.SetCredentialsMode(
          use_credentials ? network::mojom::CredentialsMode::kInclude
                          : network::mojom::CredentialsMode::kOmit);
      manifest_request.SetRequestContext(
          mojom::blink::RequestContextType::MANIFEST);
      ResourceLoaderOptions manifest_options(world);
      manifest_options.initiator_info.name =
          fetch_initiator_type_names::kInternal;
      FetchParameters manifest_params(std::move(manifest_request),
                                      manifest_options);
      ResourceClient* manifest_client =
          MakeGarbageCollected<ResourceClient>(this);
      resources_.push_back(
          RawResource::Fetch(manifest_params, fetcher, manifest_client));
      if (manifest_client->GetResource())
        pending_resource_clients_.insert(manifest_client);
    }
  }

  all_requests_started_ = true;
  CheckDone();
}

int InspectorResourceContentLoader::CreateClientId() {
  return ++last_client_id_;
}

void InspectorResourceContentLoader::EnsureResourcesContentLoaded(
    int client_id,
    base::OnceClosure callback) {
  if (!started_)
    Start();
  callbacks_.insert(client_id, Callbacks())
      .stored_value->value.push_back(std::move(callback));
  CheckDone();
}

void InspectorResourceContentLoader::Cancel(int client_id) {
  callbacks_.erase(client_id);
}

InspectorResourceContentLoader::~InspectorResourceContentLoader() {
  DCHECK(resources_.empty());
}

void InspectorResourceContentLoader::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frame_);
  visitor->Trace(pending_resource_clients_);
  visitor->Trace(resources_);
}

void InspectorResourceContentLoader::DidCommitLoadForLocalFrame(
    LocalFrame* frame) {
  if (frame == inspected_frame_)
    Stop();
}

Resource* InspectorResourceContentLoader::ResourceForURL(const KURL& url) {
  for (const auto& resource : resources_) {
    if (resource->Url() == url)
      return resource.Get();
  }
  return nullptr;
}

void InspectorResourceContentLoader::Dispose() {
  Stop();
}

void InspectorResourceContentLoader::Stop() {
  HeapHashSet<Member<ResourceClient>> pending_resource_clients;
  pending_resource_clients_.swap(pending_resource_clients);
  for (const auto& client : pending_resource_clients)
    client->loader_ = nullptr;
  resources_.clear();
  // Make sure all callbacks are called to prevent infinite waiting time.
  CheckDone();
  all_requests_started_ = false;
  started_ = false;
}

bool InspectorResourceContentLoader::HasFinished() {
  return all_requests_started_ && pending_resource_clients_.size() == 0;
}

void InspectorResourceContentLoader::CheckDone() {
  if (!HasFinished())
    return;
  HashMap<int, Callbacks> callbacks;
  callbacks.swap(callbacks_);
  for (auto& key_value : callbacks) {
    for (auto& callback : key_value.value)
      std::move(callback).Run();
  }
}

void InspectorResourceContentLoader::ResourceFinished(ResourceClient* client) {
  pending_resource_clients_.erase(client);
  CheckDone();
}

}  // namespace blink
