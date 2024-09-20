// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource_load_observer_for_worker.h"

#include "services/network/public/cpp/ip_address_space_util.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/mixed_content.h"

namespace blink {

ResourceLoadObserverForWorker::ResourceLoadObserverForWorker(
    CoreProbeSink& probe,
    const ResourceFetcherProperties& properties,
    WorkerFetchContext& worker_fetch_context,
    const base::UnguessableToken& devtools_worker_token)
    : probe_(probe),
      fetcher_properties_(properties),
      worker_fetch_context_(worker_fetch_context),
      devtools_worker_token_(devtools_worker_token) {}

ResourceLoadObserverForWorker::~ResourceLoadObserverForWorker() = default;

void ResourceLoadObserverForWorker::DidStartRequest(const FetchParameters&,
                                                    ResourceType) {}

void ResourceLoadObserverForWorker::WillSendRequest(
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    ResourceType resource_type,
    const ResourceLoaderOptions& options,
    RenderBlockingBehavior render_blocking_behavior,
    const Resource* resource) {
  probe::WillSendRequest(
      worker_fetch_context_->GetExecutionContext(), nullptr,
      fetcher_properties_->GetFetchClientSettingsObject().GlobalObjectUrl(),
      request, redirect_response, options, resource_type,
      render_blocking_behavior, base::TimeTicks::Now());
}

void ResourceLoadObserverForWorker::DidChangePriority(
    uint64_t identifier,
    ResourceLoadPriority priority,
    int intra_priority_value) {}

// Record use counter for private network access.
void RecordPrivateNetworkAccessFeature(ExecutionContext* execution_context,
                                       const ResourceResponse& response) {
  DCHECK(execution_context);

  if (response.RemoteIPEndpoint().address().IsZero()) {
    execution_context->CountUse(WebFeature::kPrivateNetworkAccessNullIpAddress);
  }

  if (!network::IsLessPublicAddressSpace(response.AddressSpace(),
                                         response.ClientAddressSpace()))
    return;
  // Only record the feature for worker contexts, not worklets. The address
  // space of worklets is not yet specified.
  // TODO(https://crbug.com/1291176): Revisit this if worklets should be subject
  // to PNA checks.
  if (!execution_context->IsWorkerGlobalScope())
    return;
  execution_context->CountUse(WebFeature::kPrivateNetworkAccessWithinWorker);
}

void ResourceLoadObserverForWorker::DidReceiveResponse(
    uint64_t identifier,
    const ResourceRequest& request,
    const ResourceResponse& response,
    const Resource* resource,
    ResponseSource) {
  RecordPrivateNetworkAccessFeature(
      worker_fetch_context_->GetExecutionContext(), response);

  if (response.HasMajorCertificateErrors()) {
    MixedContentChecker::HandleCertificateError(
        response, request.GetRequestContext(),
        MixedContent::CheckModeForPlugin::kLax,
        worker_fetch_context_->GetContentSecurityNotifier());
  }
  probe::DidReceiveResourceResponse(probe_, identifier, nullptr, response,
                                    resource);
}

void ResourceLoadObserverForWorker::DidReceiveData(
    uint64_t identifier,
    base::SpanOrSize<const char> chunk) {
  probe::DidReceiveData(probe_, identifier, nullptr, chunk);
}

void ResourceLoadObserverForWorker::DidReceiveTransferSizeUpdate(
    uint64_t identifier,
    int transfer_size_diff) {
  DCHECK_GT(transfer_size_diff, 0);
  probe::DidReceiveEncodedDataLength(probe_, nullptr, identifier,
                                     transfer_size_diff);
}

void ResourceLoadObserverForWorker::DidDownloadToBlob(uint64_t identifier,
                                                      BlobDataHandle* blob) {}

void ResourceLoadObserverForWorker::DidFinishLoading(
    uint64_t identifier,
    base::TimeTicks finish_time,
    int64_t encoded_data_length,
    int64_t decoded_body_length) {
  probe::DidFinishLoading(probe_, identifier, nullptr, finish_time,
                          encoded_data_length, decoded_body_length);
}

void ResourceLoadObserverForWorker::DidFailLoading(const KURL&,
                                                   uint64_t identifier,
                                                   const ResourceError& error,
                                                   int64_t,
                                                   IsInternalRequest) {
  probe::DidFailLoading(probe_, identifier, nullptr, error,
                        devtools_worker_token_);
}

bool ResourceLoadObserverForWorker::InterestedInAllRequests() {
  if (probe_) {
    return probe_->HasInspectorNetworkAgents();
  }
  return false;
}

void ResourceLoadObserverForWorker::Trace(Visitor* visitor) const {
  visitor->Trace(probe_);
  visitor->Trace(fetcher_properties_);
  visitor->Trace(worker_fetch_context_);
  ResourceLoadObserver::Trace(visitor);
}

}  // namespace blink
