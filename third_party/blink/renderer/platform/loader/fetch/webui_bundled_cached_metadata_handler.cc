// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/webui_bundled_cached_metadata_handler.h"

#include "base/debug/stack_trace.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"

namespace blink {

WebUIBundledCachedMetadataHandler::WebUIBundledCachedMetadataHandler() =
    default;

WebUIBundledCachedMetadataHandler::~WebUIBundledCachedMetadataHandler() =
    default;

void WebUIBundledCachedMetadataHandler::Trace(Visitor* visitor) const {
  CachedMetadataHandler::Trace(visitor);
}

void WebUIBundledCachedMetadataHandler::SetCachedMetadata(
    CodeCacheHost* code_cache_host,
    uint32_t data_type_id,
    base::span<const uint8_t> data) {}

void WebUIBundledCachedMetadataHandler::SetSerializedCachedMetadata(
    mojo_base::BigBuffer data) {
  // We only expect to receive cached metadata from the platform once. If this
  // triggers, it indicates an efficiency problem which is most likely
  // unexpected in code designed to improve performance.
  DCHECK(!cached_metadata_);
  cached_metadata_ = CachedMetadata::CreateFromSerializedData(data);
}

void WebUIBundledCachedMetadataHandler::ClearCachedMetadata(
    CodeCacheHost* code_cache_host,
    ClearCacheType cache_type) {
  switch (cache_type) {
    case ClearCacheType::kDiscardLocally:
      cached_metadata_.reset();
      break;
    case ClearCacheType::kClearLocally:
    case ClearCacheType::kClearPersistentStorage:
      // The bundled handler should not be asked to invalidate its metadata
      // cache.
      NOTREACHED();
  }
}

scoped_refptr<CachedMetadata>
WebUIBundledCachedMetadataHandler::GetCachedMetadata(
    uint32_t data_type_id,
    GetCachedMetadataBehavior behavior) const {
  if (!cached_metadata_ || cached_metadata_->DataTypeID() != data_type_id) {
    return nullptr;
  }
  return cached_metadata_;
}

String WebUIBundledCachedMetadataHandler::Encoding() const {
  return WTF::UTF8Encoding().GetName();
}

CachedMetadataHandler::ServingSource
WebUIBundledCachedMetadataHandler::GetServingSource() const {
  return ServingSource::kWebUIBundledCache;
}

void WebUIBundledCachedMetadataHandler::OnMemoryDump(
    WebProcessMemoryDump* pmd,
    const String& dump_prefix) const {
  if (!cached_metadata_) {
    return;
  }
  const String dump_name = dump_prefix + "/webui_bundled_resource";
  auto* dump = pmd->CreateMemoryAllocatorDump(dump_name);
  dump->AddScalar("size", "bytes", GetCodeCacheSize());
  pmd->AddSuballocation(dump->Guid(),
                        String(WTF::Partitions::kAllocatedObjectPoolName));
}

size_t WebUIBundledCachedMetadataHandler::GetCodeCacheSize() const {
  return (cached_metadata_) ? cached_metadata_->SerializedData().size() : 0;
}

void WebUIBundledCachedMetadataHandler::DidUseCodeCache() {
  did_use_code_cache_for_testing_ = true;
}

}  // namespace blink
