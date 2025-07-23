// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/webui_bundled_cached_metadata_handler.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

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
      // This can be reached if v8 rejects `cached_metadata_` following script
      // compilation. These ClearCacheTypes request that the persistent storage
      // invalidate the `cached_metadata_`. However for code caches backed by
      // the static resource bundle this doesn't apply. Subsequent requests may
      // continue to attempt to use the resource bundled code cache, however
      // this will not affect correctness as the code cache will simply be
      // rejected and loading will proceed as usual.
      // TODO(crbug.com/378504631): In practice this should not occur as
      // validity of the bundled code cache is enforced at build-time. Update
      // this to NOTREACHED() once this has been confirmed experimentally.
      cached_metadata_.reset();
      RESOURCE_LOADING_DVLOG(1) << "Failed to clear WebUI bundled metadata";
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
  return Utf8Encoding().GetName();
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
  const String dump_name = StrCat({dump_prefix, "/webui_bundled_resource"});
  auto* dump = pmd->CreateMemoryAllocatorDump(dump_name);
  dump->AddScalar("size", "bytes", GetCodeCacheSize());
  pmd->AddSuballocation(dump->Guid(),
                        String(Partitions::kAllocatedObjectPoolName));
}

size_t WebUIBundledCachedMetadataHandler::GetCodeCacheSize() const {
  return (cached_metadata_) ? cached_metadata_->SerializedData().size() : 0;
}

void WebUIBundledCachedMetadataHandler::DidUseCodeCache(bool was_rejected) {
  base::UmaHistogramBoolean(
      "Blink.ResourceRequest.WebUIBundledCachedMetadataHandler.ConsumeCache",
      !was_rejected);
  did_use_code_cache_for_testing_ = true;
}

}  // namespace blink
