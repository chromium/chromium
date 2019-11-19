/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/network_resources_data.h"

#include <memory>
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

static bool IsHTTPErrorStatusCode(int status_code) {
  return status_code >= 400;
}

// static
XHRReplayData* XHRReplayData::Create(ExecutionContext* execution_context,
                                     const AtomicString& method,
                                     const KURL& url,
                                     bool async,
                                     scoped_refptr<EncodedFormData> form_data,
                                     bool include_credentials) {
  return MakeGarbageCollected<XHRReplayData>(execution_context, method, url,
                                             async, std::move(form_data),
                                             include_credentials);
}

void XHRReplayData::AddHeader(const AtomicString& key,
                              const AtomicString& value) {
  headers_.Set(key, value);
}

XHRReplayData::XHRReplayData(ExecutionContext* execution_context,
                             const AtomicString& method,
                             const KURL& url,
                             bool async,
                             scoped_refptr<EncodedFormData> form_data,
                             bool include_credentials)
    : execution_context_(execution_context),
      method_(method),
      url_(url),
      async_(async),
      form_data_(form_data),
      include_credentials_(include_credentials) {}

// ResourceData
NetworkResourcesData::ResourceData::ResourceData(
    NetworkResourcesData* network_resources_data,
    const String& request_id,
    const String& loader_id,
    const KURL& requested_url)
    : network_resources_data_(network_resources_data),
      request_id_(request_id),
      loader_id_(loader_id),
      requested_url_(requested_url),
      base64_encoded_(false),
      is_content_evicted_(false),
      type_(InspectorPageAgent::kOtherResource),
      http_status_code_(0),
      raw_header_size_(0),
      pending_encoded_data_length_(0),
      cached_resource_(nullptr) {}

void NetworkResourcesData::ResourceData::Trace(blink::Visitor* visitor) {
  visitor->Trace(network_resources_data_);
  visitor->Trace(xhr_replay_data_);
  visitor->template RegisterWeakCallbackMethod<
      NetworkResourcesData::ResourceData,
      &NetworkResourcesData::ResourceData::ProcessCustomWeakness>(this);
}

void NetworkResourcesData::ResourceData::SetContent(const String& content,
                                                    bool base64_encoded) {
  DCHECK(!HasData());
  DCHECK(!HasContent());
  content_ = content;
  base64_encoded_ = base64_encoded;
}

size_t NetworkResourcesData::ResourceData::RemoveContent() {
  size_t result = 0;
  if (HasData()) {
    DCHECK(!HasContent());
    result = data_buffer_->size();
    data_buffer_ = nullptr;
  }

  if (HasContent()) {
    DCHECK(!HasData());
    result = content_.CharactersSizeInBytes();
    content_ = String();
  }

  if (post_data_ && post_data_->SizeInBytes()) {
    result += post_data_->SizeInBytes();
    post_data_ = nullptr;
  }

  if (xhr_replay_data_ && xhr_replay_data_->FormData()) {
    result += xhr_replay_data_->FormData()->SizeInBytes();
    xhr_replay_data_->DeleteFormData();
  }

  return result;
}

size_t NetworkResourcesData::ResourceData::EvictContent() {
  is_content_evicted_ = true;
  return RemoveContent();
}

void NetworkResourcesData::ResourceData::SetResource(
    const Resource* cached_resource) {
  cached_resource_ = cached_resource;
}

void NetworkResourcesData::ResourceData::ProcessCustomWeakness(
    const WeakCallbackInfo& info) {
  if (!cached_resource_ || info.IsHeapObjectAlive(cached_resource_))
    return;

  // Mark loaded resources or resources without the buffer as loaded.
  if (cached_resource_->IsLoaded() || !cached_resource_->ResourceBuffer()) {
    if (!IsHTTPErrorStatusCode(
            cached_resource_->GetResponse().HttpStatusCode())) {
      String content;
      bool base64_encoded;
      if (InspectorPageAgent::CachedResourceContent(cached_resource_, &content,
                                                    &base64_encoded))
        network_resources_data_->SetResourceContent(RequestId(), content,
                                                    base64_encoded);
    }
  } else {
    // We could be evicting resource being loaded, save the loaded part, the
    // rest will be appended.
    network_resources_data_->MaybeAddResourceData(
        RequestId(), cached_resource_->ResourceBuffer());
  }
  cached_resource_ = nullptr;
}

uint64_t NetworkResourcesData::ResourceData::DataLength() const {
  uint64_t data_length = 0;
  if (data_buffer_)
    data_length += data_buffer_->size();

  if (post_data_)
    data_length += post_data_->SizeInBytes();

  if (xhr_replay_data_ && xhr_replay_data_->FormData())
    data_length += xhr_replay_data_->FormData()->SizeInBytes();

  return data_length;
}

