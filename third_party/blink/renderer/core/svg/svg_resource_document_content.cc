/*
    Copyright (C) 2010 Rob Buis <rwlbuis@gmail.com>
    Copyright (C) 2011 Cosmin Truta <ctruta@gmail.com>
    Copyright (C) 2012 University of Szeged
    Copyright (C) 2012 Renata Hodovan <reni@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/core/svg/svg_resource_document_content.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/resource/svg_document_resource.h"
#include "third_party/blink/renderer/core/svg/svg_resource_document_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

class SVGExternalDocumentCache final
    : public GarbageCollected<SVGExternalDocumentCache>,
      public Supplement<LocalFrame> {
 public:
  static const char kSupplementName[];
  static SVGExternalDocumentCache* From(LocalFrame&);
  explicit SVGExternalDocumentCache(LocalFrame&);

  // The key is "URL (without fragment)" and the request mode (kSameOrigin or
  // kCors - other modes should be filtered by AllowedRequestMode).
  using CacheKey = std::pair<String, network::mojom::blink::RequestMode>;

  static CacheKey MakeCacheKey(const FetchParameters& params);

  SVGResourceDocumentContent* Get(const CacheKey& key);
  void Put(const CacheKey& key, SVGResourceDocumentContent* content);

  void Trace(Visitor*) const override;

 private:
  HeapHashMap<CacheKey, WeakMember<SVGResourceDocumentContent>> entries_;
};

const char SVGExternalDocumentCache::kSupplementName[] =
    "SVGExternalDocumentCache";

SVGExternalDocumentCache* SVGExternalDocumentCache::From(LocalFrame& frame) {
  SVGExternalDocumentCache* cache =
      Supplement<LocalFrame>::From<SVGExternalDocumentCache>(frame);
  if (!cache) {
    cache = MakeGarbageCollected<SVGExternalDocumentCache>(frame);
    Supplement<LocalFrame>::ProvideTo(frame, cache);
  }
  return cache;
}

SVGExternalDocumentCache::SVGExternalDocumentCache(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

SVGExternalDocumentCache::CacheKey SVGExternalDocumentCache::MakeCacheKey(
    const FetchParameters& params) {
  const KURL url_without_fragment =
      MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url());
  return {url_without_fragment.GetString(),
          params.GetResourceRequest().GetMode()};
}

SVGResourceDocumentContent* SVGExternalDocumentCache::Get(const CacheKey& key) {
  auto it = entries_.find(key);
  return it != entries_.end() ? it->value : nullptr;
}

void SVGExternalDocumentCache::Put(const CacheKey& key,
                                   SVGResourceDocumentContent* content) {
  entries_.Set(key, content);
}

void SVGExternalDocumentCache::Trace(Visitor* visitor) const {
  Supplement<LocalFrame>::Trace(visitor);
  visitor->Trace(entries_);
}

bool CanReuseContent(const SVGResourceDocumentContent& content) {
  // Don't reuse if loading failed.
  return !content.ErrorOccurred();
}

bool AllowedRequestMode(const ResourceRequest& request) {
  // Same-origin
  if (request.GetMode() == network::mojom::blink::RequestMode::kSameOrigin) {
    return true;
  }
  // CORS with same-origin credentials mode ("CORS anonymous").
  if (request.GetMode() == network::mojom::blink::RequestMode::kCors) {
    return request.GetCredentialsMode() ==
           network::mojom::CredentialsMode::kSameOrigin;
  }
  return false;
}

}  // namespace

SVGResourceDocumentContent::SVGResourceDocumentContent(
    ExecutionContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_(context), task_runner_(std::move(task_runner)) {
  DCHECK(context_);
}

SVGResourceDocumentContent::~SVGResourceDocumentContent() = default;

void SVGResourceDocumentContent::NotifyStartLoad() {
  // Check previous status.
  switch (status_) {
    case ResourceStatus::kPending:
      CHECK(false);
      break;

    case ResourceStatus::kNotStarted:
      // Normal load start.
      break;

    case ResourceStatus::kCached:
    case ResourceStatus::kLoadError:
    case ResourceStatus::kDecodeError:
      // Load start due to revalidation/reload.
      break;
  }
  status_ = ResourceStatus::kPending;
}

void SVGResourceDocumentContent::UpdateStatus(ResourceStatus new_status) {
  switch (new_status) {
    case ResourceStatus::kCached:
    case ResourceStatus::kPending:
      // In case of successful load, Resource's status can be kCached or
      // kPending. Set it to kCached in both cases.
      new_status = ResourceStatus::kCached;
      break;

    case ResourceStatus::kLoadError:
    case ResourceStatus::kDecodeError:
      // In case of error, Resource's status is set to an error status before
      // updating the document and thus we use the error status as-is.
      break;

    case ResourceStatus::kNotStarted:
      CHECK(false);
      break;
  }
  status_ = new_status;
}

void SVGResourceDocumentContent::UpdateDocument(const String& content,
                                                const KURL& request_url) {
  if (content.empty()) {
    return;
  }
  url_ = request_url;
  document_ = XMLDocument::CreateSVG(DocumentInit::Create()
                                         .WithURL(request_url)
                                         .WithExecutionContext(context_)
                                         .WithAgent(*context_->GetAgent()));
  document_->SetContent(content);
}

void SVGResourceDocumentContent::ClearDocument() {
  document_.Clear();
}

Document* SVGResourceDocumentContent::GetDocument() const {
  return document_.Get();
}

const KURL& SVGResourceDocumentContent::Url() const {
  return url_;
}

void SVGResourceDocumentContent::AddObserver(
    SVGResourceDocumentObserver* observer) {
  // We currently don't have any N:1 relations (multiple observer registrations
  // for a single document content) among the existing clients
  // (ExternalSVGResource and SVGUseElement).
  DCHECK(!observers_.Contains(observer));
  observers_.insert(observer);
  if (IsLoaded()) {
    task_runner_->PostTask(
        FROM_HERE,
        WTF::BindOnce(&SVGResourceDocumentContent::NotifyObserver,
                      WrapPersistent(this), WrapWeakPersistent(observer)));
  }
}

void SVGResourceDocumentContent::RemoveObserver(
    SVGResourceDocumentObserver* observer) {
  observers_.erase(observer);
}

void SVGResourceDocumentContent::NotifyObserver(
    SVGResourceDocumentObserver* observer) {
  if (observer && observers_.Contains(observer)) {
    observer->ResourceNotifyFinished(this);
  }
}

void SVGResourceDocumentContent::NotifyObservers() {
  for (auto& observer : observers_) {
    observer->ResourceNotifyFinished(this);
  }
}

bool SVGResourceDocumentContent::IsLoaded() const {
  return status_ > ResourceStatus::kPending;
}

bool SVGResourceDocumentContent::IsLoading() const {
  return status_ == ResourceStatus::kPending;
}

bool SVGResourceDocumentContent::ErrorOccurred() const {
  return status_ == ResourceStatus::kLoadError ||
         status_ == ResourceStatus::kDecodeError;
}

void SVGResourceDocumentContent::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(context_);
  visitor->Trace(observers_);
}

SVGResourceDocumentContent* SVGResourceDocumentContent::Fetch(
    FetchParameters& params,
    Document& document) {
  CHECK(!params.Url().IsNull());
  // Callers need to set the request and credentials mode to something suitably
  // restrictive. This limits the actual modes (simplifies caching) that we
  // allow and avoids accidental creation of overly privileged requests.
  CHECK(AllowedRequestMode(params.GetResourceRequest()));

  DCHECK_EQ(params.GetResourceRequest().GetRequestContext(),
            mojom::blink::RequestContextType::UNSPECIFIED);
  params.SetRequestContext(mojom::blink::RequestContextType::IMAGE);
  params.SetRequestDestination(network::mojom::RequestDestination::kImage);

  auto* cache =
      SVGExternalDocumentCache::From(document.GetFrame()->LocalFrameRoot());

  const SVGExternalDocumentCache::CacheKey key =
      SVGExternalDocumentCache::MakeCacheKey(params);
  auto* cached_content = cache->Get(key);
  if (cached_content && CanReuseContent(*cached_content)) {
    return cached_content;
  }

  SVGDocumentResource* resource = SVGDocumentResource::Fetch(
      params, document.Fetcher(), document.GetExecutionContext());
  if (!resource) {
    return nullptr;
  }
  cache->Put(key, resource->GetContent());
  return resource->GetContent();
}

}  // namespace blink
