// Copyright 2020 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ABSL_STRINGS_INTERNAL_CORD_INTERNAL_H_
#define ABSL_STRINGS_INTERNAL_CORD_INTERNAL_H_

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "absl/base/internal/invoke.h"
#include "absl/container/internal/compressed_tuple.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace cord_internal {

// Wraps std::atomic for reference counting.
class Refcount {
 public:
  constexpr Refcount() : count_{kRefIncrement} {}
  struct Immortal {};
  explicit constexpr Refcount(Immortal) : count_(kImmortalTag) {}

  // Increments the reference count. Imposes no memory ordering.
  inline void Increment() {
    count_.fetch_add(kRefIncrement, std::memory_order_relaxed);
  }

  // Asserts that the current refcount is greater than 0. If the refcount is
  // greater than 1, decrements the reference count.
  //
  // Returns false if there are no references outstanding; true otherwise.
  // Inserts barriers to ensure that state written before this method returns
  // false will be visible to a thread that just observed this method returning
  // false.
  inline bool Decrement() {
    int32_t refcount = count_.load(std::memory_order_acquire);
    assert(refcount > 0 || refcount & kImmortalTag);
    return refcount != kRefIncrement &&
           count_.fetch_sub(kRefIncrement, std::memory_order_acq_rel) !=
               kRefIncrement;
  }

  // Same as Decrement but expect that refcount is greater than 1.
  inline bool DecrementExpectHighRefcount() {
    int32_t refcount =
        count_.fetch_sub(kRefIncrement, std::memory_order_acq_rel);
    assert(refcount > 0 || refcount & kImmortalTag);
    return refcount != kRefIncrement;
  }

  // Returns the current reference count using acquire semantics.
  inline int32_t Get() const {
    return count_.load(std::memory_order_acquire) >> kImmortalShift;
  }

  // Returns whether the atomic integer is 1.
  // If the reference count is used in the conventional way, a
  // reference count of 1 implies that the current thread owns the
  // reference and no other thread shares it.
  // This call performs the test for a reference count of one, and
  // performs the memory barrier needed for the owning thread
  // to act on the object, knowing that it has exclusive access to the
  // object.
  inline bool IsOne() {
    return count_.load(std::memory_order_acquire) == kRefIncrement;
  }

  bool IsImmortal() const {
    return (count_.load(std::memory_order_relaxed) & kImmortalTag) != 0;
  }

 private:
  // We reserve the bottom bit to tag a reference count as immortal.
  // By making it `1` we ensure that we never reach `0` when adding/subtracting
  // `2`, thus it never looks as if it should be destroyed.
  // These are used for the StringConstant constructor where we do not increase
  // the refcount at construction time (due to constinit requirements) but we
  // will still decrease it at destruction time to avoid branching on Unref.
  enum {
    kImmortalShift = 1,
    kRefIncrement = 1 << kImmortalShift,
    kImmortalTag = kRefIncrement - 1
  };

  std::atomic<int32_t> count_;
};

// The overhead of a vtable is too much for Cord, so we roll our own subclasses
// using only a single byte to differentiate classes from each other - the "tag"
// byte.  Define the subclasses first so we can provide downcasting helper
// functions in the base class.

struct CordRepConcat;
struct CordRepSubstring;
struct CordRepExternal;

// Various representations that we allow
enum CordRepKind {
  CONCAT        = 0,
  EXTERNAL      = 1,
  SUBSTRING     = 2,

  // We have different tags for different sized flat arrays,
  // starting with FLAT
  FLAT          = 3,
};

struct CordRep {
  CordRep() = default;
  constexpr CordRep(Refcount::Immortal immortal, size_t l)
      : length(l), refcount(immortal), tag(EXTERNAL), data{} {}

  // The following three fields have to be less than 32 bytes since
  // that is the smallest supported flat node size.
  size_t length;
  Refcount refcount;
  // If tag < FLAT, it represents CordRepKind and indicates the type of node.
  // Otherwise, the node type is CordRepFlat and the tag is the encoded size.
  uint8_t tag;
  char data[1];  // Starting point for flat array: MUST BE LAST FIELD of CordRep

  inline CordRepConcat* concat();
  inline const CordRepConcat* concat() const;
  inline CordRepSubstring* substring();
  inline const CordRepSubstring* substring() const;
  inline CordRepExternal* external();
  inline const CordRepExternal* external() const;
};

struct CordRepConcat : public CordRep {
  CordRep* left;
  CordRep* right;

  uint8_t depth() const { return static_cast<uint8_t>(data[0]); }
  void set_depth(uint8_t depth) { data[0] = static_cast<char>(depth); }
};