void NetworkResourcesData::ResourceData::AppendData(const char* data,
                                                    size_t data_length) {
  DCHECK(!HasContent());
  if (!data_buffer_)
    data_buffer_ = SharedBuffer::Create(data, data_length);
  else
    data_buffer_->Append(data, data_length);
}

size_t NetworkResourcesData::ResourceData::DecodeDataToContent() {
  DCHECK(!HasContent());
  DCHECK(HasData());
  size_t data_length = data_buffer_->size();
  bool success = InspectorPageAgent::SharedBufferContent(
      data_buffer_, mime_type_, text_encoding_name_, &content_,
      &base64_encoded_);
  DCHECK(success);
  data_buffer_ = nullptr;
  return content_.CharactersSizeInBytes() - data_length;
}

// NetworkResourcesData
NetworkResourcesData::NetworkResourcesData(size_t total_buffer_size,
                                           size_t resource_buffer_size)
    : content_size_(0),
      maximum_resources_content_size_(total_buffer_size),
      maximum_single_resource_content_size_(resource_buffer_size) {}

NetworkResourcesData::~NetworkResourcesData() = default;

void NetworkResourcesData::Trace(blink::Visitor* visitor) {
  visitor->Trace(request_id_to_resource_data_map_);
}

void NetworkResourcesData::ResourceCreated(
    const String& request_id,
    const String& loader_id,
    const KURL& requested_url,
    scoped_refptr<EncodedFormData> post_data) {
  EnsureNoDataForRequestId(request_id);
  ResourceData* data = MakeGarbageCollected<ResourceData>(
      this, request_id, loader_id, requested_url);
  request_id_to_resource_data_map_.Set(request_id, data);
  if (post_data &&
      PrepareToAddResourceData(request_id, post_data->SizeInBytes())) {
    data->SetPostData(post_data);
  }
}

void NetworkResourcesData::ResponseReceived(const String& request_id,
                                            const String& frame_id,
                                            const ResourceResponse& response) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return;
  resource_data->SetFrameId(frame_id);
  resource_data->SetMimeType(response.MimeType());
  resource_data->SetTextEncodingName(response.TextEncodingName());
  resource_data->SetHTTPStatusCode(response.HttpStatusCode());
  resource_data->SetRawHeaderSize(response.EncodedDataLength());
}

void NetworkResourcesData::BlobReceived(const String& request_id,
                                        scoped_refptr<BlobDataHandle> blob) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return;
  resource_data->SetDownloadedFileBlob(std::move(blob));
}

void NetworkResourcesData::SetResourceType(
    const String& request_id,
    InspectorPageAgent::ResourceType type) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return;
  resource_data->SetType(type);
}

InspectorPageAgent::ResourceType NetworkResourcesData::GetResourceType(
    const String& request_id) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return InspectorPageAgent::kOtherResource;
  return resource_data->GetType();
}

void NetworkResourcesData::SetResourceContent(const String& request_id,
                                              const String& content,
                                              bool base64_encoded) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return;
  size_t data_length = content.CharactersSizeInBytes();
  if (data_length > maximum_single_resource_content_size_)
    return;
  if (resource_data->IsContentEvicted())
    return;
  if (EnsureFreeSpace(data_length) && !resource_data->IsContentEvicted()) {
    // We can not be sure that we didn't try to save this request data while it
    // was loading, so remove it, if any.
    if (resource_data->HasContent())
      content_size_ -= resource_data->RemoveContent();
    request_ids_deque_.push_back(request_id);
    resource_data->SetContent(content, base64_encoded);
    content_size_ += data_length;
  }
}

NetworkResourcesData::ResourceData*
NetworkResourcesData::PrepareToAddResourceData(const String& request_id,
                                               uint64_t data_length) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return nullptr;

  if (resource_data->DataLength() + data_length >
      maximum_single_resource_content_size_)
    content_size_ -= resource_data->EvictContent();
  if (resource_data->IsContentEvicted())
    return nullptr;
  if (!EnsureFreeSpace(data_length) || resource_data->IsContentEvicted())
    return nullptr;

  request_ids_deque_.push_back(request_id);
  content_size_ += data_length;

  return resource_data;
}

void NetworkResourcesData::MaybeAddResourceData(const String& request_id,
                                                const char* data,
                                                uint64_t data_length) {
  if (ResourceData* resource_data =
          PrepareToAddResourceData(request_id, data_length)) {
    resource_data->AppendData(data, SafeCast<size_t>(data_length));
  }
}

