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

namespace WTF {

static base::Lock& HashTableStatsLock() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(base::Lock, lock, ());
  return lock;
}

HashTableStats& HashTableStats::instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashTableStats, stats, ());
  return stats;
}

void HashTableStats::copy(const HashTableStats* other) {
  numAccesses = other->numAccesses.load(std::memory_order_relaxed);
  numRehashes = other->numRehashes.load(std::memory_order_relaxed);
  numRemoves = other->numRemoves.load(std::memory_order_relaxed);
  numReinserts = other->numReinserts.load(std::memory_order_relaxed);

  maxCollisions = other->maxCollisions;
  numCollisions = other->numCollisions;
  memcpy(collisionGraph, other->collisionGraph, sizeof(collisionGraph));
}

void HashTableStats::recordCollisionAtCount(int count) {
  // The global hash table singleton needs to be atomically updated.
  if (this == &instance()) {
    base::AutoLock locker(HashTableStatsLock());
    RecordCollisionAtCountWithoutLock(count);
  } else {
    RecordCollisionAtCountWithoutLock(count);
  }
}

void HashTableStats::RecordCollisionAtCountWithoutLock(int count) {
  if (count > maxCollisions)
    maxCollisions = count;
  numCollisions++;
  collisionGraph[count]++;
}

void HashTableStats::DumpStats() {
  // Lock the global hash table singleton while dumping.
  if (this == &instance()) {
    base::AutoLock locker(HashTableStatsLock());
    DumpStatsWithoutLock();
  } else {
    DumpStatsWithoutLock();
  }
}

void HashTableStats::DumpStatsWithoutLock() {
  std::stringstream collision_str;
  collision_str << std::fixed << std::setprecision(2);
  for (int i = 1; i <= maxCollisions; i++) {
    collision_str << "      " << collisionGraph[i] << " lookups with exactly "
                  << i << " collisions ("
                  << (100.0 * (collisionGraph[i] - collisionGraph[i + 1]) /
                      numAccesses)
                  << "% , " << (100.0 * collisionGraph[i] / numAccesses)
                  << "% with this many or more)\n";
  }

  DLOG(INFO) << std::fixed << std::setprecision(2)
             << "WTF::HashTable statistics:\n"
             << "    " << numAccesses << " accesses\n"
             << "    " << numCollisions << " total collisions, average "
             << (1.0 * (numAccesses + numCollisions) / numAccesses)
             << " probes per access\n"
             << "    longest collision chain: " << maxCollisions << "\n"
             << collision_str.str() << "    " << numRehashes << " rehashes\n"
             << "    " << numReinserts << " reinserts";
}

}  // namespace WTF

#endif
