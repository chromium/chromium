// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

ScriptCachedMetadataHandler::ScriptCachedMetadataHandler(
    const TextEncoding& encoding,
    std::unique_ptr<CachedMetadataSender> sender)
    : sender_(std::move(sender)), encoding_(encoding) {}

ScriptCachedMetadataHandler::~ScriptCachedMetadataHandler() = default;

void ScriptCachedMetadataHandler::Trace(Visitor* visitor) const {
  CachedMetadataHandler::Trace(visitor);
}

void ScriptCachedMetadataHandler::SetCachedMetadata(
    CodeCacheHost* code_cache_host,
    uint32_t data_type_id,
    base::span<const uint8_t> data) {
  DCHECK(!cached_metadata_);
  // Having been discarded once, the further attempts to overwrite the
  // CachedMetadata are ignored. This behavior is necessary to avoid clearing
  // the disk-based cache every time GetCachedMetadata() returns nullptr. The
  // JSModuleScript behaves similarly by preventing the creation of the code
  // cache.
  if (cached_metadata_discarded_)
    return;
  cached_metadata_ = CachedMetadata::Create(data_type_id, data);
  CommitToPersistentStorage(code_cache_host);
}

void ScriptCachedMetadataHandler::ClearCachedMetadata(
    CodeCacheHost* code_cache_host,
    ClearCacheType cache_type) {
  cached_metadata_ = nullptr;
  switch (cache_type) {
    case kClearLocally:
      break;
    case kDiscardLocally:
      cached_metadata_discarded_ = true;
      break;
    case kClearPersistentStorage:
      CommitToPersistentStorage(code_cache_host);
      break;
  }
}

scoped_refptr<CachedMetadata> ScriptCachedMetadataHandler::GetCachedMetadata(
    uint32_t data_type_id,
    GetCachedMetadataBehavior behavior) const {
  if (!cached_metadata_ || cached_metadata_->DataTypeID() != data_type_id) {
    return nullptr;
  }
  return cached_metadata_;
}

void ScriptCachedMetadataHandler::SetSerializedCachedMetadata(
    mojo_base::BigBuffer data) {
  // We only expect to receive cached metadata from the platform once. If this
  // triggers, it indicates an efficiency problem which is most likely
  // unexpected in code designed to improve performance.
  DCHECK(!cached_metadata_);
  cached_metadata_ = CachedMetadata::CreateFromSerializedData(data);
}

String ScriptCachedMetadataHandler::Encoding() const {
  return encoding_.GetName();
}

CachedMetadataHandler::ServingSource
ScriptCachedMetadataHandler::GetServingSource() const {
  return sender_->IsServedFromCacheStorage() ? ServingSource::kCacheStorage
                                             : ServingSource::kOther;
}

void ScriptCachedMetadataHandler::OnMemoryDump(
    WebProcessMemoryDump* pmd,
    const String& dump_prefix) const {
  if (!cached_metadata_)
    return;
  const String dump_name = StrCat({dump_prefix, "/script"});
  auto* dump = pmd->CreateMemoryAllocatorDump(dump_name);
  dump->AddScalar("size", "bytes", GetCodeCacheSize());
  pmd->AddSuballocation(dump->Guid(),
                        String(Partitions::kAllocatedObjectPoolName));
}

size_t ScriptCachedMetadataHandler::GetCodeCacheSize() const {
  return (cached_metadata_) ? cached_metadata_->SerializedData().size() : 0;
}

void ScriptCachedMetadataHandler::CommitToPersistentStorage(
    CodeCacheHost* code_cache_host) {
  if (cached_metadata_) {
    sender_->Send(code_cache_host, cached_metadata_->SerializedData());
  } else {
    sender_->Send(code_cache_host, base::span<const uint8_t>());
  }
}

ScriptCachedMetadataHandlerWithHashing::ScriptCachedMetadataHandlerWithHashing(
    const TextEncoding& encoding,
    std::unique_ptr<CachedMetadataSender> sender)
    : ScriptCachedMetadataHandler(encoding, std::move(sender)) {}

void ScriptCachedMetadataHandlerWithHashing::Check(
    CodeCacheHost* code_cache_host,
    const ParkableString& source_text) {
  const ParkableString::DigestHolder digest_holder = source_text.Digest();
  const SecureStringDigest& digest = digest_holder.Get();

  if (hash_state_ != kUninitialized) {
    // Compare the hash of the new source text with the one previously loaded.
    if (base::span(digest) != hash_) {
      // If this handler was previously checked and is now being checked again
      // with a different hash value, then something bad happened. We expect the
      // handler to only be used with one script source text.
      CHECK_NE(hash_state_, kChecked);

      // The cached metadata is invalid because the source file has changed.
      ClearCachedMetadata(code_cache_host, kClearPersistentStorage);
    }
  }

  // Remember the computed hash so that it can be used when saving data to
  // persistent storage.
  base::span(hash_).copy_from(digest);
  hash_state_ = kChecked;
}

void ScriptCachedMetadataHandlerWithHashing::SetSerializedCachedMetadata(
    mojo_base::BigBuffer data) {
  // We only expect to receive cached metadata from the platform once. If this
  // triggers, it indicates an efficiency problem which is most likely
  // unexpected in code designed to improve performance.
  DCHECK(!cached_metadata());
  DCHECK_EQ(hash_state_, kUninitialized);

  // The kChecked state guarantees that hash_ will never be updated again.
  CHECK(hash_state_ != kChecked);

  // Ensure the data is big enough, otherwise discard the data.
  if (data.size() < sizeof(CachedMetadataHeaderWithHash)) {
    return;
  }
  auto [header_bytes, payload_bytes] =
      base::span(data).split_at(sizeof(CachedMetadataHeaderWithHash));

  // Ensure the marker matches, otherwise discard the data.
  const CachedMetadataHeaderWithHash* header =
      reinterpret_cast<const CachedMetadataHeaderWithHash*>(
          header_bytes.data());
  if (header->marker != CachedMetadataHandler::kSingleEntryWithHashAndPadding) {
    return;
  }

  // Split out the data into the hash and the CachedMetadata that follows.
  base::span(hash_).copy_from(header->hash);
  hash_state_ = kDeserialized;
  set_cached_metadata(CachedMetadata::CreateFromSerializedData(
      data, sizeof(CachedMetadataHeaderWithHash)));
}

