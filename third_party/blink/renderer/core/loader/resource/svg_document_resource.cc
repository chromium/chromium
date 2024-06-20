// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/svg_document_resource.h"

#include "third_party/blink/renderer/core/svg/svg_resource_document_content.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"

namespace blink {

namespace {

class SVGDocumentResourceFactory : public ResourceFactory {
 public:
  SVGDocumentResourceFactory(
      AgentGroupScheduler& agent_group_scheduler,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : ResourceFactory(ResourceType::kSVGDocument,
                        TextResourceDecoderOptions::kXMLContent),
        agent_group_scheduler_(agent_group_scheduler),
        task_runner_(std::move(task_runner)) {}

  Resource* Create(
      const ResourceRequest& request,
      const ResourceLoaderOptions& options,
      const TextResourceDecoderOptions& decoder_options) const override {
    auto* content = MakeGarbageCollected<SVGResourceDocumentContent>(
        agent_group_scheduler_, task_runner_);
    return MakeGarbageCollected<SVGDocumentResource>(request, options,
                                                     decoder_options, content);
  }

 private:
  AgentGroupScheduler& agent_group_scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

bool MimeTypeAllowed(const ResourceResponse& response) {
  AtomicString mime_type = response.MimeType();
  if (response.IsHTTP()) {
    mime_type = response.HttpContentType();
  }
  return mime_type == "image/svg+xml" || mime_type == "text/xml" ||
         mime_type == "application/xml" || mime_type == "application/xhtml+xml";
}

}  // namespace

SVGDocumentResource* SVGDocumentResource::Fetch(
    FetchParameters& params,
    ResourceFetcher* fetcher,
    AgentGroupScheduler& agent_group_scheduler) {
  return To<SVGDocumentResource>(fetcher->RequestResource(
      params,
      SVGDocumentResourceFactory(agent_group_scheduler,
                                 agent_group_scheduler.DefaultTaskRunner()),
      nullptr));
}

SVGDocumentResource::SVGDocumentResource(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options,
    const TextResourceDecoderOptions& decoder_options,
    SVGResourceDocumentContent* content)
    : TextResource(request,
                   ResourceType::kSVGDocument,
                   options,
                   decoder_options),
      content_(content) {}

void SVGDocumentResource::NotifyStartLoad() {
  TextResource::NotifyStartLoad();
  CHECK_EQ(GetStatus(), ResourceStatus::kPending);
  content_->NotifyStartLoad();
}

void SVGDocumentResource::Finish(base::TimeTicks load_finish_time,
                                 base::SingleThreadTaskRunner* task_runner) {
  const ResourceResponse& response = GetResponse();
  using UpdateResult = SVGResourceDocumentContent::UpdateResult;
  UpdateResult update_status = UpdateResult::kError;
  if (MimeTypeAllowed(response) && HasData()) {
    update_status =
        content_->UpdateDocument(Data(), response.CurrentRequestUrl());
  }
  switch (update_status) {
    case UpdateResult::kCompleted:
      content_->UpdateStatus(GetStatus());
      break;
    case UpdateResult::kAsync:
      // Document loading asynchronously. Status will be updated when
      // completed.
      break;
    case UpdateResult::kError:
      if (!ErrorOccurred()) {
        SetStatus(ResourceStatus::kDecodeError);
        ClearData();
        content_->UpdateStatus(GetStatus());
      }
      break;
  }
  TextResource::Finish(load_finish_time, task_runner);
  if (update_status != UpdateResult::kAsync) {
    content_->NotifyObservers();
  }
}

void SVGDocumentResource::FinishAsError(
    const ResourceError& error,
    base::SingleThreadTaskRunner* task_runner) {
  TextResource::FinishAsError(error, task_runner);
  content_->ClearDocument();
  content_->UpdateStatus(GetStatus());
  content_->NotifyObservers();
}

void SVGDocumentResource::DestroyDecodedDataForFailedRevalidation() {
  content_->ClearDocument();
}

void SVGDocumentResource::Trace(Visitor* visitor) const {
  visitor->Trace(content_);
  TextResource::Trace(visitor);
}

}  // namespace blink