void NetworkResourcesData::MaybeAddResourceData(
    const String& request_id,
    scoped_refptr<const SharedBuffer> data) {
  DCHECK(data);
  if (ResourceData* resource_data =
          PrepareToAddResourceData(request_id, data->size())) {
    for (const auto& span : *data)
      resource_data->AppendData(span.data(), span.size());
  }
}

void NetworkResourcesData::MaybeDecodeDataToContent(const String& request_id) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return;
  if (!resource_data->HasData())
    return;
  content_size_ += resource_data->DecodeDataToContent();
  size_t data_length = resource_data->Content().CharactersSizeInBytes();
  if (data_length > maximum_single_resource_content_size_)
    content_size_ -= resource_data->EvictContent();
}

void NetworkResourcesData::AddResource(const String& request_id,
                                       const Resource* cached_resource) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return;
  resource_data->SetResource(cached_resource);
}

NetworkResourcesData::ResourceData const* NetworkResourcesData::Data(
    const String& request_id) {
  return ResourceDataForRequestId(request_id);
}

XHRReplayData* NetworkResourcesData::XhrReplayData(const String& request_id) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return nullptr;
  return resource_data->XhrReplayData();
}

void NetworkResourcesData::SetCertificate(
    const String& request_id,
    const Vector<AtomicString>& certificate) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return;
  resource_data->SetCertificate(certificate);
}

void NetworkResourcesData::SetXHRReplayData(const String& request_id,
                                            XHRReplayData* xhr_replay_data) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data || resource_data->IsContentEvicted())
    return;

  if (xhr_replay_data->FormData()) {
    if (!EnsureFreeSpace(xhr_replay_data->FormData()->SizeInBytes())) {
      xhr_replay_data->DeleteFormData();
    } else {
      content_size_ += xhr_replay_data->FormData()->SizeInBytes();
      request_ids_deque_.push_back(request_id);
    }
  }

  resource_data->SetXHRReplayData(xhr_replay_data);
}

HeapVector<Member<NetworkResourcesData::ResourceData>>
NetworkResourcesData::Resources() {
  HeapVector<Member<ResourceData>> result;
  for (auto& request : request_id_to_resource_data_map_)
    result.push_back(request.value);
  return result;
}

int64_t NetworkResourcesData::GetAndClearPendingEncodedDataLength(
    const String& request_id) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return 0;

  int64_t pending_encoded_data_length =
      resource_data->PendingEncodedDataLength();
  resource_data->ClearPendingEncodedDataLength();
  return pending_encoded_data_length;
}

void NetworkResourcesData::AddPendingEncodedDataLength(
    const String& request_id,
    size_t encoded_data_length) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return;

  resource_data->AddPendingEncodedDataLength(encoded_data_length);
}

void NetworkResourcesData::Clear(const String& preserved_loader_id) {
  if (!request_id_to_resource_data_map_.size())
    return;
  request_ids_deque_.clear();
  content_size_ = 0;

  ResourceDataMap preserved_map;

  for (auto& resource : request_id_to_resource_data_map_) {
    ResourceData* resource_data = resource.value;
    if (!preserved_loader_id.IsNull() &&
        resource_data->LoaderId() == preserved_loader_id)
      preserved_map.Set(resource.key, resource.value);
  }
  request_id_to_resource_data_map_.swap(preserved_map);
}

void NetworkResourcesData::SetResourcesDataSizeLimits(
    size_t resources_content_size,
    size_t single_resource_content_size) {
  Clear();
  maximum_resources_content_size_ = resources_content_size;
  maximum_single_resource_content_size_ = single_resource_content_size;
}

NetworkResourcesData::ResourceData*
NetworkResourcesData::ResourceDataForRequestId(const String& request_id) const {
  if (request_id.IsNull())
    return nullptr;
  return request_id_to_resource_data_map_.at(request_id);
}

void NetworkResourcesData::EnsureNoDataForRequestId(const String& request_id) {
  ResourceData* resource_data = ResourceDataForRequestId(request_id);
  if (!resource_data)
    return;
  content_size_ -= resource_data->EvictContent();
  request_id_to_resource_data_map_.erase(request_id);
}

bool NetworkResourcesData::EnsureFreeSpace(uint64_t size) {
  if (size > maximum_resources_content_size_)
    return false;

  while (size > maximum_resources_content_size_ - content_size_) {
    String request_id = request_ids_deque_.TakeFirst();
    ResourceData* resource_data = ResourceDataForRequestId(request_id);
    if (resource_data)
      content_size_ -= resource_data->EvictContent();
  }
  return true;
}

}  // namespace blink
