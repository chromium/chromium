// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SCRIPT_CACHED_METADATA_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SCRIPT_CACHED_METADATA_HANDLER_H_

#include <memory>

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

class CachedMetadata;
class CachedMetadataSender;

// ScriptCachedMetadataHandler should be created when a response is received,
// and can be used independently from its Resource.
// - It doesn't have any references to the Resource. Necessary data are captured
// from the Resource when the handler is created.
// - It is not affected by Resource's revalidation on MemoryCache. The validity
// of the handler is solely checked by |response_url_| and |response_time_|
// (not by Resource) by the browser process, and the cached metadata written to
// the handler is rejected if e.g. the disk cache entry has been updated and the
// handler refers to an older response.
class PLATFORM_EXPORT ScriptCachedMetadataHandler final
    : public SingleCachedMetadataHandler {
 public:
  ScriptCachedMetadataHandler(const WTF::TextEncoding&,
                              std::unique_ptr<CachedMetadataSender>);
  ~ScriptCachedMetadataHandler() override = default;
  void Trace(blink::Visitor*) override;
  void SetCachedMetadata(uint32_t, const uint8_t*, size_t, CacheType) override;
  void ClearCachedMetadata(CacheType) override;
  scoped_refptr<CachedMetadata> GetCachedMetadata(uint32_t) const override;

  // This returns the encoding at the time of ResponseReceived(). Therefore this
  // does NOT reflect encoding detection from body contents, but the actual
  // encoding after the encoding detection can be determined uniquely from
  // Encoding(), provided the body content is the same, as we can assume the
  // encoding detection will result in the same final encoding.
  // TODO(hiroshige): Make these semantics cleaner.
  String Encoding() const override;

  bool IsServedFromCacheStorage() const override;

  void OnMemoryDump(WebProcessMemoryDump* pmd,
                    const String& dump_prefix) const override;

  // Sets the serialized metadata retrieved from the platform's cache.
  void SetSerializedCachedMetadata(mojo_base::BigBuffer data);
  size_t GetCodeCacheSize() const override;

 private:
  void SendToPlatform();

  scoped_refptr<CachedMetadata> cached_metadata_;
  std::unique_ptr<CachedMetadataSender> sender_;

  const WTF::TextEncoding encoding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SCRIPT_CACHED_METADATA_HANDLER_H_