struct CordRepSubstring : public CordRep {
  size_t start;  // Starting offset of substring in child
  CordRep* child;
};

// Type for function pointer that will invoke the releaser function and also
// delete the `CordRepExternalImpl` corresponding to the passed in
// `CordRepExternal`.
using ExternalReleaserInvoker = void (*)(CordRepExternal*);

// External CordReps are allocated together with a type erased releaser. The
// releaser is stored in the memory directly following the CordRepExternal.
struct CordRepExternal : public CordRep {
  CordRepExternal() = default;
  explicit constexpr CordRepExternal(absl::string_view str)
      : CordRep(Refcount::Immortal{}, str.size()),
        base(str.data()),
        releaser_invoker(nullptr) {}

  const char* base;
  // Pointer to function that knows how to call and destroy the releaser.
  ExternalReleaserInvoker releaser_invoker;
};

struct Rank1 {};
struct Rank0 : Rank1 {};

template <typename Releaser, typename = ::absl::base_internal::invoke_result_t<
                                 Releaser, absl::string_view>>
void InvokeReleaser(Rank0, Releaser&& releaser, absl::string_view data) {
  ::absl::base_internal::invoke(std::forward<Releaser>(releaser), data);
}

template <typename Releaser,
          typename = ::absl::base_internal::invoke_result_t<Releaser>>
void InvokeReleaser(Rank1, Releaser&& releaser, absl::string_view) {
  ::absl::base_internal::invoke(std::forward<Releaser>(releaser));
}

// We use CompressedTuple so that we can benefit from EBCO.
template <typename Releaser>
struct CordRepExternalImpl
    : public CordRepExternal,
      public ::absl::container_internal::CompressedTuple<Releaser> {
  // The extra int arg is so that we can avoid interfering with copy/move
  // constructors while still benefitting from perfect forwarding.
  template <typename T>
  CordRepExternalImpl(T&& releaser, int)
      : CordRepExternalImpl::CompressedTuple(std::forward<T>(releaser)) {
    this->releaser_invoker = &Release;
  }

  ~CordRepExternalImpl() {
    InvokeReleaser(Rank0{}, std::move(this->template get<0>()),
                   absl::string_view(base, length));
  }

  static void Release(CordRepExternal* rep) {
    delete static_cast<CordRepExternalImpl*>(rep);
  }
};

template <typename Str>
struct ConstInitExternalStorage {
  ABSL_CONST_INIT static CordRepExternal value;
};

template <typename Str>
CordRepExternal ConstInitExternalStorage<Str>::value(Str::value);

enum {
  kMaxInline = 15,
  // Tag byte & kMaxInline means we are storing a pointer.
  kTreeFlag = 1 << 4,
  // Tag byte & kProfiledFlag means we are profiling the Cord.
  kProfiledFlag = 1 << 5
};

// If the data has length <= kMaxInline, we store it in `as_chars`, and
// store the size in `tagged_size`.
// Else we store it in a tree and store a pointer to that tree in
// `as_tree.rep` and store a tag in `tagged_size`.
struct AsTree {
  absl::cord_internal::CordRep* rep;
  char padding[kMaxInline + 1 - sizeof(absl::cord_internal::CordRep*) - 1];
  char tagged_size;
};

constexpr char GetOrNull(absl::string_view data, size_t pos) {
  return pos < data.size() ? data[pos] : '\0';
}

union InlineData {
  constexpr InlineData() : as_chars{} {}
  explicit constexpr InlineData(AsTree tree) : as_tree(tree) {}
  explicit constexpr InlineData(absl::string_view chars)
      : as_chars{GetOrNull(chars, 0),  GetOrNull(chars, 1),
                 GetOrNull(chars, 2),  GetOrNull(chars, 3),
                 GetOrNull(chars, 4),  GetOrNull(chars, 5),
                 GetOrNull(chars, 6),  GetOrNull(chars, 7),
                 GetOrNull(chars, 8),  GetOrNull(chars, 9),
                 GetOrNull(chars, 10), GetOrNull(chars, 11),
                 GetOrNull(chars, 12), GetOrNull(chars, 13),
                 GetOrNull(chars, 14), static_cast<char>(chars.size())} {}

  AsTree as_tree;
  char as_chars[kMaxInline + 1];
};
static_assert(sizeof(InlineData) == kMaxInline + 1, "");
static_assert(sizeof(AsTree) == sizeof(InlineData), "");
static_assert(offsetof(AsTree, tagged_size) == kMaxInline, "");

}  // namespace cord_internal
ABSL_NAMESPACE_END
}  // namespace absl
#endif  // ABSL_STRINGS_INTERNAL_CORD_INTERNAL_H_
