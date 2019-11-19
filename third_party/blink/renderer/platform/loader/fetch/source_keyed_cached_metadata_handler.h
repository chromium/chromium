// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SOURCE_KEYED_CACHED_METADATA_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SOURCE_KEYED_CACHED_METADATA_HANDLER_H_

#include <stdint.h>
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
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
      CachedMetadataHandler::CacheType cache_type) override;
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

 private:
  // Keys are SHA-256, which are 256/8 = 32 bytes.
  static constexpr size_t kKeySize = 32;
  typedef Vector<uint8_t, kKeySize> Key;

  class SingleKeyHandler;
  class KeyHash;
  class KeyHashTraits : public WTF::GenericHashTraits<Key> {
   public:
    // Note: This class relies on hashes never being zero or 1 followed by all
    // zeros. Practically, our hash space is large enough that the risk of such
    // a collision is infinitesimal.

    typedef Key EmptyValueType;
    static const bool kEmptyValueIsZero = true;
    static EmptyValueType EmptyValue() {
      // Rely on integer value initialization to zero out the key array.
      return Key{};
    }

    static void ConstructDeletedValue(Key& slot, bool) {
      slot = {1};  // Remaining entries are value initialized to 0.
    }
    static bool IsDeletedValue(const Key& value) { return value == Key{1}; }
  };

  void SendToPlatform();

  // TODO(leszeks): Maybe just store the SingleKeyHandlers directly in here?
  WTF::HashMap<Key, scoped_refptr<CachedMetadata>, KeyHash, KeyHashTraits>
      cached_metadata_map_;
  std::unique_ptr<CachedMetadataSender> sender_;

  const WTF::TextEncoding encoding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SOURCE_KEYED_CACHED_METADATA_HANDLER_H_
