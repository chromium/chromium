/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"

#include <memory>
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client_walker.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/source_keyed_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

RawResource* RawResource::FetchSynchronously(FetchParameters& params,
                                             ResourceFetcher* fetcher,
                                             RawResourceClient* client) {
  params.MakeSynchronous();
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(ResourceType::kRaw), client));
}

RawResource* RawResource::FetchImport(FetchParameters& params,
                                      ResourceFetcher* fetcher,
                                      RawResourceClient* client) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            network::mojom::RequestContextFrameType::kNone);
  params.SetRequestContext(mojom::RequestContextType::IMPORT);
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(ResourceType::kImportResource), client));
}

RawResource* RawResource::Fetch(FetchParameters& params,
                                ResourceFetcher* fetcher,
                                RawResourceClient* client) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            network::mojom::RequestContextFrameType::kNone);
  DCHECK_NE(params.GetResourceRequest().GetRequestContext(),
            mojom::RequestContextType::UNSPECIFIED);
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(ResourceType::kRaw), client));
}

RawResource* RawResource::FetchMainResource(
    FetchParameters& params,
    ResourceFetcher* fetcher,
    RawResourceClient* client,
    const SubstituteData& substitute_data) {
  DCHECK_NE(params.GetResourceRequest().GetFrameType(),
            network::mojom::RequestContextFrameType::kNone);
  DCHECK(params.GetResourceRequest().GetRequestContext() ==
             mojom::RequestContextType::FORM ||
         params.GetResourceRequest().GetRequestContext() ==
             mojom::RequestContextType::FRAME ||
         params.GetResourceRequest().GetRequestContext() ==
             mojom::RequestContextType::HYPERLINK ||
         params.GetResourceRequest().GetRequestContext() ==
             mojom::RequestContextType::IFRAME ||
         params.GetResourceRequest().GetRequestContext() ==
             mojom::RequestContextType::INTERNAL ||
         params.GetResourceRequest().GetRequestContext() ==
             mojom::RequestContextType::LOCATION);

  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(ResourceType::kMainResource), client,
      substitute_data));
}

RawResource* RawResource::FetchMedia(FetchParameters& params,
                                     ResourceFetcher* fetcher,
                                     RawResourceClient* client) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            network::mojom::RequestContextFrameType::kNone);
  auto context = params.GetResourceRequest().GetRequestContext();
  DCHECK(context == mojom::RequestContextType::AUDIO ||
         context == mojom::RequestContextType::VIDEO);
  ResourceType type = (context == mojom::RequestContextType::AUDIO)
                          ? ResourceType::kAudio
                          : ResourceType::kVideo;
  return ToRawResource(
      fetcher->RequestResource(params, RawResourceFactory(type), client));
}

RawResource* RawResource::FetchTextTrack(FetchParameters& params,
                                         ResourceFetcher* fetcher,
                                         RawResourceClient* client) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            network::mojom::RequestContextFrameType::kNone);
  params.SetRequestContext(mojom::RequestContextType::TRACK);
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(ResourceType::kTextTrack), client));
}

RawResource* RawResource::FetchManifest(FetchParameters& params,
                                        ResourceFetcher* fetcher,
                                        RawResourceClient* client) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            network::mojom::RequestContextFrameType::kNone);
  DCHECK_EQ(params.GetResourceRequest().GetRequestContext(),
            mojom::RequestContextType::MANIFEST);
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(ResourceType::kManifest), client));
}

RawResource::RawResource(const ResourceRequest& resource_request,
                         ResourceType type,
                         const ResourceLoaderOptions& options)
    : Resource(resource_request, type, options) {}

void RawResource::AppendData(const char* data, size_t length) {
  if (data_pipe_writer_) {
    DCHECK_EQ(kDoNotBufferData, GetDataBufferingPolicy());
    data_pipe_writer_->Write(data, length);
  } else {
    Resource::AppendData(data, length);
  }
}

void RawResource::DidAddClient(ResourceClient* c) {
  // CHECK()/RevalidationStartForbiddenScope are for
  // https://crbug.com/640960#c24.
  CHECK(!IsCacheValidator());
  if (!HasClient(c))
    return;
  DCHECK(c->IsRawResourceClient());
  RevalidationStartForbiddenScope revalidation_start_forbidden_scope(this);
  RawResourceClient* client = static_cast<RawResourceClient*>(c);
  for (const auto& redirect : RedirectChain()) {
    ResourceRequest request(redirect.request_);
    client->RedirectReceived(this, request, redirect.redirect_response_);
    if (!HasClient(c))
      return;
  }

  if (!GetResponse().IsNull()) {
    client->ResponseReceived(this, GetResponse(),
                             std::move(data_consumer_handle_));
  }
  if (!HasClient(c))
    return;
  Resource::DidAddClient(client);
}

bool RawResource::WillFollowRedirect(
    const ResourceRequest& new_request,
    const ResourceResponse& redirect_response) {
  bool follow = Resource::WillFollowRedirect(new_request, redirect_response);
  // The base class method takes a const reference of a ResourceRequest and
  // returns bool just for allowing RawResource to reject redirect. It must
  // always return true.
  DCHECK(follow);

  DCHECK(!redirect_response.IsNull());
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next()) {
    if (!c->RedirectReceived(this, new_request, redirect_response))
      follow = false;
  }

  return follow;
}

