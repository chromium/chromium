// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SOURCE_KEYED_CACHED_METADATA_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SOURCE_KEYED_CACHED_METADATA_HANDLER_H_

#include <stdint.h>

#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

// An implementation of CachedMetadataHandler which can hold multiple
// CachedMetadata entries. These entries are keyed by a cryptograph hash of the
// source code which produced them.
//
// This is used to store cached metadata for multiple inline scripts on a single
// HTML document's resource.
class PLATFORM_EXPORT SourceKeyedCachedMetadataHandler final
    : public CachedMetadataHandler {
 public:
  SourceKeyedCachedMetadataHandler(
      WTF::TextEncoding encoding,
      std::unique_ptr<CachedMetadataSender> send_callback)
      : sender_(std::move(send_callback)), encoding_(encoding) {}

  // Produce a metadata handler for a single cached metadata associated with
  // the given source code.
  SingleCachedMetadataHandler* HandlerForSource(const String& source);

  void ClearCachedMetadata(
      CodeCacheHost*,
      CachedMetadataHandler::ClearCacheType cache_type) override;
  String Encoding() const override;
  bool IsServedFromCacheStorage() const override {
    return sender_->IsServedFromCacheStorage();
  }
  void OnMemoryDump(WebProcessMemoryDump* pmd,
                    const String& dump_prefix) const override;
  size_t GetCodeCacheSize() const override {
    // No need to implement this as inline scripts are not kept in
    // blink::MemoryCache
    return 0;
  }

  void SetSerializedCachedMetadata(mojo_base::BigBuffer data);

  // Called after all inline scripts have been loaded to log metrics.
  void LogUsageMetrics();

 private:
  // Keys are SHA-256, which are 256/8 = 32 bytes.
  static constexpr size_t kKeySize = 32;
  using Key = Vector<uint8_t, kKeySize>;

  class SingleKeyHandler;
  class KeyTraits : public WTF::GenericHashTraits<Key> {
   public:
    static void ConstructDeletedValue(Key& slot, bool) {
      new (NotNullTag::kNotNull, &slot) Key(WTF::kHashTableDeletedValue);
      DCHECK(IsDeletedValue(slot));
      DCHECK(!IsEmptyValue(slot));
    }

    static bool IsEmptyValue(const Key& key) {
      return !IsDeletedValue(key) && key.IsEmpty();
    }

    static bool IsDeletedValue(const Key& value) {
      return value.IsHashTableDeletedValue();
    }

    static unsigned GetHash(const Key& key) {
      return StringHasher::ComputeHash(key.data(),
                                       static_cast<uint32_t>(key.size()));
    }

    static bool Equal(const Key& a, const Key& b) { return a == b; }

    static const bool kEmptyValueIsZero = false;
    static const bool safe_to_compare_to_empty_or_deleted = false;
    static const bool kHasIsEmptyValueFunction = true;
  };

  void SendToPlatform(CodeCacheHost*);

  // TODO(leszeks): Maybe just store the SingleKeyHandlers directly in here?
  using CachedMetadataMap =
      WTF::HashMap<Key, scoped_refptr<CachedMetadata>, KeyTraits, KeyTraits>;
  CachedMetadataMap cached_metadata_map_;
  std::unique_ptr<CachedMetadataSender> sender_;

  const WTF::TextEncoding encoding_;

  bool did_use_code_cache_ = false;
  bool will_generate_code_cache_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SOURCE_KEYED_CACHED_METADATA_HANDLER_H_
