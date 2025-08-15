/*
    Copyright (C) 2005 Apple Inc. All rights reserved.

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
*/

#include "third_party/blink/renderer/platform/wtf/hash_table.h"

#if DUMP_HASHTABLE_STATS || DUMP_HASHTABLE_STATS_PER_TABLE

#include <iomanip>

#include "base/synchronization/lock.h"

namespace blink {

static base::Lock& HashTableStatsLock() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(base::Lock, lock, ());
  return lock;
}

HashTableStats& HashTableStats::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashTableStats, stats, ());
  return stats;
}

void HashTableStats::Copy(const HashTableStats& other) {
  num_accesses = other.num_accesses.load(std::memory_order_relaxed);
  num_rehashes = other.num_rehashes.load(std::memory_order_relaxed);
  num_removes = other.num_removes.load(std::memory_order_relaxed);
  num_reinserts = other.num_reinserts.load(std::memory_order_relaxed);

  max_collisions = other.max_collisions;
  num_collisions = other.num_collisions;
  UNSAFE_TODO(memcpy(collision_graph.data(), other.collision_graph.data(),
                     sizeof(collision_graph)));
}

void HashTableStats::RecordCollisionAtCount(int count) {
  // The global hash table singleton needs to be atomically updated.
  if (this == &Instance()) {
    base::AutoLock locker(HashTableStatsLock());
    RecordCollisionAtCountWithoutLock(count);
  } else {
    RecordCollisionAtCountWithoutLock(count);
  }
}

void HashTableStats::RecordCollisionAtCountWithoutLock(int count) {
  if (count > max_collisions) {
    max_collisions = count;
  }
  ++num_collisions;
  ++collision_graph[count];
}

void HashTableStats::DumpStats() {
  // Lock the global hash table singleton while dumping.
  if (this == &Instance()) {
    base::AutoLock locker(HashTableStatsLock());
    DumpStatsWithoutLock();
  } else {
    DumpStatsWithoutLock();
  }
}

void HashTableStats::DumpStatsWithoutLock() {
  std::stringstream collision_str;
  collision_str << std::fixed << std::setprecision(2);
  for (int i = 1; i <= max_collisions; ++i) {
    collision_str << "      " << collision_graph[i] << " lookups with exactly "
                  << i << " collisions ("
                  << (100.0 * (collision_graph[i] - collision_graph[i + 1]) /
                      num_accesses)
                  << "% , " << (100.0 * collision_graph[i] / num_accesses)
                  << "% with this many or more)\n";
  }

  DLOG(INFO) << std::fixed << std::setprecision(2)
             << "blink::HashTable statistics:\n"
             << "    " << num_accesses << " accesses\n"
             << "    " << num_collisions << " total collisions, average "
             << (1.0 * (num_accesses + num_collisions) / num_accesses)
             << " probes per access\n"
             << "    longest collision chain: " << max_collisions << "\n"
             << collision_str.str() << "    " << num_rehashes << " rehashes\n"
             << "    " << num_reinserts << " reinserts";
}

}  // namespace blink

#endif
