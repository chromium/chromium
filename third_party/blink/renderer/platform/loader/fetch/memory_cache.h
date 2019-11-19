/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style
   sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_MEMORY_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_MEMORY_CACHE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/memory_cache_dump_provider.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class KURL;

// Member<MemoryCacheEntry> + MemoryCacheEntry::clearResourceWeak() monitors
// eviction from MemoryCache due to Resource garbage collection.
// WeakMember<Resource> + Resource's prefinalizer cannot determine whether the
// Resource was on MemoryCache or not, because WeakMember is already cleared
// when the prefinalizer is executed.
class MemoryCacheEntry final : public GarbageCollected<MemoryCacheEntry> {
 public:
  explicit MemoryCacheEntry(Resource* resource) : resource_(resource) {}

  void Trace(blink::Visitor*);
  Resource* GetResource() const { return resource_; }

 private:
  void ClearResourceWeak(const WeakCallbackInfo&);

  UntracedMember<Resource> resource_;
};

// This cache holds subresources used by Web pages: images, scripts,
// stylesheets, etc.
class PLATFORM_EXPORT MemoryCache final : public GarbageCollected<MemoryCache>,
                                          public MemoryCacheDumpClient,
                                          public MemoryPressureListener {
  USING_GARBAGE_COLLECTED_MIXIN(MemoryCache);

 public:
  explicit MemoryCache(scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~MemoryCache() override;

  void Trace(blink::Visitor*) override;

  struct TypeStatistic {
    STACK_ALLOCATED();

   public:
    size_t count;
    size_t size;
    size_t decoded_size;
    size_t encoded_size;
    size_t overhead_size;
    size_t code_cache_size;
    size_t encoded_size_duplicated_in_data_urls;

    TypeStatistic()
        : count(0),
          size(0),
          decoded_size(0),
          encoded_size(0),
          overhead_size(0),
          code_cache_size(0),
          encoded_size_duplicated_in_data_urls(0) {}

    void AddResource(Resource*);
  };

  struct Statistics {
    STACK_ALLOCATED();

   public:
    TypeStatistic images;
    TypeStatistic css_style_sheets;
    TypeStatistic scripts;
    TypeStatistic xsl_style_sheets;
    TypeStatistic fonts;
    TypeStatistic other;
  };

  Resource* ResourceForURL(const KURL&) const;
  Resource* ResourceForURL(const KURL&, const String& cache_identifier) const;
  HeapVector<Member<Resource>> ResourcesForURL(const KURL&) const;

  void Add(Resource*);
  void Remove(Resource*);
  bool Contains(const Resource*) const;

  static KURL RemoveFragmentIdentifierIfNeeded(const KURL& original_url);

  static String DefaultCacheIdentifier();

  // Sets the cache's memory capacities, in bytes. These will hold only
  // approximately, since the decoded cost of resources like scripts and
  // stylesheets is not known.
  //  - totalBytes: The maximum number of bytes that the cache should consume
  //    overall.
  void SetCapacity(size_t total_bytes);
  void SetDelayBeforeLiveDecodedPrune(base::TimeDelta seconds) {
    delay_before_live_decoded_prune_ = seconds;
  }

  void EvictResources();

  void Prune();

  // Called to update MemoryCache::size().
  void Update(Resource*, size_t old_size, size_t new_size);

  void RemoveURLFromCache(const KURL&);

  Statistics GetStatistics() const;

  size_t Capacity() const { return capacity_; }
  size_t size() const { return size_; }

  void PruneAll();

  void UpdateFramePaintTimestamp();

  // Take memory usage snapshot for tracing.
  bool OnMemoryDump(WebMemoryDumpLevelOfDetail, WebProcessMemoryDump*) override;

  void OnMemoryPressure(WebMemoryPressureLevel) override;

 private:
  enum PruneStrategy {
    // Automatically decide how much to prune.
    kAutomaticPrune,
    // Maximally prune resources.
    kMaximalPrune
  };

  // A URL-based map of all resources that are in the cache (including the
  // freshest version of objects that are currently being referenced by a Web
  // page). removeFragmentIdentifierIfNeeded() should be called for the url
  // before using it as a key for the map.
  using ResourceMap = HeapHashMap<String, Member<MemoryCacheEntry>>;
  using ResourceMapIndex = HeapHashMap<String, Member<ResourceMap>>;
  ResourceMap* EnsureResourceMap(const String& cache_identifier);
  ResourceMapIndex resource_maps_;

  void AddInternal(ResourceMap*, MemoryCacheEntry*);
  void RemoveInternal(ResourceMap*, const ResourceMap::iterator&);

  void PruneResources(PruneStrategy);
  void PruneNow(PruneStrategy);

  bool in_prune_resources_;
  bool prune_pending_;
  base::TimeDelta max_prune_deferral_delay_;
  base::TimeTicks prune_time_stamp_;
  base::TimeTicks prune_frame_time_stamp_;
  base::TimeTicks last_frame_paint_time_stamp_;  // used for detecting decoded
                                                 // resource thrash in the cache

  size_t capacity_;
  base::TimeDelta delay_before_live_decoded_prune_;

  // The number of bytes currently consumed by resources in the cache.
  size_t size_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  friend class MemoryCacheTest;

  DISALLOW_COPY_AND_ASSIGN(MemoryCache);
};

// Returns the global cache.
PLATFORM_EXPORT MemoryCache* GetMemoryCache();

// Sets the global cache, used to swap in a test instance. Returns the old
// MemoryCache object.
PLATFORM_EXPORT MemoryCache* ReplaceMemoryCacheForTesting(MemoryCache*);

}  // namespace blink

#endif
