/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_NG_SHAPE_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_NG_SHAPE_CACHE_H_

#include "base/hash/hash.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/fonts/font_performance.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_coordinator/memory_consumer_registration.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// The `can_cache` flag is used to indicate if this shape-result can be
// inserted into the cache.
// In certain circumstances the shape call isn't guaranteed to be idempotent,
// e.g. when reusing previous shape-results.
struct ShaperResult {
  STACK_ALLOCATED();

 public:
  const ShapeResult* shape_result;
  bool can_cache;
};

// The key for the shape-cache. Contains:
//  - The text to be shaped.
//  - The start/end offset of the text.
//
// NOTE: We don't store a StringView or similar so we can reuse the hash of the
//       text in multiple keys.
struct ShapeCacheKey {
  DISALLOW_NEW();

 public:
  ShapeCacheKey() = default;
  ShapeCacheKey(const String& text,
                unsigned start_offset,
                unsigned end_offset,
                const AtomicString& locale,
                base::span<const FontFeatureRange> font_features,
                TextDirection direction)
      : text_(text),
        start_offset_(start_offset),
        end_offset_(end_offset),
        locale_(locale),
        font_features_(font_features),
        direction_(direction) {
    DCHECK_NE(text_, g_empty_string);
  }

  explicit ShapeCacheKey(HashTableDeletedValueType) : text_(g_empty_string) {}

  bool IsHashTableDeletedValue() const { return text_ == g_empty_string; }

  unsigned GetHash() const {
    unsigned hash = blink::GetHash(text_);
    AddIntToHash(hash, start_offset_);
    AddIntToHash(hash, end_offset_);
    AddIntToHash(hash, locale_ ? blink::GetHash(locale_) : 0);
    AddIntToHash(hash, static_cast<unsigned>(StringHasher::HashMemory(
                           base::as_byte_span(font_features_))));
    AddIntToHash(hash, static_cast<unsigned>(direction_));
    return hash;
  }

  bool operator==(const ShapeCacheKey& other) const {
    return text_ == other.text_ && start_offset_ == other.start_offset_ &&
           end_offset_ == other.end_offset_ && locale_ == other.locale_ &&
           font_features_ == other.font_features_ &&
           direction_ == other.direction_;
  }

  const String& GetText() const { return text_; }

 private:
  String text_;
  unsigned start_offset_ = 0u;
  unsigned end_offset_ = 0u;
  AtomicString locale_;
  Vector<FontFeatureRange, 1> font_features_;
  TextDirection direction_ = TextDirection::kLtr;
};

template <>
struct HashTraits<ShapeCacheKey> : SimpleClassHashTraits<ShapeCacheKey> {};

