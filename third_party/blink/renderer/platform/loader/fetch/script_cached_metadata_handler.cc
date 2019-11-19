// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"

#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

ScriptCachedMetadataHandler::ScriptCachedMetadataHandler(
    const WTF::TextEncoding& encoding,
    std::unique_ptr<CachedMetadataSender> sender)
    : sender_(std::move(sender)), encoding_(encoding) {}

void ScriptCachedMetadataHandler::Trace(blink::Visitor* visitor) {
  CachedMetadataHandler::Trace(visitor);
}

void ScriptCachedMetadataHandler::SetCachedMetadata(
    uint32_t data_type_id,
    const uint8_t* data,
    size_t size,
    CachedMetadataHandler::CacheType cache_type) {
  // Currently, only one type of cached metadata per resource is supported. If
  // the need arises for multiple types of metadata per resource this could be
  // enhanced to store types of metadata in a map.
  DCHECK(!cached_metadata_);
  cached_metadata_ = CachedMetadata::Create(data_type_id, data, size);
  if (cache_type == CachedMetadataHandler::kSendToPlatform)
    SendToPlatform();
}

void ScriptCachedMetadataHandler::ClearCachedMetadata(
    CachedMetadataHandler::CacheType cache_type) {
  cached_metadata_ = nullptr;
  if (cache_type == CachedMetadataHandler::kSendToPlatform)
    SendToPlatform();
}

scoped_refptr<CachedMetadata> ScriptCachedMetadataHandler::GetCachedMetadata(
    uint32_t data_type_id) const {
  if (!cached_metadata_ || cached_metadata_->DataTypeID() != data_type_id)
    return nullptr;
  return cached_metadata_;
}

void ScriptCachedMetadataHandler::SetSerializedCachedMetadata(
    mojo_base::BigBuffer data) {
  // We only expect to receive cached metadata from the platform once. If this
  // triggers, it indicates an efficiency problem which is most likely
  // unexpected in code designed to improve performance.
  DCHECK(!cached_metadata_);
  cached_metadata_ = CachedMetadata::CreateFromSerializedData(std::move(data));
}

String ScriptCachedMetadataHandler::Encoding() const {
  return String(encoding_.GetName());
}

bool ScriptCachedMetadataHandler::IsServedFromCacheStorage() const {
  return sender_->IsServedFromCacheStorage();
}

void ScriptCachedMetadataHandler::OnMemoryDump(
    WebProcessMemoryDump* pmd,
    const String& dump_prefix) const {
  if (!cached_metadata_)
    return;
  const String dump_name = dump_prefix + "/script";
  auto* dump = pmd->CreateMemoryAllocatorDump(dump_name);
  dump->AddScalar("size", "bytes", GetCodeCacheSize());
  pmd->AddSuballocation(dump->Guid(),
                        String(WTF::Partitions::kAllocatedObjectPoolName));
}

size_t ScriptCachedMetadataHandler::GetCodeCacheSize() const {
  return (cached_metadata_) ? cached_metadata_->SerializedData().size() : 0;
}

void ScriptCachedMetadataHandler::SendToPlatform() {
  if (cached_metadata_) {
    base::span<const uint8_t> serialized_data =
        cached_metadata_->SerializedData();
    sender_->Send(serialized_data.data(), serialized_data.size());
  } else {
    sender_->Send(nullptr, 0);
  }
}

}  // namespace blink
