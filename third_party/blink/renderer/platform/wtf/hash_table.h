/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 David Levin <levin@chromium.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_TABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_TABLE_H_

#include <memory>

#include "base/bits.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/conditional_destructor.h"
#include "third_party/blink/renderer/platform/wtf/construct_traits.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

#if !defined(DUMP_HASHTABLE_STATS)
#define DUMP_HASHTABLE_STATS 0
#endif

#if !defined(DUMP_HASHTABLE_STATS_PER_TABLE)
#define DUMP_HASHTABLE_STATS_PER_TABLE 0
#endif

#if DUMP_HASHTABLE_STATS
#include "third_party/blink/renderer/platform/wtf/threading.h"
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
#include <type_traits>
#include "third_party/blink/renderer/platform/wtf/DataLog.h"
#endif

#if DUMP_HASHTABLE_STATS
#if DUMP_HASHTABLE_STATS_PER_TABLE

#define UPDATE_PROBE_COUNTS()                                    \
  ++probeCount;                                                  \
  HashTableStats::instance().recordCollisionAtCount(probeCount); \
  ++perTableProbeCount;                                          \
  stats_->recordCollisionAtCount(perTableProbeCount)
#define UPDATE_ACCESS_COUNTS()                                                 \
  HashTableStats::instance().numAccesses.fetch_add(1,                          \
                                                   std::memory_order_relaxed); \
  int probeCount = 0;                                                          \
  stats_->numAccesses.fetch_add(1, std::memory_order_relaxed);                 \
  int perTableProbeCount = 0
#else
#define UPDATE_PROBE_COUNTS() \
  ++probeCount;               \
  HashTableStats::instance().recordCollisionAtCount(probeCount)
#define UPDATE_ACCESS_COUNTS()                                                 \
  HashTableStats::instance().numAccesses.fetch_add(1,                          \
                                                   std::memory_order_relaxed); \
  int probeCount = 0
#endif
#else
#if DUMP_HASHTABLE_STATS_PER_TABLE
#define UPDATE_PROBE_COUNTS() \
  ++perTableProbeCount;       \
  stats_->recordCollisionAtCount(perTableProbeCount)
#define UPDATE_ACCESS_COUNTS()                                 \
  stats_->numAccesses.fetch_add(1, std::memory_order_relaxed); \
  int perTableProbeCount = 0
#else
#define UPDATE_PROBE_COUNTS() \
  do {                        \
  } while (0)
#define UPDATE_ACCESS_COUNTS() \
  do {                         \
  } while (0)
#endif
#endif

namespace WTF {

// This is for tracing inside collections that have special support for weak
// pointers.
//
// Structure:
// - |Trace|: Traces the contents and returns true if there are still unmarked
//   objects left to process
//
// Default implementation for non-weak types is to use the regular non-weak
// TraceTrait. Default implementation for types with weakness is to
// call |TraceInCollection| on the type's trait.
template <WeakHandlingFlag weakness, typename T, typename Traits>
struct TraceInCollectionTrait;

#if DUMP_HASHTABLE_STATS || DUMP_HASHTABLE_STATS_PER_TABLE
struct WTF_EXPORT HashTableStats {
  HashTableStats()
      : numAccesses(0),
        numRehashes(0),
        numRemoves(0),
        numReinserts(0),
        maxCollisions(0),
        numCollisions(0),
        collisionGraph() {}

  // The following variables are all atomically incremented when modified.
  std::atomic_int numAccesses;
  std::atomic_int numRehashes;
  std::atomic_int numRemoves;
  std::atomic_int numReinserts;

  // The following variables are only modified in the recordCollisionAtCount
  // method within a mutex.
  int maxCollisions;
  int numCollisions;
  int collisionGraph[4096];

  void copy(const HashTableStats* other);
  void recordCollisionAtCount(int count);
  void DumpStats();

  static HashTableStats& instance();

  template <typename VisitorDispatcher>
  void trace(VisitorDispatcher) const {}

 private:
  void RecordCollisionAtCountWithoutLock(int count);
  void DumpStatsWithoutLock();
};

#if DUMP_HASHTABLE_STATS_PER_TABLE
template <typename Allocator, bool isGCType = Allocator::kIsGarbageCollected>
class HashTableStatsPtr;

template <typename Allocator>
class HashTableStatsPtr<Allocator, false> final {
  STATIC_ONLY(HashTableStatsPtr);

 public:
  static std::unique_ptr<HashTableStats> Create() {
    return std::make_unique<HashTableStats>();
  }

  static std::unique_ptr<HashTableStats> copy(
      const std::unique_ptr<HashTableStats>& other) {
    if (!other)
      return nullptr;
    return std::make_unique<HashTableStats>(*other);
  }

  static void swap(std::unique_ptr<HashTableStats>& stats,
                   std::unique_ptr<HashTableStats>& other) {
    stats.swap(other);
  }
};

template <typename Allocator>
class HashTableStatsPtr<Allocator, true> final {
  STATIC_ONLY(HashTableStatsPtr);

 public:
  static HashTableStats* Create() {
    // TODO(cavalcantii): fix this.
    return new HashTableStats;
  }

  static HashTableStats* copy(const HashTableStats* other) {
    if (!other)
      return nullptr;
    HashTableStats* obj = Create();
    obj->copy(other);
    return obj;
  }