scoped_refptr<CachedMetadata>
ScriptCachedMetadataHandlerWithHashing::GetCachedMetadata(
    uint32_t data_type_id,
    GetCachedMetadataBehavior behavior) const {
  // The caller should have called Check before attempting to read the cached
  // metadata. If you just want to know whether cached metadata exists, and it's
  // okay for that metadata to possibly mismatch with the loaded script content,
  // then you can pass kAllowUnchecked as the second parameter.
  if (behavior == kCrashIfUnchecked) {
    CHECK(hash_state_ == kChecked);
  }

  scoped_refptr<CachedMetadata> result =
      ScriptCachedMetadataHandler::GetCachedMetadata(data_type_id, behavior);

  return result;
}

void ScriptCachedMetadataHandlerWithHashing::CommitToPersistentStorage(
    CodeCacheHost* code_cache_host) {
  sender()->Send(code_cache_host, GetSerializedCachedMetadata());
}

Vector<uint8_t>
ScriptCachedMetadataHandlerWithHashing::GetSerializedCachedMetadata() const {
  Vector<uint8_t> serialized_data;
  if (cached_metadata() && hash_state_ == kChecked) {
    uint32_t marker = CachedMetadataHandler::kSingleEntryWithHashAndPadding;
    CHECK_EQ(serialized_data.size(),
             offsetof(CachedMetadataHeaderWithHash, marker));
    serialized_data.append_range(base::byte_span_from_ref(marker));
    uint32_t padding = 0;
    CHECK_EQ(serialized_data.size(),
             offsetof(CachedMetadataHeaderWithHash, padding));
    serialized_data.append_range(base::byte_span_from_ref(padding));
    CHECK_EQ(serialized_data.size(),
             offsetof(CachedMetadataHeaderWithHash, hash));
    serialized_data.append_range(hash_);
    CHECK_EQ(serialized_data.size(), sizeof(CachedMetadataHeaderWithHash));
    serialized_data.append_range(cached_metadata()->SerializedData());
  }
  return serialized_data;
}

void ScriptCachedMetadataHandlerWithHashing::ResetForTesting() {
  if (hash_state_ == kChecked)
    hash_state_ = kDeserialized;
}

SourceKeyedCachedMetadataHandler::SourceKeyedCachedMetadataHandler(
    const TextEncoding& encoding,
    const ParkableString& source_text)
    : encoding_(encoding), source_hash_(source_text.Digest().Get()) {}

SourceKeyedCachedMetadataHandler::~SourceKeyedCachedMetadataHandler() = default;

void SourceKeyedCachedMetadataHandler::Trace(Visitor* visitor) const {
  CachedMetadataHandler::Trace(visitor);
}

void SourceKeyedCachedMetadataHandler::SetCachedMetadata(
    CodeCacheHost* code_cache_host,
    uint32_t data_type_id,
    base::span<const uint8_t> data) {
  CHECK(!cached_metadata_);
  if (!code_cache_host) {
    return;
  }

  cached_metadata_ = CachedMetadata::Create(data_type_id, data);
  code_cache_host->get()->DidGenerateSourceKeyedCacheableMetadata(
      blink::Vector<uint8_t>(source_hash_), cached_metadata_->SerializedData());
}

void SourceKeyedCachedMetadataHandler::SetSerializedCachedMetadata(
    mojo_base::BigBuffer data) {
  // We only expect to receive cached metadata from the platform once. If this
  // triggers, it indicates an efficiency problem which is most likely
  // unexpected in code designed to improve performance.
  DCHECK(!cached_metadata_);
  cached_metadata_ = CachedMetadata::CreateFromSerializedData(data);
}

void SourceKeyedCachedMetadataHandler::ClearCachedMetadata(
    CodeCacheHost* code_cache_host,
    ClearCacheType cache_type) {
  cached_metadata_ = nullptr;
}

scoped_refptr<CachedMetadata>
SourceKeyedCachedMetadataHandler::GetCachedMetadata(
    uint32_t data_type_id,
    GetCachedMetadataBehavior behavior) const {
  if (!cached_metadata_ || cached_metadata_->DataTypeID() != data_type_id) {
    return nullptr;
  }
  return cached_metadata_;
}

String SourceKeyedCachedMetadataHandler::Encoding() const {
  return encoding_.GetName();
}

CachedMetadataHandler::ServingSource
SourceKeyedCachedMetadataHandler::GetServingSource() const {
  return ServingSource::kOther;
}

void SourceKeyedCachedMetadataHandler::OnMemoryDump(
    WebProcessMemoryDump* pmd,
    const String& dump_prefix) const {
  if (!cached_metadata_) {
    return;
  }
  const String dump_name = StrCat({dump_prefix, "/inline_script"});
  auto* dump = pmd->CreateMemoryAllocatorDump(dump_name);
  dump->AddScalar("size", "bytes", GetCodeCacheSize());
  pmd->AddSuballocation(dump->Guid(),
                        String(Partitions::kAllocatedObjectPoolName));
}

size_t SourceKeyedCachedMetadataHandler::GetCodeCacheSize() const {
  return cached_metadata_ ? cached_metadata_->SerializedData().size() : 0;
}

}  // namespace blink
