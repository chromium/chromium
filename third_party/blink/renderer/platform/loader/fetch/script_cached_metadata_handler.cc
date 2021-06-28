// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

namespace {

void RecordState(StateOnGet state) {
  UMA_HISTOGRAM_ENUMERATION("Memory.Renderer.BlinkCachedMetadataGetResult",
                            state);
}

}  // namespace

ScriptCachedMetadataHandler::ScriptCachedMetadataHandler(
    const WTF::TextEncoding& encoding,
    std::unique_ptr<CachedMetadataSender> sender)
    : sender_(std::move(sender)), encoding_(encoding) {}

void ScriptCachedMetadataHandler::Trace(Visitor* visitor) const {
  CachedMetadataHandler::Trace(visitor);
}

void ScriptCachedMetadataHandler::SetCachedMetadata(uint32_t data_type_id,
                                                    const uint8_t* data,
                                                    size_t size) {
  DCHECK(!cached_metadata_);
  // Having been discarded once, the further attempts to overwrite the
  // CachedMetadata are ignored. This behavior is necessary to avoid clearing
  // the disk-based cache every time GetCachedMetadata() returns nullptr. The
  // JSModuleScript behaves similarly by preventing the creation of the code
  // cache.
  if (cached_metadata_discarded_)
    return;
  cached_metadata_ = CachedMetadata::Create(data_type_id, data, size);
  if (!disable_send_to_platform_for_testing_)
    CommitToPersistentStorage();
}

void ScriptCachedMetadataHandler::ClearCachedMetadata(
    ClearCacheType cache_type) {
  cached_metadata_ = nullptr;
  switch (cache_type) {
    case kClearLocally:
      break;
    case kDiscardLocally:
      cached_metadata_discarded_ = true;
      break;
    case kClearPersistentStorage:
      CommitToPersistentStorage();
      break;
  }
}

scoped_refptr<CachedMetadata> ScriptCachedMetadataHandler::GetCachedMetadata(
    uint32_t data_type_id) const {
  if (!cached_metadata_) {
    RecordState(cached_metadata_discarded_ ? StateOnGet::kWasDiscarded
                                           : StateOnGet::kWasNeverPresent);
    return nullptr;
  }
  if (cached_metadata_->DataTypeID() != data_type_id) {
    RecordState(StateOnGet::kDataTypeMismatch);
    return nullptr;
  }
  RecordState(StateOnGet::kPresent);
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

void ScriptCachedMetadataHandler::CommitToPersistentStorage() {
  if (cached_metadata_) {
    base::span<const uint8_t> serialized_data =
        cached_metadata_->SerializedData();
    sender_->Send(serialized_data.data(), serialized_data.size());
  } else {
    sender_->Send(nullptr, 0);
  }
}

}  // namespace blink