  static void swap(HashTableStats*& stats, HashTableStats*& other) {
    std::swap(stats, other);
  }
};
#endif
#endif

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTable;
template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTableIterator;
template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTableConstIterator;
template <WeakHandlingFlag x,
          typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y,
          typename Z>
struct WeakProcessingHashTableHelper;

typedef enum { kHashItemKnownGood } HashItemKnownGoodTag;

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTableConstIterator final {
  DISALLOW_NEW();

 private:
  typedef HashTable<Key,
                    Value,
                    Extractor,
                    HashFunctions,
                    Traits,
                    KeyTraits,
                    Allocator>
      HashTableType;
  typedef HashTableIterator<Key,
                            Value,
                            Extractor,
                            HashFunctions,
                            Traits,
                            KeyTraits,
                            Allocator>
      iterator;
  typedef HashTableConstIterator<Key,
                                 Value,
                                 Extractor,
                                 HashFunctions,
                                 Traits,
                                 KeyTraits,
                                 Allocator>
      const_iterator;
  typedef Value ValueType;
  using value_type = ValueType;
  typedef typename Traits::IteratorConstGetType GetType;
  typedef const ValueType* PointerType;

  friend class HashTable<Key,
                         Value,
                         Extractor,
                         HashFunctions,
                         Traits,
                         KeyTraits,
                         Allocator>;
  friend class HashTableIterator<Key,
                                 Value,
                                 Extractor,
                                 HashFunctions,
                                 Traits,
                                 KeyTraits,
                                 Allocator>;

  void SkipEmptyBuckets() {
    while (position_ != end_position_ &&
           HashTableType::IsEmptyOrDeletedBucket(*position_))
      ++position_;
  }

  void ReverseSkipEmptyBuckets() {
    // Don't need to check for out-of-bounds positions, as begin position is
    // always going to be a non-empty bucket.
    while (HashTableType::IsEmptyOrDeletedBucket(*position_)) {
#if DCHECK_IS_ON()
      DCHECK_NE(position_, begin_position_);
#endif
      --position_;
    }
  }

  HashTableConstIterator(PointerType position,
                         PointerType begin_position,
                         PointerType end_position,
                         const HashTableType* container)
      : position_(position),
        end_position_(end_position)
#if DCHECK_IS_ON()
        ,
        begin_position_(begin_position),
        container_(container),
        container_modifications_(container->Modifications())
#endif
  {
    SkipEmptyBuckets();
  }

  HashTableConstIterator(PointerType position,
                         PointerType begin_position,
                         PointerType end_position,
                         const HashTableType* container,
                         HashItemKnownGoodTag)
      : position_(position),
        end_position_(end_position)
#if DCHECK_IS_ON()
        ,
        begin_position_(begin_position),
        container_(container),
        container_modifications_(container->Modifications())
#endif
  {
#if DCHECK_IS_ON()
    DCHECK_EQ(container_modifications_, container_->Modifications());
#endif
  }

  void CheckModifications() const {
#if DCHECK_IS_ON()
    // HashTable and collections that build on it do not support
    // modifications while there is an iterator in use. The exception is
    // ListHashSet, which has its own iterators that tolerate modification
    // of the underlying set.
    DCHECK_EQ(container_modifications_, container_->Modifications());
    DCHECK(!container_->AccessForbidden());
#endif
  }

 public:
  HashTableConstIterator() = default;

  GetType Get() const {
    CheckModifications();
    return position_;
  }
  typename Traits::IteratorConstReferenceType operator*() const {
    return Traits::GetToReferenceConstConversion(Get());
  }
  GetType operator->() const { return Get(); }

  const_iterator& operator++() {
    DCHECK_NE(position_, end_position_);
    CheckModifications();
    ++position_;
    SkipEmptyBuckets();
    return *this;
  }

  // postfix ++ intentionally omitted

  const_iterator& operator--() {
#if DCHECK_IS_ON()
    DCHECK_NE(position_, begin_position_);
#endif
    CheckModifications();
    --position_;
    ReverseSkipEmptyBuckets();
    return *this;
  }

  // postfix -- intentionally omitted

  // Comparison.
  bool operator==(const const_iterator& other) const {
    return position_ == other.position_;
  }
  bool operator!=(const const_iterator& other) const {
    return position_ != other.position_;
  }
  bool operator==(const iterator& other) const {
    return *this == static_cast<const_iterator>(other);
  }
  bool operator!=(const iterator& other) const {
    return *this != static_cast<const_iterator>(other);
  }

  std::ostream& PrintTo(std::ostream& stream) const {
    if (position_ == end_position_)
      return stream << "iterator representing <end>";
    // TODO(tkent): Change |position_| to |*position_| to show the
    // pointed object. It requires a lot of new stream printer functions.
    return stream << "iterator pointing to " << position_;
  }

 private:
  PointerType position_;
  PointerType end_position_;
#if DCHECK_IS_ON()
  PointerType begin_position_;
  const HashTableType* container_;
  int64_t container_modifications_;
#endif
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename Hash,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
std::ostream& operator<<(std::ostream& stream,
                         const HashTableConstIterator<Key,
                                                      Value,
                                                      Extractor,
                                                      Hash,
                                                      Traits,
                                                      KeyTraits,
                                                      Allocator>& iterator) {
  return iterator.PrintTo(stream);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTableIterator final {
  DISALLOW_NEW();

 private:
  typedef HashTable<Key,
                    Value,
                    Extractor,
                    HashFunctions,
                    Traits,
                    KeyTraits,
                    Allocator>
      HashTableType;
  typedef HashTableIterator<Key,
                            Value,
                            Extractor,
                            HashFunctions,
                            Traits,
                            KeyTraits,
                            Allocator>
      iterator;
  typedef HashTableConstIterator<Key,
                                 Value,
                                 Extractor,
                                 HashFunctions,
                                 Traits,
                                 KeyTraits,
                                 Allocator>
      const_iterator;
  typedef Value ValueType;
  typedef typename Traits::IteratorGetType GetType;
  typedef ValueType* PointerType;

  friend class HashTable<Key,
                         Value,
                         Extractor,
                         HashFunctions,
                         Traits,
                         KeyTraits,
                         Allocator>;

  HashTableIterator(PointerType pos,
                    PointerType begin,
                    PointerType end,
                    const HashTableType* container)
      : iterator_(pos, begin, end, container) {}
  HashTableIterator(PointerType pos,
                    PointerType begin,
                    PointerType end,
                    const HashTableType* container,
                    HashItemKnownGoodTag tag)
      : iterator_(pos, begin, end, container, tag) {}

 public:
  HashTableIterator() = default;

  // default copy, assignment and destructor are OK

  GetType Get() const { return const_cast<GetType>(iterator_.Get()); }
  typename Traits::IteratorReferenceType operator*() const {
    return Traits::GetToReferenceConversion(Get());
  }
  GetType operator->() const { return Get(); }

  iterator& operator++() {
    ++iterator_;
    return *this;
  }

  // postfix ++ intentionally omitted

  iterator& operator--() {
    --iterator_;
    return *this;
  }

  // postfix -- intentionally omitted

  // Comparison.
  bool operator==(const iterator& other) const {
    return iterator_ == other.iterator_;
  }
  bool operator!=(const iterator& other) const {
    return iterator_ != other.iterator_;
  }
  bool operator==(const const_iterator& other) const {
    return iterator_ == other;
  }
  bool operator!=(const const_iterator& other) const {
    return iterator_ != other;
  }

  operator const_iterator() const { return iterator_; }
  std::ostream& PrintTo(std::ostream& stream) const {
    return iterator_.PrintTo(stream);
  }

 private:
  const_iterator iterator_;
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename Hash,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
std::ostream& operator<<(std::ostream& stream,
                         const HashTableIterator<Key,
                                                 Value,
                                                 Extractor,
                                                 Hash,
                                                 Traits,
                                                 KeyTraits,
                                                 Allocator>& iterator) {
  return iterator.PrintTo(stream);
}

using std::swap;

template <typename T,
          typename Allocator,
          typename Traits,
          bool enterGCForbiddenScope>
struct Mover {
  STATIC_ONLY(Mover);
  static void Move(T&& from, T& to) {
    to.~T();
    new (NotNull, &to) T(std::move(from));
  }
};

template <typename T, typename Allocator, typename Traits>
struct Mover<T, Allocator, Traits, true> {
  STATIC_ONLY(Mover);
  static void Move(T&& from, T& to) {
    Allocator::EnterGCForbiddenScope();
    to.~T();
    new (NotNull, &to) T(std::move(from));
    Allocator::LeaveGCForbiddenScope();
  }
};

template <typename HashFunctions, typename Traits, typename Allocator>
class IdentityHashTranslator {
  STATIC_ONLY(IdentityHashTranslator);

 public:
  template <typename T>
  static unsigned GetHash(const T& key) {
    return HashFunctions::GetHash(key);
  }
  template <typename T, typename U>
  static bool Equal(const T& a, const U& b) {
    return HashFunctions::Equal(a, b);
  }
  template <typename T, typename U, typename V>
  static void Translate(T& location, U&&, V&& value) {
    location = std::forward<V>(value);
  }
};

template <typename HashTableType, typename ValueType>
struct HashTableAddResult final {
  STACK_ALLOCATED();

 public:
  HashTableAddResult(const HashTableType* container,
                     ValueType* stored_value,
                     bool is_new_entry)
      : stored_value(stored_value),
        is_new_entry(is_new_entry)
#if ENABLE_SECURITY_ASSERT
        ,
        container_(container),
        container_modifications_(container->Modifications())
#endif
  {
    ALLOW_UNUSED_LOCAL(container);
    DCHECK(container);
  }

  ValueType* stored_value;
  bool is_new_entry;

#if ENABLE_SECURITY_ASSERT
  ~HashTableAddResult() {
    // If rehash happened before accessing storedValue, it's
    // use-after-free. Any modification may cause a rehash, so we check for
    // modifications here.

    // Rehash after accessing storedValue is harmless but will assert if the
    // AddResult destructor takes place after a modification. You may need
    // to limit the scope of the AddResult.
    SECURITY_DCHECK(container_modifications_ == container_->Modifications());
  }

 private:
  const HashTableType* container_;
  const int64_t container_modifications_;
#endif
};

template <typename Value, typename Extractor, typename KeyTraits>
struct HashTableHelper {
  template <typename T>
  struct AddConstToPtrType {
    using type = T;
  };
  template <typename T>
  struct AddConstToPtrType<T*> {
    using type = const T*;
  };

  using Key = typename AddConstToPtrType<typename KeyTraits::TraitType>::type;

  STATIC_ONLY(HashTableHelper);
  static bool IsEmptyBucket(const Key& key) {
    return IsHashTraitsEmptyValue<KeyTraits>(key);
  }
  static bool IsDeletedBucket(const Key& key) {
    return KeyTraits::IsDeletedValue(key);
  }
  static bool IsEmptyOrDeletedBucketForKey(const Key& key) {
    return IsEmptyBucket(key) || IsDeletedBucket(key);
  }
  static bool IsEmptyOrDeletedBucket(const Value& value) {
    const Key& key = Extractor::Extract(value);
    return IsEmptyOrDeletedBucketForKey(key);
  }
};

template <typename HashTranslator,
          typename KeyTraits,
          bool safeToCompareToEmptyOrDeleted>
struct HashTableKeyChecker {
  STATIC_ONLY(HashTableKeyChecker);
  // There's no simple generic way to make this check if
  // safeToCompareToEmptyOrDeleted is false, so the check always passes.
  template <typename T>
  static bool CheckKey(const T&) {
    return true;
  }
};

template <typename HashTranslator, typename KeyTraits>
struct HashTableKeyChecker<HashTranslator, KeyTraits, true> {
  STATIC_ONLY(HashTableKeyChecker);
  template <typename T>
  static bool CheckKey(const T& key) {
    // FIXME : Check also equality to the deleted value.
    return !HashTranslator::Equal(KeyTraits::EmptyValue(), key);
  }
};

// Note: empty or deleted key values are not allowed, using them may lead to
// undefined behavior.  For pointer keys this means that null pointers are not
// allowed unless you supply custom key traits.
template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTable final
    : public ConditionalDestructor<HashTable<Key,
                                             Value,
                                             Extractor,
                                             HashFunctions,
                                             Traits,
                                             KeyTraits,
                                             Allocator>,
                                   Allocator::kIsGarbageCollected> {
  DISALLOW_NEW();

 public:
  typedef HashTableIterator<Key,
                            Value,
                            Extractor,
                            HashFunctions,
                            Traits,
                            KeyTraits,
                            Allocator>
      iterator;
  typedef HashTableConstIterator<Key,
                                 Value,
                                 Extractor,
                                 HashFunctions,
                                 Traits,
                                 KeyTraits,
                                 Allocator>
      const_iterator;
  typedef Traits ValueTraits;
  typedef Key KeyType;
  typedef typename KeyTraits::PeekInType KeyPeekInType;
  typedef Value ValueType;
  typedef Extractor ExtractorType;
  typedef KeyTraits KeyTraitsType;
  typedef IdentityHashTranslator<HashFunctions, Traits, Allocator>
      IdentityTranslatorType;
  typedef HashTableAddResult<HashTable, ValueType> AddResult;

  HashTable();

  void Finalize() {
    static_assert(!Allocator::kIsGarbageCollected,
                  "GCed collections can't be finalized.");
    if (LIKELY(!table_))
      return;
    EnterAccessForbiddenScope();
    DeleteAllBucketsAndDeallocate(table_, table_size_);
    LeaveAccessForbiddenScope();
    table_ = nullptr;
  }

  HashTable(const HashTable&);
  HashTable(HashTable&&);
  void swap(HashTable&);
  HashTable& operator=(const HashTable&);
  HashTable& operator=(HashTable&&);

  // When the hash table is empty, just return the same iterator for end as
  // for begin.  This is more efficient because we don't have to skip all the
  // empty and deleted buckets, and iterating an empty table is a common case
  // that's worth optimizing.
  iterator begin() { return IsEmpty() ? end() : MakeIterator(table_); }
  iterator end() { return MakeKnownGoodIterator(table_ + table_size_); }
  const_iterator begin() const {
    return IsEmpty() ? end() : MakeConstIterator(table_);
  }
  const_iterator end() const {
    return MakeKnownGoodConstIterator(table_ + table_size_);
  }

  unsigned size() const {
    DCHECK(!AccessForbidden());
    return key_count_;
  }
  unsigned Capacity() const {
    DCHECK(!AccessForbidden());
    return table_size_;
  }
  bool IsEmpty() const {
    DCHECK(!AccessForbidden());
    return !key_count_;
  }

  void ReserveCapacityForSize(unsigned size);

  template <typename IncomingValueType>
  AddResult insert(IncomingValueType&& value) {
    return insert<IdentityTranslatorType>(
        Extractor::Extract(value), std::forward<IncomingValueType>(value));
  }

  // A special version of insert() that finds the object by hashing and
  // comparing with some other type, to avoid the cost of type conversion if the
  // object is already in the table.
  template <typename HashTranslator, typename T, typename Extra>
  AddResult insert(T&& key, Extra&&);
  template <typename HashTranslator, typename T, typename Extra>
  AddResult InsertPassingHashCode(T&& key, Extra&&);

  iterator find(KeyPeekInType key) { return Find<IdentityTranslatorType>(key); }
  const_iterator find(KeyPeekInType key) const {
    return Find<IdentityTranslatorType>(key);
  }
  bool Contains(KeyPeekInType key) const {
    return Contains<IdentityTranslatorType>(key);
  }

  template <typename HashTranslator, typename T>
  iterator Find(const T&);
  template <typename HashTranslator, typename T>
  const_iterator Find(const T&) const;
  template <typename HashTranslator, typename T>
  bool Contains(const T&) const;

  void erase(KeyPeekInType);
  void erase(iterator);
  void erase(const_iterator);
  void clear();

  static bool IsEmptyBucket(const ValueType& value) {
    return IsHashTraitsEmptyValue<KeyTraits>(Extractor::Extract(value));
  }
  static bool IsDeletedBucket(const ValueType& value) {
    return KeyTraits::IsDeletedValue(Extractor::Extract(value));
  }
  static bool IsEmptyOrDeletedBucket(const ValueType& value) {
    return HashTableHelper<ValueType, Extractor,
                           KeyTraits>::IsEmptyOrDeletedBucket(value);
  }

  ValueType* Lookup(KeyPeekInType key) {
    return Lookup<IdentityTranslatorType, KeyPeekInType>(key);
  }
  const ValueType* Lookup(KeyPeekInType key) const {
    return Lookup<IdentityTranslatorType, KeyPeekInType>(key);
  }
  template <typename HashTranslator, typename T>
  ValueType* Lookup(const T&);
  template <typename HashTranslator, typename T>
  const ValueType* Lookup(const T&) const;

  ValueType** GetBufferSlot() { return &table_; }

  template <typename VisitorDispatcher, typename A = Allocator>
  std::enable_if_t<A::kIsGarbageCollected> Trace(VisitorDispatcher) const;

#if DCHECK_IS_ON()
  void EnterAccessForbiddenScope() {
    DCHECK(!access_forbidden_);
    access_forbidden_ = true;
  }
  void LeaveAccessForbiddenScope() { access_forbidden_ = false; }
  bool AccessForbidden() const { return access_forbidden_; }
  int64_t Modifications() const { return modifications_; }
  void RegisterModification() { modifications_++; }
  // HashTable and collections that build on it do not support modifications
  // while there is an iterator in use. The exception is ListHashSet, which
  // has its own iterators that tolerate modification of the underlying set.
  void CheckModifications(int64_t mods) const {
    DCHECK_EQ(mods, modifications_);
  }
#else
  ALWAYS_INLINE void EnterAccessForbiddenScope() {}
  ALWAYS_INLINE void LeaveAccessForbiddenScope() {}
  ALWAYS_INLINE bool AccessForbidden() const { return false; }
  ALWAYS_INLINE int64_t Modifications() const { return 0; }
  ALWAYS_INLINE void RegisterModification() {}
  ALWAYS_INLINE void CheckModifications(int64_t mods) const {}
#endif

 protected:
  template <typename VisitorDispatcher, typename A = Allocator>
  std::enable_if_t<A::kIsGarbageCollected> TraceTable(
      VisitorDispatcher,
      const ValueType* table) const;

 private:
  static ValueType* AllocateTable(unsigned size);
  static void DeleteAllBucketsAndDeallocate(ValueType* table, unsigned size);

  typedef std::pair<ValueType*, bool> LookupType;
  typedef std::pair<LookupType, unsigned> FullLookupType;

  LookupType LookupForWriting(const Key& key) {
    return LookupForWriting<IdentityTranslatorType>(key);
  }
  template <typename HashTranslator, typename T>
  FullLookupType FullLookupForWriting(const T&);
  template <typename HashTranslator, typename T>
  LookupType LookupForWriting(const T&);

  void erase(const ValueType*);

  bool ShouldExpand() const {
    return (key_count_ + deleted_count_) * kMaxLoad >= table_size_;
  }
  bool MustRehashInPlace() const {
    return key_count_ * kMinLoad < table_size_ * 2;
  }
  bool ShouldShrink() const {
    // isAllocationAllowed check should be at the last because it's
    // expensive.
    return key_count_ * kMinLoad < table_size_ &&
           table_size_ > KeyTraits::kMinimumTableSize &&
           Allocator::IsAllocationAllowed();
  }
  ValueType* Expand(ValueType* entry = nullptr);
  void Shrink() { Rehash(table_size_ / 2, nullptr); }

  ValueType* ExpandBuffer(unsigned new_table_size, ValueType* entry, bool&);
  ValueType* RehashTo(ValueType* new_table,
                      unsigned new_table_size,
                      ValueType* entry);
  ValueType* Rehash(unsigned new_table_size, ValueType* entry);
  ValueType* Reinsert(ValueType&&);

  static void InitializeBucket(ValueType& bucket);
  static void DeleteBucket(const ValueType& bucket) {
    bucket.~ValueType();
    Traits::ConstructDeletedValue(const_cast<ValueType&>(bucket),
                                  Allocator::kIsGarbageCollected);
  }

  FullLookupType MakeLookupResult(ValueType* position,
                                  bool found,
                                  unsigned hash) {
    return FullLookupType(LookupType(position, found), hash);
  }

  iterator MakeIterator(ValueType* pos) {
    return iterator(pos, table_, table_ + table_size_, this);
  }
  const_iterator MakeConstIterator(const ValueType* pos) const {
    return const_iterator(pos, table_, table_ + table_size_, this);
  }
  iterator MakeKnownGoodIterator(ValueType* pos) {
    return iterator(pos, table_, table_ + table_size_, this,
                    kHashItemKnownGood);
  }
  const_iterator MakeKnownGoodConstIterator(const ValueType* pos) const {
    return const_iterator(pos, table_, table_ + table_size_, this,
                          kHashItemKnownGood);
  }

  static const unsigned kMaxLoad = 2;
  static const unsigned kMinLoad = 6;

  unsigned TableSizeMask() const {
    unsigned mask = table_size_ - 1;
    DCHECK_EQ((mask & table_size_), 0u);
    return mask;
  }

  void SetEnqueued() { queue_flag_ = true; }
  void ClearEnqueued() { queue_flag_ = false; }
  bool Enqueued() { return queue_flag_; }

  // Constructor for hash tables with raw storage.
  struct RawStorageTag {};
  HashTable(RawStorageTag, ValueType* table, unsigned size)
      : table_(table),
        table_size_(size),
        key_count_(0),
        deleted_count_(0),
        queue_flag_(0)
#if DCHECK_IS_ON()
        ,
        access_forbidden_(0),
        modifications_(0)
#endif
  {
  }

  ValueType* table_;
  unsigned table_size_;
  unsigned key_count_;
#if DCHECK_IS_ON()
  unsigned deleted_count_ : 30;
  unsigned queue_flag_ : 1;
  unsigned access_forbidden_ : 1;
  unsigned modifications_;
#else
  unsigned deleted_count_ : 31;
  unsigned queue_flag_ : 1;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
 public:
  mutable
      typename std::conditional<Allocator::kIsGarbageCollected,
                                HashTableStats*,
                                std::unique_ptr<HashTableStats>>::type stats_;
  void DumpStats() {
    if (stats_) {
      stats_->DumpStats();
    }
  }
#endif

  template <WeakHandlingFlag x,
            typename T,
            typename U,
            typename V,
            typename W,
            typename X,
            typename Y,
            typename Z>
  friend struct WeakProcessingHashTableHelper;
  template <typename T, size_t, typename U, typename V>
  friend class ListHashSet;
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
inline HashTable<Key,
                 Value,
                 Extractor,
                 HashFunctions,
                 Traits,
                 KeyTraits,
                 Allocator>::HashTable()
    : table_(nullptr),
      table_size_(0),
      key_count_(0),
      deleted_count_(0),
      queue_flag_(false)
#if DCHECK_IS_ON()
      ,
      access_forbidden_(false),
      modifications_(0)
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
      ,
      stats_(nullptr)
#endif
{
  static_assert(Allocator::kIsGarbageCollected ||
                    (!IsPointerToGarbageCollectedType<Key>::value &&
                     !IsPointerToGarbageCollectedType<Value>::value),
                "Cannot put raw pointers to garbage-collected classes into an "
                "off-heap collection.");
}

inline unsigned DoubleHash(unsigned key) {
  key = ~key + (key >> 23);
  key ^= (key << 12);
  key ^= (key >> 7);
  key ^= (key << 2);
  key ^= (key >> 20);
  return key;
}

inline unsigned CalculateCapacity(unsigned size) {
  for (unsigned mask = size; mask; mask >>= 1)
    size |= mask;         // 00110101010 -> 00111111111
  return (size + 1) * 2;  // 00111111111 -> 10000000000
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
void HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::ReserveCapacityForSize(unsigned new_size) {
  unsigned new_capacity = CalculateCapacity(new_size);
  if (new_capacity < KeyTraits::kMinimumTableSize)
    new_capacity = KeyTraits::kMinimumTableSize;

  if (new_capacity > Capacity()) {
    CHECK(!static_cast<int>(
        new_capacity >>
        31));  // HashTable capacity should not overflow 32bit int.
    Rehash(new_capacity, nullptr);
  }
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    Lookup(const T& key) {
  // Call the const version of Lookup<HashTranslator, T>().
  return const_cast<Value*>(
      const_cast<const HashTable*>(this)->Lookup<HashTranslator>(key));
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline const Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    Lookup(const T& key) const {
  DCHECK(!AccessForbidden());
  DCHECK((HashTableKeyChecker<
          HashTranslator, KeyTraits,
          HashFunctions::safe_to_compare_to_empty_or_deleted>::CheckKey(key)));
  const ValueType* table = table_;
  if (!table)
    return nullptr;

  size_t k = 0;
  size_t size_mask = TableSizeMask();
  unsigned h = HashTranslator::GetHash(key);
  size_t i = h & size_mask;

  UPDATE_ACCESS_COUNTS();

  while (1) {
    const ValueType* entry = table + i;

    if (HashFunctions::safe_to_compare_to_empty_or_deleted) {
      if (HashTranslator::Equal(Extractor::Extract(*entry), key))
        return entry;

      if (IsEmptyBucket(*entry))
        return nullptr;
    } else {
      if (IsEmptyBucket(*entry))
        return nullptr;

      if (!IsDeletedBucket(*entry) &&
          HashTranslator::Equal(Extractor::Extract(*entry), key))
        return entry;
    }
    UPDATE_PROBE_COUNTS();
    if (!k)
      k = 1 | DoubleHash(h);
    i = (i + k) & size_mask;
  }
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline typename HashTable<Key,
                          Value,
                          Extractor,
                          HashFunctions,
                          Traits,
                          KeyTraits,
                          Allocator>::LookupType
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    LookupForWriting(const T& key) {
  DCHECK(!AccessForbidden());
  DCHECK(table_);
  RegisterModification();

  ValueType* table = table_;
  size_t k = 0;
  size_t size_mask = TableSizeMask();
  unsigned h = HashTranslator::GetHash(key);
  size_t i = h & size_mask;

  UPDATE_ACCESS_COUNTS();

  ValueType* deleted_entry = nullptr;

  while (1) {
    ValueType* entry = table + i;

    if (IsEmptyBucket(*entry))
      return LookupType(deleted_entry ? deleted_entry : entry, false);

    if (HashFunctions::safe_to_compare_to_empty_or_deleted) {
      if (HashTranslator::Equal(Extractor::Extract(*entry), key))
        return LookupType(entry, true);

      if (IsDeletedBucket(*entry))
        deleted_entry = entry;
    } else {
      if (IsDeletedBucket(*entry))
        deleted_entry = entry;
      else if (HashTranslator::Equal(Extractor::Extract(*entry), key))
        return LookupType(entry, true);
    }
    UPDATE_PROBE_COUNTS();
    if (!k)
      k = 1 | DoubleHash(h);
    i = (i + k) & size_mask;
  }
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline typename HashTable<Key,
                          Value,
                          Extractor,
                          HashFunctions,
                          Traits,
                          KeyTraits,
                          Allocator>::FullLookupType
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    FullLookupForWriting(const T& key) {
  DCHECK(!AccessForbidden());
  DCHECK(table_);
  RegisterModification();

  ValueType* table = table_;
  size_t k = 0;
  size_t size_mask = TableSizeMask();
  unsigned h = HashTranslator::GetHash(key);
  size_t i = h & size_mask;

  UPDATE_ACCESS_COUNTS();

  ValueType* deleted_entry = nullptr;

  while (1) {
    ValueType* entry = table + i;

    if (IsEmptyBucket(*entry))
      return MakeLookupResult(deleted_entry ? deleted_entry : entry, false, h);

    if (HashFunctions::safe_to_compare_to_empty_or_deleted) {
      if (HashTranslator::Equal(Extractor::Extract(*entry), key))
        return MakeLookupResult(entry, true, h);

      if (IsDeletedBucket(*entry))
        deleted_entry = entry;
    } else {
      if (IsDeletedBucket(*entry))
        deleted_entry = entry;
      else if (HashTranslator::Equal(Extractor::Extract(*entry), key))
        return MakeLookupResult(entry, true, h);
    }
    UPDATE_PROBE_COUNTS();
    if (!k)
      k = 1 | DoubleHash(h);
    i = (i + k) & size_mask;
  }
}

template <bool emptyValueIsZero>
struct HashTableBucketInitializer;

template <>
struct HashTableBucketInitializer<false> {
  STATIC_ONLY(HashTableBucketInitializer);
  template <typename Traits, typename Allocator, typename Value>
  static void Initialize(Value& bucket) {
    ConstructTraits<Value, Traits, Allocator>::ConstructAndNotifyElement(
        &bucket, Traits::EmptyValue());
  }
};

template <>
struct HashTableBucketInitializer<true> {
  STATIC_ONLY(HashTableBucketInitializer);
  template <typename Traits, typename Allocator, typename Value>
  static void Initialize(Value& bucket) {
    // This initializes the bucket without copying the empty value.  That
    // makes it possible to use this with types that don't support copying.
    // The memset to 0 looks like a slow operation but is optimized by the
    // compilers.
    memset(&bucket, 0, sizeof(bucket));
  }
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
inline void
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    InitializeBucket(ValueType& bucket) {
  HashTableBucketInitializer<Traits::kEmptyValueIsZero>::template Initialize<
      Traits, Allocator>(bucket);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T, typename Extra>
typename HashTable<Key,
                   Value,
                   Extractor,
                   HashFunctions,
                   Traits,
                   KeyTraits,
                   Allocator>::AddResult
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    insert(T&& key, Extra&& extra) {
  DCHECK(!AccessForbidden());
  DCHECK(Allocator::IsAllocationAllowed());
  if (!table_)
    Expand();

  DCHECK(table_);

  ValueType* table = table_;
  size_t k = 0;
  size_t size_mask = TableSizeMask();
  unsigned h = HashTranslator::GetHash(key);
  size_t i = h & size_mask;

  UPDATE_ACCESS_COUNTS();

  ValueType* deleted_entry = nullptr;
  ValueType* entry;
  while (1) {
    entry = table + i;

    if (IsEmptyBucket(*entry))
      break;

    if (HashFunctions::safe_to_compare_to_empty_or_deleted) {
      if (HashTranslator::Equal(Extractor::Extract(*entry), key))
        return AddResult(this, entry, false);

      if (IsDeletedBucket(*entry))
        deleted_entry = entry;
    } else {
      if (IsDeletedBucket(*entry))
        deleted_entry = entry;
      else if (HashTranslator::Equal(Extractor::Extract(*entry), key))
        return AddResult(this, entry, false);
    }
    UPDATE_PROBE_COUNTS();
    if (!k)
      k = 1 | DoubleHash(h);
    i = (i + k) & size_mask;
  }

  RegisterModification();

  if (deleted_entry) {
    // Overwrite any data left over from last use, using placement new or
    // memset.
    InitializeBucket(*deleted_entry);
    entry = deleted_entry;
    --deleted_count_;
  }

  HashTranslator::Translate(*entry, std::forward<T>(key),
                            std::forward<Extra>(extra));
  DCHECK(!IsEmptyOrDeletedBucket(*entry));
  // Translate constructs an element so we need to notify using the trait. Avoid
  // doing that in the translator so that they can be easily customized.
  ConstructTraits<ValueType, Traits, Allocator>::NotifyNewElement(entry);

  ++key_count_;

  if (ShouldExpand()) {
    entry = Expand(entry);
  } else if (WTF::IsWeak<ValueType>::value && ShouldShrink()) {
    // When weak hash tables are processed by the garbage collector,
    // elements with no other strong references to them will have their
    // table entries cleared. But no shrinking of the backing store is
    // allowed at that time, as allocations are prohibited during that
    // GC phase.
    //
    // With that weak processing taking care of removals, explicit
    // erase()s of elements is rarely done. Which implies that the
    // weak hash table will never be checked if it can be shrunk.
    //
    // To prevent weak hash tables with very low load factors from
    // developing, we perform it when adding elements instead.
    entry = Rehash(table_size_ / 2, entry);
  }

  return AddResult(this, entry, true);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T, typename Extra>
typename HashTable<Key,
                   Value,
                   Extractor,
                   HashFunctions,
                   Traits,
                   KeyTraits,
                   Allocator>::AddResult
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    InsertPassingHashCode(T&& key, Extra&& extra) {
  DCHECK(!AccessForbidden());
  DCHECK(Allocator::IsAllocationAllowed());
  if (!table_)
    Expand();

  FullLookupType lookup_result = FullLookupForWriting<HashTranslator>(key);

  ValueType* entry = lookup_result.first.first;
  bool found = lookup_result.first.second;
  unsigned h = lookup_result.second;

  if (found)
    return AddResult(this, entry, false);

  RegisterModification();

  if (IsDeletedBucket(*entry)) {
    InitializeBucket(*entry);
    --deleted_count_;
  }

  HashTranslator::Translate(*entry, std::forward<T>(key),
                            std::forward<Extra>(extra), h);
  DCHECK(!IsEmptyOrDeletedBucket(*entry));
  // Translate constructs an element so we need to notify using the trait. Avoid
  // doing that in the translator so that they can be easily customized.
  ConstructTraits<ValueType, Traits, Allocator>::NotifyNewElement(entry);

  ++key_count_;
  if (ShouldExpand())
    entry = Expand(entry);

  return AddResult(this, entry, true);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    Reinsert(ValueType&& entry) {
  DCHECK(table_);
  RegisterModification();
  DCHECK(!LookupForWriting(Extractor::Extract(entry)).second);
  DCHECK(
      !IsDeletedBucket(*(LookupForWriting(Extractor::Extract(entry)).first)));
#if DUMP_HASHTABLE_STATS
  HashTableStats::instance().numReinserts.fetch_add(1,
                                                    std::memory_order_relaxed);
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
  stats_->numReinserts.fetch_add(1, std::memory_order_relaxed);
#endif
  Value* new_entry = LookupForWriting(Extractor::Extract(entry)).first;
  Mover<ValueType, Allocator, Traits,
        Traits::template NeedsToForbidGCOnMove<>::value>::Move(std::move(entry),
                                                               *new_entry);

  return new_entry;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline typename HashTable<Key,
                          Value,
                          Extractor,
                          HashFunctions,
                          Traits,
                          KeyTraits,
                          Allocator>::iterator
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    Find(const T& key) {
  ValueType* entry = Lookup<HashTranslator>(key);
  if (!entry)
    return end();

  return MakeKnownGoodIterator(entry);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline typename HashTable<Key,
                          Value,
                          Extractor,
                          HashFunctions,
                          Traits,
                          KeyTraits,
                          Allocator>::const_iterator
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    Find(const T& key) const {
  const ValueType* entry = Lookup<HashTranslator>(key);
  if (!entry)
    return end();

  return MakeKnownGoodConstIterator(entry);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
bool HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::Contains(const T& key) const {
  return Lookup<HashTranslator>(key);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
void HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::erase(const ValueType* pos) {
  RegisterModification();
#if DUMP_HASHTABLE_STATS
  HashTableStats::instance().numRemoves.fetch_add(1, std::memory_order_relaxed);
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
  stats_->numRemoves.fetch_add(1, std::memory_order_relaxed);
#endif

  EnterAccessForbiddenScope();
  DeleteBucket(*pos);
  LeaveAccessForbiddenScope();
  ++deleted_count_;
  --key_count_;

  if (ShouldShrink())
    Shrink();
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
inline void
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    erase(iterator it) {
  if (it == end())
    return;
  erase(it.iterator_.position_);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
inline void
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    erase(const_iterator it) {
  if (it == end())
    return;
  erase(it.position_);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
inline void
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    erase(KeyPeekInType key) {
  erase(find(key));
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    AllocateTable(unsigned size) {
  size_t alloc_size = base::CheckMul(size, sizeof(ValueType)).ValueOrDie();
  ValueType* result;
  // Assert that we will not use memset on things with a vtable entry.  The
  // compiler will also check this on some platforms. We would like to check
  // this on the whole value (key-value pair), but std::is_polymorphic will
  // return false for a pair of two types, even if one of the components is
  // polymorphic.
  static_assert(
      !Traits::kEmptyValueIsZero || !std::is_polymorphic<KeyType>::value,
      "empty value cannot be zero for things with a vtable");
  static_assert(
      Allocator::kIsGarbageCollected ||
          ((!IsDisallowNew<KeyType>::value || !IsTraceable<KeyType>::value) &&
           (!IsDisallowNew<ValueType>::value ||
            !IsTraceable<ValueType>::value)),
      "Cannot put DISALLOW_NEW objects that "
      "have trace methods into an off-heap HashTable");

  if (Traits::kEmptyValueIsZero) {
    result = Allocator::template AllocateZeroedHashTableBacking<ValueType,
                                                                HashTable>(
        alloc_size);
  } else {
    result = Allocator::template AllocateHashTableBacking<ValueType, HashTable>(
        alloc_size);
    for (unsigned i = 0; i < size; i++)
      InitializeBucket(result[i]);
  }
  return result;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
void HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::DeleteAllBucketsAndDeallocate(ValueType* table,
                                                         unsigned size) {
  // We delete a bucket in the following cases:
  // - It is not trivially destructible.
  // - The table is weak (thus garbage collected) and we are currently marking.
  // This is to handle the case where a backing store is removed from the
  // HashTable after HashTable has been enqueued for processing. If we remove
  // the backing in that case it stays unprocessed which upsets the marking
  // verifier that checks that all backings are in consistent state.
  const bool needs_bucket_deletion =
      !std::is_trivially_destructible<ValueType>::value ||
      (WTF::IsWeak<ValueType>::value && Allocator::IsIncrementalMarking());
  if (needs_bucket_deletion) {
    for (unsigned i = 0; i < size; ++i) {
      // This code is called when the hash table is cleared or resized. We
      // have allocated a new backing store and we need to run the
      // destructors on the old backing store, as it is being freed. If we
      // are GCing we need to both call the destructor and mark the bucket
      // as deleted, otherwise the destructor gets called again when the
      // GC finds the backing store. With the default allocator it's
      // enough to call the destructor, since we will free the memory
      // explicitly and we won't see the memory with the bucket again.
      if (Allocator::kIsGarbageCollected) {
        if (!IsEmptyOrDeletedBucket(table[i]))
          DeleteBucket(table[i]);
      } else {
        if (!IsDeletedBucket(table[i]))
          table[i].~ValueType();
      }
    }
  }
  Allocator::FreeHashTableBacking(table);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    Expand(Value* entry) {
  unsigned new_size;
  if (!table_size_) {
    new_size = KeyTraits::kMinimumTableSize;
  } else if (MustRehashInPlace()) {
    new_size = table_size_;
  } else {
    new_size = table_size_ * 2;
    CHECK_GT(new_size, table_size_);
  }

  return Rehash(new_size, entry);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    ExpandBuffer(unsigned new_table_size, Value* entry, bool& success) {
  success = false;
  DCHECK_LT(table_size_, new_table_size);
  CHECK(Allocator::IsAllocationAllowed());
  if (!Allocator::ExpandHashTableBacking(table_,
                                         new_table_size * sizeof(ValueType)))
    return nullptr;

  success = true;

  Value* new_entry = nullptr;
  unsigned old_table_size = table_size_;
  ValueType* original_table = table_;

  ValueType* temporary_table = AllocateTable(old_table_size);
  for (unsigned i = 0; i < old_table_size; i++) {
    if (&table_[i] == entry)
      new_entry = &temporary_table[i];
    if (IsEmptyOrDeletedBucket(table_[i])) {
      DCHECK_NE(&table_[i], entry);
      if (Traits::kEmptyValueIsZero) {
        memset(&temporary_table[i], 0, sizeof(ValueType));
      } else {
        InitializeBucket(temporary_table[i]);
      }
    } else {
      Mover<ValueType, Allocator, Traits,
            Traits::template NeedsToForbidGCOnMove<>::value>::
          Move(std::move(table_[i]), temporary_table[i]);
      table_[i].~ValueType();
    }
  }
  table_ = temporary_table;
  Allocator::template BackingWriteBarrier(&table_);

  if (Traits::kEmptyValueIsZero) {
    memset(original_table, 0, new_table_size * sizeof(ValueType));
  } else {
    for (unsigned i = 0; i < new_table_size; i++)
      InitializeBucket(original_table[i]);
  }
  new_entry = RehashTo(original_table, new_table_size, new_entry);

  return new_entry;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    RehashTo(ValueType* new_table, unsigned new_table_size, Value* entry) {
#if DUMP_HASHTABLE_STATS
  if (table_size_ != 0) {
    HashTableStats::instance().numRehashes.fetch_add(1,
                                                     std::memory_order_relaxed);
  }
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
  if (table_size_ != 0)
    stats_->numRehashes.fetch_add(1, std::memory_order_relaxed);
#endif

  HashTable new_hash_table(RawStorageTag{}, new_table, new_table_size);

  Value* new_entry = nullptr;
  for (unsigned i = 0; i != table_size_; ++i) {
    if (IsEmptyOrDeletedBucket(table_[i])) {
      DCHECK_NE(&table_[i], entry);
      continue;
    }
    Value* reinserted_entry = new_hash_table.Reinsert(std::move(table_[i]));
    if (&table_[i] == entry) {
      DCHECK(!new_entry);
      new_entry = reinserted_entry;
    }
  }

  Allocator::TraceBackingStoreIfMarked(new_hash_table.table_);

  ValueType* old_table = table_;
  unsigned old_table_size = table_size_;

  // This swaps the newly allocated buffer with the current one. The store to
  // the current table has to be atomic to prevent races with concurrent marker.
  AsAtomicPtr(&table_)->store(new_hash_table.table_, std::memory_order_relaxed);
  Allocator::template BackingWriteBarrier(&table_);
  table_size_ = new_table_size;

  new_hash_table.table_ = old_table;
  new_hash_table.table_size_ = old_table_size;

  // Explicitly clear since garbage collected HashTables don't do this on
  // destruction.
  new_hash_table.clear();

  deleted_count_ = 0;

#if DUMP_HASHTABLE_STATS_PER_TABLE
  if (!stats_)
    stats_ = HashTableStatsPtr<Allocator>::Create();
#endif

  return new_entry;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    Rehash(unsigned new_table_size, Value* entry) {
  unsigned old_table_size = table_size_;

#if DUMP_HASHTABLE_STATS
  if (old_table_size != 0) {
    HashTableStats::instance().numRehashes.fetch_add(1,
                                                     std::memory_order_relaxed);
  }
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
  if (old_table_size != 0)
    stats_->numRehashes.fetch_add(1, std::memory_order_relaxed);
#endif

  // The Allocator::kIsGarbageCollected check is not needed.  The check is just
  // a static hint for a compiler to indicate that Base::expandBuffer returns
  // false if Allocator is a PartitionAllocator.
  if (Allocator::kIsGarbageCollected && new_table_size > old_table_size) {
    bool success;
    Value* new_entry = ExpandBuffer(new_table_size, entry, success);
    if (success)
      return new_entry;
  }

  ValueType* new_table = AllocateTable(new_table_size);
  Value* new_entry = RehashTo(new_table, new_table_size, entry);

  return new_entry;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
void HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::clear() {
  RegisterModification();
  if (!table_)
    return;

  EnterAccessForbiddenScope();
  DeleteAllBucketsAndDeallocate(table_, table_size_);
  LeaveAccessForbiddenScope();
  AsAtomicPtr(&table_)->store(nullptr, std::memory_order_relaxed);
  table_size_ = 0;
  key_count_ = 0;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    HashTable(const HashTable& other)
    : table_(nullptr),
      table_size_(0),
      key_count_(0),
      deleted_count_(0),
      queue_flag_(false)
#if DCHECK_IS_ON()
      ,
      access_forbidden_(false),
      modifications_(0)
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
      ,
      stats_(HashTableStatsPtr<Allocator>::copy(other.stats_))
#endif
{
  if (other.size())
    ReserveCapacityForSize(other.size());
  // Copy the hash table the dumb way, by adding each element to the new
  // table.  It might be more efficient to copy the table slots, but it's not
  // clear that efficiency is needed.
  for (const auto& element : other)
    insert(element);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    HashTable(HashTable&& other)
    : table_(nullptr),
      table_size_(0),
      key_count_(0),
      deleted_count_(0),
      queue_flag_(false)
#if DCHECK_IS_ON()
      ,
      access_forbidden_(false),
      modifications_(0)
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
      ,
      stats_(HashTableStatsPtr<Allocator>::copy(other.stats_))
#endif
{
  swap(other);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
void HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::swap(HashTable& other) {
  DCHECK(!AccessForbidden());
  // Following 3 lines swap table_ and other.table_ using atomic stores. These
  // are needed for Oilpan concurrent marking which might trace the hash table
  // while it is being swapped (i.e. the atomic stores are to avoid a data
  // race). Atomic reads are not needed here because this method is only called
  // on the mutator thread, which is also the only one that writes to them, so
  // there is *no* risk of data races when reading.
  AtomicWriteSwap(table_, other.table_);
  Allocator::template BackingWriteBarrier(&table_);
  Allocator::template BackingWriteBarrier(&other.table_);
  if (IsWeak<ValueType>::value) {
    // Weak processing is omitted when no backing store is present. In case such
    // an empty table is later on used it needs to be strongified.
    if (table_)
      Allocator::TraceBackingStoreIfMarked(table_);
    if (other.table_)
      Allocator::TraceBackingStoreIfMarked(other.table_);
  }
  std::swap(table_size_, other.table_size_);
  std::swap(key_count_, other.key_count_);
  // std::swap does not work for bit fields.
  unsigned deleted = deleted_count_;
  deleted_count_ = other.deleted_count_;
  other.deleted_count_ = deleted;
  DCHECK(!queue_flag_);
  DCHECK(!other.queue_flag_);

#if DCHECK_IS_ON()
  std::swap(modifications_, other.modifications_);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
  HashTableStatsPtr<Allocator>::swap(stats_, other.stats_);
#endif
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>&
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
operator=(const HashTable& other) {
  HashTable tmp(other);
  swap(tmp);
  return *this;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>&
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
operator=(HashTable&& other) {
  swap(other);
  return *this;
}

template <WeakHandlingFlag weakHandlingFlag,
          typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
struct WeakProcessingHashTableHelper {
  STATIC_ONLY(WeakProcessingHashTableHelper);
  static void Process(const typename Allocator::LivenessBroker&, const void*) {}
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
struct WeakProcessingHashTableHelper<kWeakHandling,
                                     Key,
                                     Value,
                                     Extractor,
                                     HashFunctions,
                                     Traits,
                                     KeyTraits,
                                     Allocator> {
  STATIC_ONLY(WeakProcessingHashTableHelper);

  using HashTableType = HashTable<Key,
                                  Value,
                                  Extractor,
                                  HashFunctions,
                                  Traits,
                                  KeyTraits,
                                  Allocator>;
  using ValueType = typename HashTableType::ValueType;

  // Used for purely weak and for weak-and-strong tables (ephemerons).
  static void Process(const typename Allocator::LivenessBroker& info,
                      const void* parameter) {
    HashTableType* table =
        reinterpret_cast<HashTableType*>(const_cast<void*>(parameter));
    // During incremental marking, the table may be freed after the callback has
    // been registered.
    if (!table->table_)
      return;

    // Weak processing: If the backing was accessible through an iterator and
    // thus marked strongly this loop will find all buckets as non-empty.
    for (ValueType* element = table->table_ + table->table_size_ - 1;
         element >= table->table_; element--) {
      if (!HashTableType::IsEmptyOrDeletedBucket(*element)) {
        if (!TraceInCollectionTrait<kWeakHandling, ValueType, Traits>::IsAlive(
                info, *element)) {
          table->RegisterModification();
          HashTableType::DeleteBucket(*element);  // Also calls the destructor.
          table->deleted_count_++;
          table->key_count_--;
          // We don't rehash the backing until the next add or delete,
          // because that would cause allocation during GC.
        }
      }
    }
  }
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename VisitorDispatcher, typename A>
std::enable_if_t<A::kIsGarbageCollected>
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    Trace(VisitorDispatcher visitor) const {
  static_assert(WTF::IsWeak<ValueType>::value ||
                    IsTraceableInCollectionTrait<Traits>::value,
                "Value should not be traced");
  TraceTable(visitor, AsAtomicPtr(&table_)->load(std::memory_order_relaxed));
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename VisitorDispatcher, typename A>
std::enable_if_t<A::kIsGarbageCollected>
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    TraceTable(VisitorDispatcher visitor, const ValueType* table) const {
  if (!WTF::IsWeak<ValueType>::value) {
    // Strong HashTable.
    Allocator::template TraceHashTableBackingStrongly<ValueType, HashTable>(
        visitor, table, &table_);
  } else {
    // Weak HashTable. The HashTable may be held alive strongly from somewhere
    // else, e.g., an iterator.

    // Trace the table weakly. For marking this will result in delaying the
    // processing until the end of the atomic pause. It is safe to trace
    // weakly multiple times.
    Allocator::template TraceHashTableBackingWeakly<ValueType, HashTable>(
        visitor, table, &table_,
        WeakProcessingHashTableHelper<WeakHandlingTrait<ValueType>::value, Key,
                                      Value, Extractor, HashFunctions, Traits,
                                      KeyTraits, Allocator>::Process,
        this);
  }
}

// iterator adapters

template <typename HashTableType, typename Traits>
struct HashTableConstIteratorAdapter {
  STACK_ALLOCATED();

 public:
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = HashTableType;
  using difference_type = ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;

  HashTableConstIteratorAdapter() = default;
  HashTableConstIteratorAdapter(
      const typename HashTableType::const_iterator& impl)
      : impl_(impl) {}
  typedef typename Traits::IteratorConstGetType GetType;
  typedef
      typename HashTableType::ValueTraits::IteratorConstGetType SourceGetType;

  GetType Get() const {
    return const_cast<GetType>(SourceGetType(impl_.Get()));
  }
  typename Traits::IteratorConstReferenceType operator*() const {
    return Traits::GetToReferenceConstConversion(Get());
  }
  GetType operator->() const { return Get(); }

  HashTableConstIteratorAdapter& operator++() {
    ++impl_;
    return *this;
  }
  // postfix ++ intentionally omitted
  HashTableConstIteratorAdapter& operator--() {
    --impl_;
    return *this;
  }
  // postfix -- intentionally omitted
  typename HashTableType::const_iterator impl_;
};

template <typename HashTable, typename Traits>
std::ostream& operator<<(
    std::ostream& stream,
    const HashTableConstIteratorAdapter<HashTable, Traits>& iterator) {
  return stream << iterator.impl_;
}

template <typename HashTableType, typename Traits>
struct HashTableIteratorAdapter {
  STACK_ALLOCATED();
  typedef typename Traits::IteratorGetType GetType;
  typedef typename HashTableType::ValueTraits::IteratorGetType SourceGetType;

  HashTableIteratorAdapter() = default;
  HashTableIteratorAdapter(const typename HashTableType::iterator& impl)
      : impl_(impl) {}

  GetType Get() const {
    return const_cast<GetType>(SourceGetType(impl_.get()));
  }
  typename Traits::IteratorReferenceType operator*() const {
    return Traits::GetToReferenceConversion(Get());
  }
  GetType operator->() const { return Get(); }

  HashTableIteratorAdapter& operator++() {
    ++impl_;
    return *this;
  }
  // postfix ++ intentionally omitted

  HashTableIteratorAdapter& operator--() {
    --impl_;
    return *this;
  }
  // postfix -- intentionally omitted

  operator HashTableConstIteratorAdapter<HashTableType, Traits>() {
    typename HashTableType::const_iterator i = impl_;
    return i;
  }

  typename HashTableType::iterator impl_;
};

template <typename HashTable, typename Traits>
std::ostream& operator<<(
    std::ostream& stream,
    const HashTableIteratorAdapter<HashTable, Traits>& iterator) {
  return stream << iterator.impl_;
}

template <typename T, typename U>
inline bool operator==(const HashTableConstIteratorAdapter<T, U>& a,
                       const HashTableConstIteratorAdapter<T, U>& b) {
  return a.impl_ == b.impl_;
}

template <typename T, typename U>
inline bool operator!=(const HashTableConstIteratorAdapter<T, U>& a,
                       const HashTableConstIteratorAdapter<T, U>& b) {
  return a.impl_ != b.impl_;
}

template <typename T, typename U>
inline bool operator==(const HashTableIteratorAdapter<T, U>& a,
                       const HashTableIteratorAdapter<T, U>& b) {
  return a.impl_ == b.impl_;
}

template <typename T, typename U>
inline bool operator!=(const HashTableIteratorAdapter<T, U>& a,
                       const HashTableIteratorAdapter<T, U>& b) {
  return a.impl_ != b.impl_;
}

// All 4 combinations of ==, != and Const,non const.
template <typename T, typename U>
inline bool operator==(const HashTableConstIteratorAdapter<T, U>& a,
                       const HashTableIteratorAdapter<T, U>& b) {
  return a.impl_ == b.impl_;
}

template <typename T, typename U>
inline bool operator!=(const HashTableConstIteratorAdapter<T, U>& a,
                       const HashTableIteratorAdapter<T, U>& b) {
  return a.impl_ != b.impl_;
}

template <typename T, typename U>
inline bool operator==(const HashTableIteratorAdapter<T, U>& a,
                       const HashTableConstIteratorAdapter<T, U>& b) {
  return a.impl_ == b.impl_;
}

template <typename T, typename U>
inline bool operator!=(const HashTableIteratorAdapter<T, U>& a,
                       const HashTableConstIteratorAdapter<T, U>& b) {
  return a.impl_ != b.impl_;
}

template <typename Collection1, typename Collection2>
inline void RemoveAll(Collection1& collection,
                      const Collection2& to_be_removed) {
  if (collection.IsEmpty() || to_be_removed.IsEmpty())
    return;
  typedef typename Collection2::const_iterator CollectionIterator;
  CollectionIterator end(to_be_removed.end());
  for (CollectionIterator it(to_be_removed.begin()); it != end; ++it)
    collection.erase(*it);
}

}  // namespace WTF

#include "third_party/blink/renderer/platform/wtf/hash_iterators.h"

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_TABLE_H_