class NGShapeCache : public GarbageCollected<NGShapeCache>,
                     public base::MemoryConsumer {
  USING_PRE_FINALIZER(NGShapeCache, Dispose);

 public:
  static constexpr unsigned kMaxTextLengthOfEntries = 30;
  static constexpr unsigned kMaxSize = 2048;

  explicit NGShapeCache(const SimpleFontData* primary_font)
      : primary_font_(primary_font) {
    if (RuntimeEnabledFeatures::MemoryConsumerForNGShapeCacheEnabled() &&
        base::MemoryConsumerRegistry::Exists() &&
        (!base::SingleThreadTaskRunner::HasMainThreadDefault() ||
         base::SingleThreadTaskRunner::GetMainThreadDefault()
             ->RunsTasksInCurrentSequence())) {
      memory_consumer_registration_ =
          std::make_unique<MemoryConsumerRegistration>(
              kConsumerId, kNGShapeCacheTraits, this,
              MemoryConsumerRegistration::CheckUnregister::kDisabled);
    }
  }
  NGShapeCache(const NGShapeCache&) = delete;
  NGShapeCache& operator=(const NGShapeCache&) = delete;

  void Trace(Visitor* visitor) const {
    visitor->Trace(weak_map_);
    visitor->Trace(strong_map_);
    visitor->Trace(primary_font_);
  }

  void Dispose() {
    if (memory_consumer_registration_) {
      memory_consumer_registration_->Dispose();
    }
  }

  void OnUpdateMemoryLimit() override {}
  void OnReleaseMemory() override {
    weak_map_.clear();
    strong_map_.clear();
  }

  template <typename ShapeResultFunc>
  const ShapeResult* GetOrCreate(const ShapeCacheKey& key,
                                 const ShapeResultFunc& shape_result_func) {
    DCHECK(!key.GetText().empty());

    if (key.GetText().length() > kMaxTextLengthOfEntries) {
      return shape_result_func().shape_result;
    }

    if (RuntimeEnabledFeatures::MemoryConsumerForNGShapeCacheEnabled()) {
      return GetOrCreateImpl(strong_map_, key, shape_result_func);
    } else {
      return GetOrCreateImpl(weak_map_, key, shape_result_func);
    }
  }

 private:
  typedef HeapHashMap<ShapeCacheKey, WeakMember<const ShapeResult>> WeakMap;
  typedef HeapHashMap<ShapeCacheKey, Member<const ShapeResult>> StrongMap;

  static constexpr char kConsumerId[] = "NGShapeCache";
  static constexpr base::MemoryConsumerTraits kNGShapeCacheTraits{
      // Bounded entries of short strings; footprint under 10MB.
      base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
      // Clearing maps requires traversing hash map structures.
      base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
      // Results can be shaped again if cleared.
      base::MemoryConsumerTraits::InformationRetention::kLossless,
      // Synchronously clears maps.
      base::MemoryConsumerTraits::ExecutionType::kSynchronous,
      // Fixed entry limit that is independent to memory pressure.
      base::MemoryConsumerTraits::SupportsMemoryLimit::kNo,
      // Shaping short text runs is fast.
      base::MemoryConsumerTraits::RecreateMemoryCost::kCheap,
      // Drops pointers allocated on the Oilpan heap.
      base::MemoryConsumerTraits::ReleaseGCReferences::kYes,
      // Performs a one-shot eviction of the whole cache when under pressure.
      base::MemoryConsumerTraits::IsStateful::kNo};

  template <typename StringMap, typename ShapeResultFunc>
  const ShapeResult* GetOrCreateImpl(StringMap& map,
                                     const ShapeCacheKey& key,
                                     const ShapeResultFunc& shape_result_func) {
    if (map.size() >= kMaxSize) [[unlikely]] {
      const auto it = map.find(key);
      if (it != map.end() && it->value) {
        FontPerformance::AddShapeCacheHit();
        return it->value.Get();
      }
      FontPerformance::AddShapeCacheMiss();
      return shape_result_func().shape_result;
    }

    const auto add_result = map.insert(key, nullptr);
    if (const auto* cached_result = add_result.stored_value->value.Get()) {
      FontPerformance::AddShapeCacheHit();
      // Verify that the cache is consistent.
#if EXPENSIVE_DCHECKS_ARE_ON()
      const auto [other_shape_result, can_cache_other] = shape_result_func();
      const bool has_private_or_non_characters =
          !key.GetText().Is8Bit() &&
          std::ranges::any_of(key.GetText().Span16(), [](UChar32 c) {
            return Character::IsPrivateUse(c) || Character::IsNonCharacter(c);
          });
      // The shape-result call might try and reuse previous shape-results, we
      // can't check for equality in this case.
      //
      // TODO(crbug.com/486945341): We currently incorrectly cache shape-results
      // which contain PUA or non-characters.
      //
      // Specifically `FontCache::FallbackFontForCharacter` has different
      // fallback logic for these characters; for the same primary-font, and
      // different fallback lists we may produce two different shape-results.
      //
      // We should avoid the cache for this case.
      if (can_cache_other && !has_private_or_non_characters) {
        DCHECK_EQ(*cached_result, *other_shape_result);
      }
#endif
      return cached_result;
    }

    FontPerformance::AddShapeCacheMiss();
    const auto [shape_result, can_cache] = shape_result_func();

    // Only shape-results without font-fallback are valid, because the cache is
    // in the `primary_font_`.
    if (can_cache && !shape_result->HasFallbackFonts(primary_font_)) {
      add_result.stored_value->value = shape_result;
    }

    return shape_result;
  }

  WeakMap weak_map_;
  StrongMap strong_map_;
  Member<const SimpleFontData> primary_font_;

  std::unique_ptr<MemoryConsumerRegistration> memory_consumer_registration_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_NG_SHAPE_CACHE_H_
