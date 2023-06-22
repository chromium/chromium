// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SCRIPT_CACHED_METADATA_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SCRIPT_CACHED_METADATA_HANDLER_H_

#include <memory>

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace mojo_base {
class BigBuffer;
}

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
class PLATFORM_EXPORT ScriptCachedMetadataHandler
    : public CachedMetadataHandler {
 public:
  ScriptCachedMetadataHandler(const WTF::TextEncoding&,
                              std::unique_ptr<CachedMetadataSender>);
  ~ScriptCachedMetadataHandler() override;
  void Trace(Visitor*) const override;
  void SetCachedMetadata(CodeCacheHost*,
                         uint32_t,
                         const uint8_t*,
                         size_t) override;
  void ClearCachedMetadata(CodeCacheHost*, ClearCacheType) override;
  scoped_refptr<CachedMetadata> GetCachedMetadata(
      uint32_t,
      GetCachedMetadataBehavior = kCrashIfUnchecked) const override;

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
  virtual void SetSerializedCachedMetadata(mojo_base::BigBuffer data);
  size_t GetCodeCacheSize() const override;

 protected:
  virtual void CommitToPersistentStorage(CodeCacheHost*);

  CachedMetadataSender* Sender() const { return sender_.get(); }

  scoped_refptr<CachedMetadata> cached_metadata_;

 private:
  friend class ModuleScriptTest;

  bool cached_metadata_discarded_ = false;
  std::unique_ptr<CachedMetadataSender> sender_;

  const WTF::TextEncoding encoding_;
};

// The serialized header format for cached metadata which includes a hash of the
// script text. This header is followed by a second header in the format defined
// by CachedMetadataHeader, which is in turn followed by the data.
struct CachedMetadataHeaderWithHash {
  static constexpr uint32_t kSha256Bytes = 256 / 8;
  uint32_t
      marker;  // Must be CachedMetadataHandler::kSingleEntryWithHashAndPadding.
  uint32_t padding;  // Always zero. Ensures 8-byte alignment of subsequent data
                     // so that reading the tag is defined behavior and V8
                     // doesn't copy the data again for better alignment (see
                     // AlignedCachedData).
  uint8_t hash[kSha256Bytes];
};

class PLATFORM_EXPORT ScriptCachedMetadataHandlerWithHashing final
    : public ScriptCachedMetadataHandler {
 public:
  ScriptCachedMetadataHandlerWithHashing(
      const WTF::TextEncoding& encoding,
      std::unique_ptr<CachedMetadataSender> sender)
      : ScriptCachedMetadataHandler(encoding, std::move(sender)) {}
  ~ScriptCachedMetadataHandlerWithHashing() override = default;

  // Sets the serialized metadata retrieved from the platform's cache.
  void SetSerializedCachedMetadata(mojo_base::BigBuffer data) override;

  bool HashRequired() const override { return true; }

  scoped_refptr<CachedMetadata> GetCachedMetadata(
      uint32_t,
      GetCachedMetadataBehavior = kCrashIfUnchecked) const override;

  // Pretend that the current content and hash were loaded from disk, not
  // created by the current process.
  void ResetForTesting();

  void Check(CodeCacheHost*, const ParkableString& source_text) override;

  Vector<uint8_t> GetSerializedCachedMetadata() const;

 protected:
  void CommitToPersistentStorage(CodeCacheHost*) override;

 private:
  static const uint32_t kSha256Bytes = 256 / 8;
  uint8_t hash_[kSha256Bytes];
  enum HashState {
    kUninitialized,  // hash_ has not been written.
    kDeserialized,   // hash_ contains data from the code cache that has not yet
                     // been checked for matching the script text.
    kChecked,        // hash_ contains the hash of the script text. Neither
                     // hash_state_ nor hash_ will ever change again.
  };
  HashState hash_state_ = kUninitialized;
};

// Describes a few interesting states of the ScriptCachedMetadataHandler when
// GetCachedMetadata() is called. These values are written to logs. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum class StateOnGet : int {
  kPresent = 0,
  kDataTypeMismatch = 1,
  kWasNeverPresent = 2,
  kWasDiscarded = 3,

  // Must be equal to the greatest among enumeraiton values.
  kMaxValue = kWasDiscarded
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SCRIPT_CACHED_METADATA_HANDLER_H_
