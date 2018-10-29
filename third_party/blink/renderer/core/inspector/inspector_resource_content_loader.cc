// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_resource_content_loader.h"

#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
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

// NOTE: While this is a RawResourceClient, it loads both raw and css stylesheet
// resources. Stylesheets can only safely use a RawResourceClient because it has
// no custom interface and simply uses the base ResourceClient.
class InspectorResourceContentLoader::ResourceClient final
    : public GarbageCollectedFinalized<
          InspectorResourceContentLoader::ResourceClient>,
      private RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(ResourceClient);

 public:
  explicit ResourceClient(InspectorResourceContentLoader* loader)
      : loader_(loader) {}

  void Trace(blink::Visitor* visitor) override {
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
  InspectedFrames* inspected_frames = new InspectedFrames(inspected_frame_);
  for (LocalFrame* frame : *inspected_frames) {
    documents.push_back(frame->GetDocument());
    documents.AppendVector(InspectorPageAgent::ImportsForFrame(frame));
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
    resource_request.SetRequestContext(mojom::RequestContextType::INTERNAL);

    if (!resource_request.Url().GetString().IsEmpty()) {
      urls_to_fetch.insert(resource_request.Url().GetString());
      ResourceLoaderOptions options;
      options.initiator_info.name = FetchInitiatorTypeNames::internal;
      FetchParameters params(resource_request, options);
      ResourceClient* resource_client = new ResourceClient(this);
      // Prevent garbage collection by holding a reference to this resource.
      resources_.push_back(
          RawResource::Fetch(params, document->Fetcher(), resource_client));
      pending_resource_clients_.insert(resource_client);
    }

    HeapVector<Member<CSSStyleSheet>> style_sheets;
    InspectorCSSAgent::CollectAllDocumentStyleSheets(document, style_sheets);
    for (CSSStyleSheet* style_sheet : style_sheets) {
      if (style_sheet->IsInline() || !style_sheet->Contents()->LoadCompleted())
        continue;
      String url = style_sheet->href();
      if (url.IsEmpty() || urls_to_fetch.Contains(url))
        continue;
      urls_to_fetch.insert(url);
      ResourceRequest resource_request(url);
      resource_request.SetRequestContext(mojom::RequestContextType::INTERNAL);
      ResourceLoaderOptions options;
      options.initiator_info.name = FetchInitiatorTypeNames::internal;
      FetchParameters params(resource_request, options);
      ResourceClient* resource_client = new ResourceClient(this);
      // Prevent garbage collection by holding a reference to this resource.
      resources_.push_back(CSSStyleSheetResource::Fetch(
          params, document->Fetcher(), resource_client));
      // A cache hit for a css stylesheet will complete synchronously. Don't
      // mark the client as pending if it already finished.
      if (resource_client->GetResource())
        pending_resource_clients_.insert(resource_client);
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
  DCHECK(resources_.IsEmpty());
}

void InspectorResourceContentLoader::Trace(blink::Visitor* visitor) {
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
      return resource;
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
