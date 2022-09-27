// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_BACKGROUND_TRACING_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_BACKGROUND_TRACING_HELPER_H_

#include <cstdint>
#include <string>

#include "base/strings/string_piece.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class PerformanceMark;

// The following class is a helper for implementing the background-tracing
// performance.mark integration. See crbug.com/1181774 for details, and refer
// to integration with performance.cc.

// This is CORE_EXPORT for component builds of blink_unittests.
class CORE_EXPORT BackgroundTracingHelper final
    : public GarbageCollected<BackgroundTracingHelper> {
 public:
  using MarkHashSet = HashSet<uint32_t>;
  using SiteMarkHashMap = HashMap<uint32_t, MarkHashSet>;

  explicit BackgroundTracingHelper(ExecutionContext* context);
  ~BackgroundTracingHelper();

  void MaybeEmitBackgroundTracingPerformanceMarkEvent(
      const PerformanceMark& mark);

  // Implements GarbageCollected:
  void Trace(Visitor*) const;

 protected:
  struct SiteMarkHashMapContainer;
  friend class BackgroundTracingHelperTest;

  // Returns a reference to a thread-safe static singleton `SiteMarkHashMap`,
  // with all of the configured allow-listed sites and marks. This object is
  // populated with parsed data upon first access.
  static const SiteMarkHashMap& GetSiteMarkHashMap();

  // Returns a pointer to the `MarkHashSet` contain allow-listed hashes for the
  // provided |site_hash|. This will be nullptr if there are no allow-listed
  // mark hashes for the given |site_hash|. This is threadsafe.
  static const MarkHashSet* GetMarkHashSetForSiteHash(uint32_t site_hash);

  // Splits a string and an optional numeric suffix preceded by an underscore.
  // This is used by the "sequence number" mechanism for mark names. Returns
  // the location of the underscore if a split is to occur, otherwise returns
  // 0.
  static size_t GetSequenceNumberPos(base::StringPiece string);

  // Generates a 32-bit MD5 hash of the given string piece. This will return a
  // value that is equivalent to the first 8 bytes of a full MD5 hash. In bash
  // parlance, the returned 32-bit integer expressed in hex format:
  //
  //   printf "%08x" <hashed_value>
  //
  // will have the same value as
  //
  //   echo -n <string_value> | md5sum | cut -b 1-8
  //
  // This will return the same result as MD5Hash32Constexpr as defined in
  // base/hash/md5_constexpr.h. This uses base::StringPiece because it is
  // interacting with Finch code, which doesn't use WTF primitives.
  static uint32_t MD5Hash32(base::StringPiece string);

  // Given a mark name with an optional sequence number suffix, parses out the
  // suffix and hashes the mark name.
  static void GetMarkHashAndSequenceNumber(base::StringPiece mark_name,
                                           uint32_t sequence_number_offset,
                                           uint32_t* mark_hash,
                                           uint32_t* sequence_number);

  // For the given |target_site_hash| (`MD5Hash32()` of eTLD+1 represented in
  // ASCII), and the provided background-tracing performance.mark |allow_list|
  // (also represented as an ASCII std::string), populates vector
  // |allow_listed_mark_hashes| of allowed performance.mark event names, as
  // 32-bit hashes. Returns true on success, or false if any errors were
  // observed in the input data. If this returns false the
  // |allow_listed_mark_hashes| will be returned empty. This uses
  // std::string because it is interacting with Finch code, which doesn't use
  // WTF primitives.
  static bool ParseBackgroundTracingPerformanceMarkHashes(
      base::StringPiece allow_list,
      SiteMarkHashMap& allow_listed_hashes);

 private:
  String site_;
  uint32_t site_hash_ = 0;
  uint32_t execution_context_id_ = 0;
  uint32_t sequence_number_offset_ = 0;
  // This points to a thread-safe global singleton.
  const MarkHashSet* mark_hashes_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_BACKGROUND_TRACING_HELPER_H_
