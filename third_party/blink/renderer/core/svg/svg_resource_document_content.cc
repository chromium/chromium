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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/resource/svg_document_resource.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/svg/graphics/isolated_svg_document_host.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_resource_document_cache.h"
#include "third_party/blink/renderer/core/svg/svg_resource_document_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

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

class SVGResourceDocumentContent::ChromeClient final
    : public IsolatedSVGChromeClient {
 public:
  explicit ChromeClient(SVGResourceDocumentContent* content)
      : content_(content) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(content_);
    IsolatedSVGChromeClient::Trace(visitor);
  }

 private:
  void ChromeDestroyed() override { content_.Clear(); }
  void InvalidateContainer() override { content_->ContentChanged(); }
  void ScheduleAnimation(const LocalFrameView*, base::TimeDelta) override {
    content_->ContentChanged();
  }

  Member<SVGResourceDocumentContent> content_;
};

SVGResourceDocumentContent::SVGResourceDocumentContent(
    AgentGroupScheduler& agent_group_scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : agent_group_scheduler_(agent_group_scheduler),
      task_runner_(std::move(task_runner)) {}

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

SVGResourceDocumentContent::UpdateResult
SVGResourceDocumentContent::UpdateDocument(scoped_refptr<SharedBuffer> data,
                                           const KURL& request_url) {
  if (data->empty() || was_disposed_) {
    return UpdateResult::kError;
  }
  CHECK(!document_host_);
  auto* chrome_client = MakeGarbageCollected<ChromeClient>(this);
  document_host_ = MakeGarbageCollected<IsolatedSVGDocumentHost>(
      *chrome_client, *agent_group_scheduler_);
  document_host_->InstallDocument(
      std::move(data),
      WTF::BindOnce(&SVGResourceDocumentContent::AsyncLoadingFinished,
                    WrapWeakPersistent(this)),
      nullptr, IsolatedSVGDocumentHost::ProcessingMode::kStatic);
  // If IsLoaded() returns true then the document load completed synchronously,
  // so we can check if we have a usable document and notify our listeners. If
  // not, then we need to wait for the async load completion callback.
  if (!document_host_->IsLoaded()) {
    return UpdateResult::kAsync;
  }
  LoadingFinished();
  return UpdateResult::kCompleted;
}

void SVGResourceDocumentContent::LoadingFinished() {
  LocalFrame* frame = document_host_->GetFrame();
  frame->View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kSVGImage);
  UpdateStatus(ResourceStatus::kCached);
}

void SVGResourceDocumentContent::AsyncLoadingFinished() {
  LoadingFinished();
  NotifyObservers();
}

void SVGResourceDocumentContent::Dispose() {
  ClearDocument();
  was_disposed_ = true;
}

void SVGResourceDocumentContent::ClearDocument() {
  if (!document_host_) {
    return;
  }
  auto* document_host = document_host_.Release();
  document_host->Shutdown();
}

Document* SVGResourceDocumentContent::GetDocument() const {
  // Only return a Document if the load sequence fully completed.
  if (document_host_ && document_host_->IsLoaded()) {
    return document_host_->GetFrame()->GetDocument();
  }
  return nullptr;
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

SVGResourceTarget* SVGResourceDocumentContent::GetResourceTarget(
    const AtomicString& element_id) {
  Document* document = GetDocument();
  if (!document) {
    return nullptr;
  }
  auto* svg_target =
      DynamicTo<SVGElement>(document->getElementById(element_id));
  if (!svg_target) {
    return nullptr;
  }
  return &svg_target->EnsureResourceTarget();
}

void SVGResourceDocumentContent::ContentChanged() {
  for (auto& observer : observers_) {
    observer->ResourceContentChanged(this);
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
  visitor->Trace(document_host_);
  visitor->Trace(agent_group_scheduler_);
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

  Page* page = document.GetPage();
  auto& cache = page->GetSVGResourceDocumentCache();

  const SVGResourceDocumentCache::CacheKey key =
      SVGResourceDocumentCache::MakeCacheKey(params);
  auto* cached_content = cache.Get(key);
  if (cached_content && CanReuseContent(*cached_content)) {
    return cached_content;
  }

  SVGDocumentResource* resource = SVGDocumentResource::Fetch(
      params, document.Fetcher(), page->GetAgentGroupScheduler());
  if (!resource) {
    return nullptr;
  }
  cache.Put(key, resource->GetContent());
  return resource->GetContent();
}

}  // namespace blink
