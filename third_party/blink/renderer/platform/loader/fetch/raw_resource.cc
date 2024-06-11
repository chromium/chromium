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

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/loader/fetch/buffering_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client_walker.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

RawResource* RawResource::FetchSynchronously(FetchParameters& params,
                                             ResourceFetcher* fetcher,
                                             RawResourceClient* client) {
  params.MakeSynchronous();
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(ResourceType::kRaw), client));
}

RawResource* RawResource::Fetch(FetchParameters& params,
                                ResourceFetcher* fetcher,
                                RawResourceClient* client) {
  DCHECK_NE(params.GetResourceRequest().GetRequestContext(),
            mojom::blink::RequestContextType::UNSPECIFIED);
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(ResourceType::kRaw), client));
}

RawResource* RawResource::FetchMedia(FetchParameters& params,
                                     ResourceFetcher* fetcher,
                                     RawResourceClient* client) {
  auto context = params.GetResourceRequest().GetRequestContext();
  DCHECK(context == mojom::blink::RequestContextType::AUDIO ||
         context == mojom::blink::RequestContextType::VIDEO);
  ResourceType type = (context == mojom::blink::RequestContextType::AUDIO)
                          ? ResourceType::kAudio
                          : ResourceType::kVideo;
  return ToRawResource(
      fetcher->RequestResource(params, RawResourceFactory(type), client));
}

RawResource* RawResource::FetchTextTrack(FetchParameters& params,
                                         ResourceFetcher* fetcher,
                                         RawResourceClient* client) {
  params.SetRequestContext(mojom::blink::RequestContextType::TRACK);
  params.SetRequestDestination(network::mojom::RequestDestination::kTrack);
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(ResourceType::kTextTrack), client));
}

RawResource* RawResource::FetchManifest(FetchParameters& params,
                                        ResourceFetcher* fetcher,
                                        RawResourceClient* client) {
  DCHECK_EQ(params.GetResourceRequest().GetRequestContext(),
            mojom::blink::RequestContextType::MANIFEST);
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(ResourceType::kManifest), client));
}

RawResource::RawResource(const ResourceRequest& resource_request,
                         ResourceType type,
                         const ResourceLoaderOptions& options)
    : Resource(resource_request, type, options) {}

void RawResource::AppendData(
    absl::variant<SegmentedBuffer, base::span<const char>> data) {
  if (GetResourceRequest().UseStreamOnResponse())
    return;

  Resource::AppendData(std::move(data));
}

class RawResource::PreloadBytesConsumerClient final
    : public GarbageCollected<PreloadBytesConsumerClient>,
      public BytesConsumer::Client {
 public:
  PreloadBytesConsumerClient(BytesConsumer& bytes_consumer,
                             RawResource& resource,
                             RawResourceClient& client)
      : bytes_consumer_(bytes_consumer),
        resource_(resource),
        client_(&client) {}
  void OnStateChange() override {
    auto* client = client_.Get();
    if (!client) {
      return;
    }
    while (resource_->HasClient(client)) {
      const char* buffer = nullptr;
      size_t available = 0;
      auto result = bytes_consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        client->DataReceived(resource_,
                             // SAFETY: `BeginRead` must ensure that `buffer`
                             // has `available` bytes available.
                             // TODO(crbug.com/40284755): Capture this invariant
                             // by making it return an actual span.
                             UNSAFE_BUFFERS(base::span(buffer, available)));
        result = bytes_consumer_->EndRead(available);
      }
      if (result != BytesConsumer::Result::kOk) {
        return;
      }
    }
    client_ = nullptr;
  }

  String DebugName() const override { return "PreloadBytesConsumerClient"; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(bytes_consumer_);
    visitor->Trace(resource_);
    visitor->Trace(client_);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  const Member<BytesConsumer> bytes_consumer_;
  const Member<RawResource> resource_;
  WeakMember<RawResourceClient> client_;
};

