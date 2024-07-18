// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_

#include <stdint.h>

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {

template <typename T>
class ArrayDataView;

template <typename T>
class AssociatedInterfacePtrInfoDataView;

template <typename T>
class AssociatedInterfaceRequestDataView;

template <typename T>
class InterfacePtrDataView;

template <typename T>
class InterfaceRequestDataView;

template <typename K, typename V>
class MapDataView;

class StringDataView;

namespace internal {

// Please note that this is a different value than |mojo::kInvalidHandleValue|,
// which is the "decoded" invalid handle.
const uint32_t kEncodedInvalidHandleValue = static_cast<uint32_t>(-1);

// A serialized union always takes 16 bytes:
//   4-byte size + 4-byte tag + 8-byte payload.
const uint32_t kUnionDataSize = 16;

template <typename T>
class Array_Data;

template <typename K, typename V>
class Map_Data;

using String_Data = Array_Data<char>;

inline size_t Align(size_t size) {
  return (size + 7) & ~size_t{7};
}

inline bool IsAligned(const void* ptr) {
  return !(reinterpret_cast<uintptr_t>(ptr) & 0x7);
}

// Pointers are encoded as relative offsets. The offsets are relative to the
// address of where the offset value is stored, such that the pointer may be
// recovered with the expression:
//
//   ptr = reinterpret_cast<char*>(offset) + *offset
//
// A null pointer is encoded as an offset value of 0.
//
inline void EncodePointer(const void* ptr, uint64_t* offset) {
  if (!ptr) {
    *offset = 0;
    return;
  }

  const char* p_obj = reinterpret_cast<const char*>(ptr);
  const char* p_slot = reinterpret_cast<const char*>(offset);
  DCHECK(p_obj > p_slot);

  *offset = static_cast<uint64_t>(p_obj - p_slot);
}

// Note: This function doesn't validate the encoded pointer value.
inline const void* DecodePointer(const uint64_t* offset) {
  if (!*offset)
    return nullptr;
  return reinterpret_cast<const char*>(offset) + *offset;
}

#pragma pack(push, 1)

struct StructHeader {
  uint32_t num_bytes;
  uint32_t version;
};
static_assert(sizeof(StructHeader) == 8, "Bad sizeof(StructHeader)");

struct ArrayHeader {
  uint32_t num_bytes;
  uint32_t num_elements;
};
static_assert(sizeof(ArrayHeader) == 8, "Bad_sizeof(ArrayHeader)");

template <typename T>
struct Pointer {
  using BaseType = T;

  void Set(T* ptr) { EncodePointer(ptr, &offset); }
  const T* Get() const { return static_cast<const T*>(DecodePointer(&offset)); }
  T* Get() {
    return static_cast<T*>(const_cast<void*>(DecodePointer(&offset)));
  }

  bool is_null() const { return offset == 0; }

  uint64_t offset;
};
static_assert(sizeof(Pointer<char>) == 8, "Bad_sizeof(Pointer)");

using GenericPointer = Pointer<void>;

struct Handle_Data {
  Handle_Data() = default;
  explicit Handle_Data(uint32_t value) : value(value) {}

  bool is_valid() const { return value != kEncodedInvalidHandleValue; }

  uint32_t value;
};
static_assert(sizeof(Handle_Data) == 4, "Bad_sizeof(Handle_Data)");

struct Interface_Data {
  Handle_Data handle;
  uint32_t version;
};
static_assert(sizeof(Interface_Data) == 8, "Bad_sizeof(Interface_Data)");

struct AssociatedEndpointHandle_Data {
  AssociatedEndpointHandle_Data() = default;
  explicit AssociatedEndpointHandle_Data(uint32_t value) : value(value) {}

  bool is_valid() const { return value != kEncodedInvalidHandleValue; }

  uint32_t value;
};
static_assert(sizeof(AssociatedEndpointHandle_Data) == 4,
              "Bad_sizeof(AssociatedEndpointHandle_Data)");

struct AssociatedInterface_Data {
  AssociatedEndpointHandle_Data handle;
  uint32_t version;
};
static_assert(sizeof(AssociatedInterface_Data) == 8,
              "Bad_sizeof(AssociatedInterface_Data)");

#pragma pack(pop)

template <typename T>
T FetchAndReset(T* ptr) {
  T temp = *ptr;
  *ptr = T();
  return temp;
}

template <typename T, typename SFINAE = void>
struct IsUnionDataType : std::false_type {
  static_assert(sizeof(T), "T must be a complete type.");
};

template <typename T>
struct IsUnionDataType<T, typename T::MojomUnionDataType> {
  static const bool value = !std::is_const_v<T>;
};

enum class MojomTypeCategory : uint32_t {
  kArray = 1 << 0,
  kAssociatedInterface = 1 << 1,
  kAssociatedInterfaceRequest = 1 << 2,
  kBoolean = 1 << 3,
  kEnum = 1 << 4,
  kHandle = 1 << 5,
  kInterface = 1 << 6,
  kInterfaceRequest = 1 << 7,
  kMap = 1 << 8,
  // POD except boolean and enum.
  kPOD = 1 << 9,
  kString = 1 << 10,
  kStruct = 1 << 11,
  kUnion = 1 << 12
};

inline constexpr MojomTypeCategory operator&(MojomTypeCategory x,
                                             MojomTypeCategory y) {
  return static_cast<MojomTypeCategory>(static_cast<uint32_t>(x) &
                                        static_cast<uint32_t>(y));
}

inline constexpr MojomTypeCategory operator|(MojomTypeCategory x,
                                             MojomTypeCategory y) {
  return static_cast<MojomTypeCategory>(static_cast<uint32_t>(x) |
                                        static_cast<uint32_t>(y));
}

template <typename T>
struct MojomTypeTraits {
  using Data = T;
  using DataAsArrayElement = Data;

  static const MojomTypeCategory category = MojomTypeCategory::kPOD;
};

template <typename T>
struct MojomTypeTraits<std::optional<T>> {
  using Data = std::optional<T>;
  using DataAsArrayElement = Data;

  static const MojomTypeCategory category = MojomTypeCategory::kPOD;
};

template <typename T>
struct MojomTypeTraits<ArrayDataView<T>> {
  using Data = Array_Data<typename MojomTypeTraits<T>::DataAsArrayElement>;
  using DataAsArrayElement = Pointer<Data>;

  static const MojomTypeCategory category = MojomTypeCategory::kArray;
};

template <typename T>
struct MojomTypeTraits<AssociatedInterfacePtrInfoDataView<T>> {
  using Data = AssociatedInterface_Data;
  using DataAsArrayElement = Data;

  static const MojomTypeCategory category =
      MojomTypeCategory::kAssociatedInterface;
};

template <typename T>
struct MojomTypeTraits<AssociatedInterfaceRequestDataView<T>> {
  using Data = AssociatedEndpointHandle_Data;
  using DataAsArrayElement = Data;

  static const MojomTypeCategory category =
      MojomTypeCategory::kAssociatedInterfaceRequest;
};

template <>
struct MojomTypeTraits<bool> {
  using Data = bool;
  using DataAsArrayElement = Data;

  static const MojomTypeCategory category = MojomTypeCategory::kBoolean;
};

template <typename T>
  requires(std::is_enum_v<T>)
struct MojomTypeTraits<T> {
  using Data = int32_t;
  using DataAsArrayElement = Data;

  static const MojomTypeCategory category = MojomTypeCategory::kEnum;
};

template <typename T>
  requires(std::is_enum_v<T>)
struct MojomTypeTraits<std::optional<T>> {
  using Data = std::optional<int32_t>;
  using DataAsArrayElement = Data;

  static const MojomTypeCategory category = MojomTypeCategory::kEnum;
};

template <typename T>
struct MojomTypeTraits<ScopedHandleBase<T>> {
  using Data = Handle_Data;
  using DataAsArrayElement = Data;

  static const MojomTypeCategory category = MojomTypeCategory::kHandle;
};

template <>
struct MojomTypeTraits<PlatformHandle> {
  using Data = Handle_Data;
  using DataAsArrayElement = Data;

  static const MojomTypeCategory category = MojomTypeCategory::kHandle;
};

template <typename T>
struct MojomTypeTraits<InterfacePtrDataView<T>> {
  using Data = Interface_Data;
  using DataAsArrayElement = Data;

  static const MojomTypeCategory category = MojomTypeCategory::kInterface;
};

template <typename T>
struct MojomTypeTraits<InterfaceRequestDataView<T>> {
  using Data = Handle_Data;
  using DataAsArrayElement = Data;

  static const MojomTypeCategory category =
      MojomTypeCategory::kInterfaceRequest;
};

template <typename K, typename V>
struct MojomTypeTraits<MapDataView<K, V>> {
  using Data = Map_Data<typename MojomTypeTraits<K>::DataAsArrayElement,
                        typename MojomTypeTraits<V>::DataAsArrayElement>;
  using DataAsArrayElement = Pointer<Data>;

  static const MojomTypeCategory category = MojomTypeCategory::kMap;
};

template <>
struct MojomTypeTraits<StringDataView> {
  using Data = String_Data;
  using DataAsArrayElement = Pointer<Data>;

  static const MojomTypeCategory category = MojomTypeCategory::kString;
};

template <typename T, MojomTypeCategory categories>
struct BelongsTo {
  static const bool value =
      static_cast<uint32_t>(MojomTypeTraits<T>::category & categories) != 0;
};

template <typename T>
struct EnumHashImpl {
  static_assert(std::is_enum<T>::value, "Incorrect hash function.");

  size_t operator()(T input) const {
    using UnderlyingType = typename std::underlying_type<T>::type;
    return std::hash<UnderlyingType>()(static_cast<UnderlyingType>(input));
  }
};

template <typename MojomType, typename T>
T ConvertEnumValue(MojomType input) {
  T output;
  bool result = EnumTraits<MojomType, T>::FromMojom(input, &output);
  DCHECK(result);
  return output;
}

template <typename MojomType, typename SFINAE = void>
struct EnumKnownValueTraits {
  static MojomType ToKnownValue(MojomType in) { return in; }
};

template <typename MojomType>
struct EnumKnownValueTraits<
    MojomType,
    std::void_t<decltype(ToKnownEnumValue(std::declval<MojomType>()))>> {
  static MojomType ToKnownValue(MojomType in) { return ToKnownEnumValue(in); }
};

template <typename MojomType>
MojomType ToKnownEnumValueHelper(MojomType in) {
  return EnumKnownValueTraits<MojomType>::ToKnownValue(in);
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_