void RawResource::WillNotFollowRedirect() {
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->RedirectBlocked();
}

SourceKeyedCachedMetadataHandler* RawResource::InlineScriptCacheHandler() {
  DCHECK_EQ(ResourceType::kMainResource, GetType());
  return static_cast<SourceKeyedCachedMetadataHandler*>(
      Resource::CacheHandler());
}

SingleCachedMetadataHandler* RawResource::ScriptCacheHandler() {
  DCHECK_EQ(ResourceType::kRaw, GetType());
  return static_cast<SingleCachedMetadataHandler*>(Resource::CacheHandler());
}

void RawResource::ResponseReceived(
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  if (response.WasFallbackRequiredByServiceWorker()) {
    // The ServiceWorker asked us to re-fetch the request. This resource must
    // not be reused.
    // Note: This logic is needed here because ThreadableLoader handles
    // CORS independently from ResourceLoader. Fix it.
    if (IsMainThread())
      GetMemoryCache()->Remove(this);
  }

  Resource::ResponseReceived(response, nullptr);

  DCHECK(!handle || !data_consumer_handle_);
  if (!handle && Clients().size() > 0)
    handle = std::move(data_consumer_handle_);
  ResourceClientWalker<RawResourceClient> w(Clients());
  DCHECK(Clients().size() <= 1 || !handle);
  while (RawResourceClient* c = w.Next()) {
    // |handle| is cleared when passed, but it's not a problem because |handle|
    // is null when there are two or more clients, as asserted.
    c->ResponseReceived(this, this->GetResponse(), std::move(handle));
  }
}

CachedMetadataHandler* RawResource::CreateCachedMetadataHandler(
    std::unique_ptr<CachedMetadataSender> send_callback) {
  if (GetType() == ResourceType::kMainResource) {
    // This is a document resource; create a cache handler that can handle
    // multiple inline scripts.
    return new SourceKeyedCachedMetadataHandler(Encoding(),
                                                std::move(send_callback));
  } else if (GetType() == ResourceType::kRaw) {
    // This is a resource of indeterminate type, e.g. a fetched WebAssembly
    // module; create a cache handler that can store a single metadata entry.
    return new ScriptCachedMetadataHandler(Encoding(),
                                           std::move(send_callback));
  }
  return Resource::CreateCachedMetadataHandler(std::move(send_callback));
}

void RawResource::SetSerializedCachedMetadata(const char* data, size_t size) {
  Resource::SetSerializedCachedMetadata(data, size);

  if (GetType() == ResourceType::kMainResource) {
    SourceKeyedCachedMetadataHandler* cache_handler =
        InlineScriptCacheHandler();
    if (cache_handler) {
      cache_handler->SetSerializedCachedMetadata(data, size);
    }
  } else if (GetType() == ResourceType::kRaw) {
    ScriptCachedMetadataHandler* cache_handler =
        static_cast<ScriptCachedMetadataHandler*>(Resource::CacheHandler());
    if (cache_handler) {
      cache_handler->SetSerializedCachedMetadata(data, size);
    }
  }

  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->SetSerializedCachedMetadata(this, data, size);
}

void RawResource::DidSendData(unsigned long long bytes_sent,
                              unsigned long long total_bytes_to_be_sent) {
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->DataSent(this, bytes_sent, total_bytes_to_be_sent);
}

void RawResource::DidDownloadData(int data_length) {
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->DataDownloaded(this, data_length);
}

void RawResource::DidDownloadToBlob(scoped_refptr<BlobDataHandle> blob) {
  downloaded_blob_ = blob;
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->DidDownloadToBlob(this, blob);
}

void RawResource::ReportResourceTimingToClients(
    const ResourceTimingInfo& info) {
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->DidReceiveResourceTiming(this, info);
}

bool RawResource::MatchPreload(const FetchParameters& params,
                               base::SingleThreadTaskRunner* task_runner) {
  if (!Resource::MatchPreload(params, task_runner))
    return false;

  // This is needed to call Platform::Current() below. Remove this branch
  // when the calls are removed.
  if (!IsMainThread())
    return false;

  if (!params.GetResourceRequest().UseStreamOnResponse())
    return true;

  if (ErrorOccurred())
    return true;

  // A preloaded resource is not for streaming.
  DCHECK(!GetResourceRequest().UseStreamOnResponse());
  DCHECK_EQ(GetDataBufferingPolicy(), kBufferData);

  // Preloading for raw resources are not cached.
  DCHECK(!IsMainThread() || !GetMemoryCache()->Contains(this));

  constexpr auto kCapacity = 32 * 1024;
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = kCapacity;

  MojoResult result = mojo::CreateDataPipe(&options, &producer, &consumer);
  if (result != MOJO_RESULT_OK)
    return false;

  data_consumer_handle_ =
      Platform::Current()->CreateDataConsumerHandle(std::move(consumer));
  data_pipe_writer_ = std::make_unique<BufferingDataPipeWriter>(
      std::move(producer), task_runner);

  if (Data()) {
    for (const auto& span : *Data())
      data_pipe_writer_->Write(span.data(), span.size());
  }
  SetDataBufferingPolicy(kDoNotBufferData);

  if (IsLoaded())
    data_pipe_writer_->Finish();
  return true;
}

void RawResource::NotifyFinished() {
  if (data_pipe_writer_)
    data_pipe_writer_->Finish();
  Resource::NotifyFinished();
}

static bool ShouldIgnoreHeaderForCacheReuse(AtomicString header_name) {
  // FIXME: This list of headers that don't affect cache policy almost certainly
  // isn't complete.
  DEFINE_STATIC_LOCAL(
      HashSet<AtomicString>, headers,
      ({"Cache-Control", "If-Modified-Since", "If-None-Match", "Origin",
        "Pragma", "Purpose", "Referer", "User-Agent"}));
  return headers.Contains(header_name);
}

Resource::MatchStatus RawResource::CanReuse(
    const FetchParameters& new_fetch_parameters) const {
  const ResourceRequest& new_request =
      new_fetch_parameters.GetResourceRequest();
  // Ensure most headers match the existing headers before continuing. Note that
  // the list of ignored headers includes some headers explicitly related to
  // caching. A more detailed check of caching policy will be performed later,
  // this is simply a list of headers that we might permit to be different and
  // still reuse the existing Resource.
  const HTTPHeaderMap& new_headers = new_request.HttpHeaderFields();
  const HTTPHeaderMap& old_headers = GetResourceRequest().HttpHeaderFields();

  for (const auto& header : new_headers) {
    AtomicString header_name = header.key;
    if (!ShouldIgnoreHeaderForCacheReuse(header_name) &&
        header.value != old_headers.Get(header_name)) {
      return MatchStatus::kRequestHeadersDoNotMatch;
    }
  }

  for (const auto& header : old_headers) {
    AtomicString header_name = header.key;
    if (!ShouldIgnoreHeaderForCacheReuse(header_name) &&
        header.value != new_headers.Get(header_name)) {
      return MatchStatus::kRequestHeadersDoNotMatch;
    }
  }

  return Resource::CanReuse(new_fetch_parameters);
}

RawResourceClientStateChecker::RawResourceClientStateChecker()
    : state_(kNotAddedAsClient) {}

RawResourceClientStateChecker::~RawResourceClientStateChecker() = default;

NOINLINE void RawResourceClientStateChecker::WillAddClient() {
  SECURITY_CHECK(state_ == kNotAddedAsClient);
  state_ = kStarted;
}

NOINLINE void RawResourceClientStateChecker::WillRemoveClient() {
  SECURITY_CHECK(state_ != kNotAddedAsClient);
  state_ = kNotAddedAsClient;
}

NOINLINE void RawResourceClientStateChecker::RedirectReceived() {
  SECURITY_CHECK(state_ == kStarted);
}

NOINLINE void RawResourceClientStateChecker::RedirectBlocked() {
  SECURITY_CHECK(state_ == kStarted);
  state_ = kRedirectBlocked;
}

NOINLINE void RawResourceClientStateChecker::DataSent() {
  SECURITY_CHECK(state_ == kStarted);
}

NOINLINE void RawResourceClientStateChecker::ResponseReceived() {
  SECURITY_CHECK(state_ == kStarted);
  state_ = kResponseReceived;
}

NOINLINE void RawResourceClientStateChecker::SetSerializedCachedMetadata() {
  SECURITY_CHECK(state_ == kResponseReceived);
  state_ = kSetSerializedCachedMetadata;
}

NOINLINE void RawResourceClientStateChecker::DataReceived() {
  SECURITY_CHECK(state_ == kResponseReceived ||
                 state_ == kSetSerializedCachedMetadata ||
                 state_ == kDataReceived);
  state_ = kDataReceived;
}

NOINLINE void RawResourceClientStateChecker::DataDownloaded() {
  SECURITY_CHECK(state_ == kResponseReceived ||
                 state_ == kSetSerializedCachedMetadata ||
                 state_ == kDataDownloaded);
  state_ = kDataDownloaded;
}

NOINLINE void RawResourceClientStateChecker::DidDownloadToBlob() {
  SECURITY_CHECK(state_ == kResponseReceived ||
                 state_ == kSetSerializedCachedMetadata ||
                 state_ == kDataDownloaded);
  state_ = kDidDownloadToBlob;
}

NOINLINE void RawResourceClientStateChecker::NotifyFinished(
    Resource* resource) {
  SECURITY_CHECK(state_ != kNotAddedAsClient);
  SECURITY_CHECK(state_ != kNotifyFinished);
  SECURITY_CHECK(resource->ErrorOccurred() ||
                 (state_ == kResponseReceived ||
                  state_ == kSetSerializedCachedMetadata ||
                  state_ == kDataReceived || state_ == kDataDownloaded ||
                  state_ == kDidDownloadToBlob));
  state_ = kNotifyFinished;
}

}  // namespace blink