void RawResource::DidAddClient(ResourceClient* c) {
  auto* bytes_consumer_for_preload = bytes_consumer_for_preload_.Release();

  // CHECK()/RevalidationStartForbiddenScope are for
  // https://crbug.com/640960#c24.
  CHECK(!IsCacheValidator());
  if (!HasClient(c))
    return;
  DCHECK(c->IsRawResourceClient());
  RevalidationStartForbiddenScope revalidation_start_forbidden_scope(this);
  RawResourceClient* client = static_cast<RawResourceClient*>(c);
  for (const auto& redirect : RedirectChain()) {
    client->RedirectReceived(this, ResourceRequest(redirect.request_),
                             redirect.redirect_response_);
    if (!HasClient(c))
      return;
  }

  if (!GetResponse().IsNull()) {
    client->ResponseReceived(this, GetResponse());
  }
  if (!HasClient(c))
    return;

  if (bytes_consumer_for_preload) {
    bytes_consumer_for_preload->StopBuffering();

    if (matched_with_non_streaming_destination_) {
      // In this case, the client needs individual chunks so we need
      // PreloadBytesConsumerClient for the translation.
      auto* preload_bytes_consumer_client =
          MakeGarbageCollected<PreloadBytesConsumerClient>(
              *bytes_consumer_for_preload, *this, *client);
      bytes_consumer_for_preload->SetClient(preload_bytes_consumer_client);
      preload_bytes_consumer_client->OnStateChange();
    } else {
      // In this case, we can simply pass the BytesConsumer to the client.
      client->ResponseBodyReceived(this, *bytes_consumer_for_preload);
    }
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

scoped_refptr<BlobDataHandle> RawResource::DownloadedBlob() const {
  return downloaded_blob_;
}

void RawResource::Trace(Visitor* visitor) const {
  visitor->Trace(bytes_consumer_for_preload_);
  Resource::Trace(visitor);
}

void RawResource::ResponseReceived(const ResourceResponse& response) {
  Resource::ResponseReceived(response);

  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next()) {
    c->ResponseReceived(this, GetResponse());
  }
}

void RawResource::ResponseBodyReceived(
    ResponseBodyLoaderDrainableInterface& body_loader,
    scoped_refptr<base::SingleThreadTaskRunner> loader_task_runner) {
  DCHECK_LE(Clients().size(), 1u);
  RawResourceClient* client =
      ResourceClientWalker<RawResourceClient>(Clients()).Next();
  if (!client && GetResourceRequest().UseStreamOnResponse()) {
    // For preload, we want to store the body while dispatching
    // onload and onerror events.
    bytes_consumer_for_preload_ =
        BufferingBytesConsumer::Create(&body_loader.DrainAsBytesConsumer());
    return;
  }

  if (matched_with_non_streaming_destination_) {
    DCHECK(GetResourceRequest().UseStreamOnResponse());
    // The loading was initiated as a preload (hence UseStreamOnResponse is
    // set), but this resource has been matched with a request without
    // UseStreamOnResponse set.
    auto& bytes_consumer_for_preload = body_loader.DrainAsBytesConsumer();
    auto* preload_bytes_consumer_client =
        MakeGarbageCollected<PreloadBytesConsumerClient>(
            bytes_consumer_for_preload, *this, *client);
    bytes_consumer_for_preload.SetClient(preload_bytes_consumer_client);
    preload_bytes_consumer_client->OnStateChange();
    return;
  }

  if (!GetResourceRequest().UseStreamOnResponse()) {
    return;
  }

  client->ResponseBodyReceived(this, body_loader.DrainAsBytesConsumer());
}

void RawResource::SetSerializedCachedMetadata(mojo_base::BigBuffer data) {
  // Resource ignores the cached metadata.
  Resource::SetSerializedCachedMetadata(mojo_base::BigBuffer());

  ResourceClientWalker<RawResourceClient> w(Clients());
  // We rely on the fact that RawResource cannot have multiple clients.
  CHECK_LE(Clients().size(), 1u);
  if (RawResourceClient* c = w.Next()) {
    c->CachedMetadataReceived(this, std::move(data));
  }
}

void RawResource::DidSendData(uint64_t bytes_sent,
                              uint64_t total_bytes_to_be_sent) {
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->DataSent(this, bytes_sent, total_bytes_to_be_sent);
}

void RawResource::DidDownloadData(uint64_t data_length) {
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

void RawResource::MatchPreload(const FetchParameters& params) {
  Resource::MatchPreload(params);
  matched_with_non_streaming_destination_ =
      !params.GetResourceRequest().UseStreamOnResponse();
}

void RawResourceClient::DidDownloadToBlob(Resource*,
                                          scoped_refptr<BlobDataHandle>) {}

RawResourceClientStateChecker::RawResourceClientStateChecker() = default;

NOINLINE void RawResourceClientStateChecker::WillAddClient() {
  SECURITY_CHECK(state_ == kNotAddedAsClient);
  state_ = kStarted;
}

NOINLINE void RawResourceClientStateChecker::WillRemoveClient() {
  SECURITY_CHECK(state_ != kNotAddedAsClient);
  SECURITY_CHECK(state_ != kDetached);
  state_ = kDetached;
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
  // TODO(crbug.com/1431421): remove |state_| dump when the cause is clarified.
  SECURITY_CHECK(state_ == kStarted) << " state_ was " << state_;
  state_ = kResponseReceived;
}

NOINLINE void RawResourceClientStateChecker::SetSerializedCachedMetadata() {
  SECURITY_CHECK(state_ == kStarted || state_ == kResponseReceived ||
                 state_ == kDataReceivedAsBytesConsumer ||
                 state_ == kDataReceived);
}

NOINLINE void RawResourceClientStateChecker::ResponseBodyReceived() {
  SECURITY_CHECK(state_ == kResponseReceived);
  state_ = kDataReceivedAsBytesConsumer;
}

NOINLINE void RawResourceClientStateChecker::DataReceived() {
  SECURITY_CHECK(state_ == kResponseReceived ||
                 state_ == kDataReceived);
  state_ = kDataReceived;
}

NOINLINE void RawResourceClientStateChecker::DataDownloaded() {
  SECURITY_CHECK(state_ == kResponseReceived ||
                 state_ == kDataDownloaded);
  state_ = kDataDownloaded;
}

NOINLINE void RawResourceClientStateChecker::DidDownloadToBlob() {
  SECURITY_CHECK(state_ == kResponseReceived ||
                 state_ == kDataDownloaded);
  state_ = kDidDownloadToBlob;
}

NOINLINE void RawResourceClientStateChecker::NotifyFinished(
    Resource* resource) {
  SECURITY_CHECK(state_ != kNotAddedAsClient);
  SECURITY_CHECK(state_ != kDetached);
  SECURITY_CHECK(state_ != kNotifyFinished);

  SECURITY_CHECK(resource->ErrorOccurred() ||
                 (state_ == kResponseReceived || state_ == kDataReceived ||
                  state_ == kDataDownloaded ||
                  state_ == kDataReceivedAsBytesConsumer ||
                  state_ == kDidDownloadToBlob));
  state_ = kNotifyFinished;
}

}  // namespace blink
