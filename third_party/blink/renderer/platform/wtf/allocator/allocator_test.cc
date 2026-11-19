// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#include <array>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "partition_alloc/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace {

struct Empty {};

struct StackAllocatedType {
  STACK_ALLOCATED();
};

static_assert(!blink::IsStackAllocatedTypeV<Empty>,
              "Failed to detect STACK_ALLOCATED macro.");
static_assert(blink::IsStackAllocatedTypeV<StackAllocatedType>,
              "Failed to detect STACK_ALLOCATED macro.");

static_assert(blink::IsStackAllocatedTypeV<std::pair<int, StackAllocatedType>>,
              "Failed to detect STACK_ALLOCATED macro.");
static_assert(blink::IsStackAllocatedTypeV<std::optional<StackAllocatedType>>,
              "Failed to detect STACK_ALLOCATED macro.");
static_assert(
    blink::IsStackAllocatedTypeV<std::variant<int, StackAllocatedType>>,
    "Failed to detect STACK_ALLOCATED macro.");

template <typename T>
concept HasClassSpecificOperatorNew =
    requires(size_t size) { T::operator new(size); };

template <typename T>
concept HasClassSpecificOperatorDelete =
    requires(void* ptr) { T::operator delete(ptr); };

class FastMallocClass {
  USING_FAST_MALLOC(FastMallocClass);

 public:
  explicit FastMallocClass(int value = 0) : value_(value) {}
  int value() const { return value_; }

 private:
  int value_;
};

struct FastMallocWithTypeNameStruct {
  USING_FAST_MALLOC_WITH_TYPE_NAME(FastMallocWithTypeNameStruct);

 public:
  int value = 7;
};

// Positive compile-time checks verifying that `FastMallocChecker` accepts valid
// inheritance and template combinations (including `DISALLOW_NEW()` on a
// subclass of a `USING_FAST_MALLOC` base and CRTP `RefCounted<T>`).
class DerivedFastMallocClass : public FastMallocClass {
  USING_FAST_MALLOC(DerivedFastMallocClass);

 public:
  explicit DerivedFastMallocClass(int value = 0) : FastMallocClass(value) {}
};

class DisallowNewDerivedFromFastMalloc : public FastMallocClass {
  DISALLOW_NEW();

 public:
  explicit DisallowNewDerivedFromFastMalloc(int value = 0)
      : FastMallocClass(value) {}
};

class RefCountedFastMallocClass
    : public blink::RefCounted<RefCountedFastMallocClass> {
  USING_FAST_MALLOC(RefCountedFastMallocClass);

 public:
  int value = 0;
};

template <typename T>
class TemplateFastMallocClass {
  USING_FAST_MALLOC(TemplateFastMallocClass);

 public:
  T value{};
};

static_assert(sizeof(DerivedFastMallocClass) > 0);
static_assert(sizeof(DisallowNewDerivedFromFastMalloc) > 0);
static_assert(sizeof(RefCountedFastMallocClass) > 0);
static_assert(sizeof(TemplateFastMallocClass<int>) > 0);

// Historically `USING_FAST_MALLOC` ends in a `private:` access specifier, so
// members declared after it without an explicit `public:` label are implicitly
// private (even inside a `struct`, where default access is `public:`). Verify
// that the PA-E no-op branch preserves that behavior.
struct FastMallocAccessCheck {
  USING_FAST_MALLOC(FastMallocAccessCheck);
  using ImplicitlyPrivateType = int;
};

template <typename T>
concept HasAccessibleImplicitlyPrivateType =
    requires { typename T::ImplicitlyPrivateType; };

static_assert(!HasAccessibleImplicitlyPrivateType<FastMallocAccessCheck>);

// Verify that `USING_FAST_MALLOC` omits class-specific `operator new`/`delete`
// overrides under PA-E (delegating to the global operators) and still provides
// them in non-PA-E builds.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
static_assert(!HasClassSpecificOperatorNew<FastMallocClass>);
static_assert(!HasClassSpecificOperatorDelete<FastMallocClass>);
static_assert(!HasClassSpecificOperatorNew<FastMallocWithTypeNameStruct>);
static_assert(!HasClassSpecificOperatorDelete<FastMallocWithTypeNameStruct>);
#else
static_assert(HasClassSpecificOperatorNew<FastMallocClass>);
static_assert(HasClassSpecificOperatorDelete<FastMallocClass>);
static_assert(HasClassSpecificOperatorNew<FastMallocWithTypeNameStruct>);
static_assert(HasClassSpecificOperatorDelete<FastMallocWithTypeNameStruct>);
#endif

// Verify basic allocation and placement-new paths work in both PA-E (using
// global `operator new`/`delete`) and non-PA-E (using class-specific overrides)
// builds, including Blink's `base::NotNullTag` placement-new overload.
TEST(AllocatorTest, UsingFastMalloc) {
  auto instance = std::make_unique<FastMallocClass>(42);
  EXPECT_EQ(instance->value(), 42);

  FastMallocClass* array = new FastMallocClass[4];
  delete[] array;

  union Storage {
    Storage() {}
    ~Storage() {}
    FastMallocClass object;
  } storage;

  FastMallocClass* placed = new (&storage.object) FastMallocClass(123);
  EXPECT_EQ(placed, &storage.object);
  EXPECT_EQ(placed->value(), 123);
  placed->~FastMallocClass();

  FastMallocClass* placed_not_null =
      new (base::NotNullTag::kNotNull, &storage.object) FastMallocClass(456);
  EXPECT_EQ(placed_not_null, &storage.object);
  EXPECT_EQ(placed_not_null->value(), 456);
  placed_not_null->~FastMallocClass();
}

// Over-aligned `new` (`std::align_val_t`) is only supported under PA-E, where
// `USING_FAST_MALLOC` omits class-specific `operator new` overrides and lets
// C++17 dispatch to global `::operator new(size_t, std::align_val_t)`. In
// non-PA-E builds, `USING_FAST_MALLOC` still declares unaligned class-specific
// `operator new(size_t)` / `operator new[](size_t)` (`Partitions` has no
// aligned `FastMalloc`), hiding the global aligned overloads per C++17
// `[expr.new]`.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
struct alignas(128) OverAlignedFastMallocType {
  USING_FAST_MALLOC(OverAlignedFastMallocType);

 public:
  std::array<uint8_t, 257> payload{};
};

TEST(AllocatorTest, UsingFastMallocOverAligned) {
  static_assert(alignof(OverAlignedFastMallocType) >
                __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  std::vector<std::unique_ptr<OverAlignedFastMallocType>> allocations;
  allocations.reserve(16);
  for (int i = 0; i < 16; ++i) {
    auto ptr = std::make_unique<OverAlignedFastMallocType>();
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr.get()) %
                  alignof(OverAlignedFastMallocType),
              0u);
    allocations.push_back(std::move(ptr));
  }

  OverAlignedFastMallocType* array = new OverAlignedFastMallocType[4];
  EXPECT_EQ(
      reinterpret_cast<uintptr_t>(array) % alignof(OverAlignedFastMallocType),
      0u);
  delete[] array;
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace
