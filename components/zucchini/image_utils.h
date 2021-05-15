// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_IMAGE_UTILS_H_
#define COMPONENTS_ZUCCHINI_IMAGE_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/typed_value.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace zucchini {

// offset_t is used to describe an offset in an image.
// Files bigger than 4GB are not supported.
using offset_t = uint32_t;
// Divide by 2 since label marking uses the most significant bit.
constexpr offset_t kOffsetBound = static_cast<offset_t>(-1) / 2;
// Use 0xFFFFFFF*E*, since 0xFFFFFFF*F* is a sentinel value for Dex references.
constexpr offset_t kInvalidOffset = static_cast<offset_t>(-2);

// key_t is used to identify an offset in a table.
using key_t = uint32_t;

enum Bitness : uint8_t {
  // The numerical values are intended to simplify WidthOf() below.
  kBit32 = 4,
  kBit64 = 8
};

inline uint32_t WidthOf(Bitness bitness) {
  return static_cast<uint32_t>(bitness);
}

// Used to uniquely identify a reference type.
// Strongly typed objects are used to avoid ambiguitees with PoolTag.
struct TypeTag : public TypedValue<TypeTag, uint8_t> {
  // inheriting constructor:
  using TypedValue<TypeTag, uint8_t>::TypedValue;
};

// Used to uniquely identify a pool.
struct PoolTag : public TypedValue<PoolTag, uint8_t> {
  // inheriting constructor:
  using TypedValue<PoolTag, uint8_t>::TypedValue;
};

constexpr TypeTag kNoTypeTag(0xFF);  // Typically used to identify raw data.
constexpr PoolTag kNoPoolTag(0xFF);

// Specification of references in an image file.
struct ReferenceTypeTraits {
  constexpr ReferenceTypeTraits(offset_t width_in,
                                TypeTag type_tag_in,
                                PoolTag pool_tag_in)
      : width(width_in), type_tag(type_tag_in), pool_tag(pool_tag_in) {}

  // |width| specifies number of bytes covered by the reference's binary
  // encoding.
  const offset_t width;
  // |type_tag| identifies the reference type being described.
  const TypeTag type_tag;
  // |pool_tag| identifies the pool this type belongs to.
  const PoolTag pool_tag;
};

// There is no need to store |type| because references of the same type are
// always aggregated into the same container, and so during iteration we'd have
// |type| already.
struct Reference {
  offset_t location;
  offset_t target;
};

inline bool operator==(const Reference& a, const Reference& b) {
  return a.location == b.location && a.target == b.target;
}

// Interface for extracting References through member function GetNext().
// This is used by Disassemblers to extract references from an image file.
// Typically, a Reader lazily extracts values and does not hold any storage.
class ReferenceReader {
 public:
  virtual ~ReferenceReader() = default;

  // Returns the next available Reference, or nullopt_t if exhausted.
  // Extracted References must be ordered by their location in the image.
  virtual absl::optional<Reference> GetNext() = 0;
};

// Interface for writing References through member function
// PutNext(reference). This is used by Disassemblers to write new References
// in the image file.
class ReferenceWriter {
 public:
  virtual ~ReferenceWriter() = default;

  // Writes |reference| in the underlying image file. This operation always
  // succeeds.
  virtual void PutNext(Reference reference) = 0;
};

// An Equivalence is a block of length |length| that approximately match in
// |old_image| at an offset of |src_offset| and in |new_image| at an offset of
// |dst_offset|.
struct Equivalence {
  offset_t src_offset;
  offset_t dst_offset;
  offset_t length;

  offset_t src_end() const { return src_offset + length; }
  offset_t dst_end() const { return dst_offset + length; }
};

inline bool operator==(const Equivalence& a, const Equivalence& b) {
  return a.src_offset == b.src_offset && a.dst_offset == b.dst_offset &&
         a.length == b.length;
}

// Same as Equivalence, but with a similarity score. This is only used when
// generating the patch.
struct EquivalenceCandidate {
  Equivalence eq;
  double similarity;
};

template <size_t N>
inline constexpr uint32_t ExeTypeToUint32(const char (&exe_type)[N]) {
  static_assert(N == 5, "Expected ExeType of length 4 + 1 null byte.");
  return (exe_type[3] << 24) | (exe_type[2] << 16) | (exe_type[1] << 8) |
         exe_type[0];
}

// Enumerations for supported executables. Values in this enum must be distinct.
// Once present, values should never be altered or removed to ensure backwards
// compatibility and patch type collision avoidance.
enum ExecutableType : uint32_t {
  kExeTypeUnknown = UINT32_MAX,
  kExeTypeNoOp = ExeTypeToUint32("NoOp"),
  kExeTypeWin32X86 = ExeTypeToUint32("Px86"),
  kExeTypeWin32X64 = ExeTypeToUint32("Px64"),
  kExeTypeElfX86 = ExeTypeToUint32("Ex86"),
  kExeTypeElfX64 = ExeTypeToUint32("Ex64"),
  kExeTypeElfArm32 = ExeTypeToUint32("EA32"),
  kExeTypeElfAArch64 = ExeTypeToUint32("EA64"),
  kExeTypeDex = ExeTypeToUint32("DEX "),
  kExeTypeZtf = ExeTypeToUint32("ZTF "),
};

constexpr ExecutableType CastToExecutableType(uint32_t possible_exe_type) {
  switch (static_cast<ExecutableType>(possible_exe_type)) {
    case kExeTypeNoOp:        // Falls through.
    case kExeTypeWin32X86:    // Falls through.
    case kExeTypeWin32X64:    // Falls through.
    case kExeTypeElfX86:      // Falls through.
    case kExeTypeElfX64:      // Falls through.
    case kExeTypeElfArm32:    // Falls through.
    case kExeTypeElfAArch64:  // Falls through.
    case kExeTypeDex:         // Falls through.
    case kExeTypeZtf:         // Falls through.
    case kExeTypeUnknown:
      return static_cast<ExecutableType>(possible_exe_type);
    default:
      return kExeTypeUnknown;
  }
}

inline std::string CastExecutableTypeToString(ExecutableType exe_type) {
  uint32_t v = static_cast<uint32_t>(exe_type);
  char result[] = {v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF,
                   (v >> 24) & 0xFF, 0};
  return result;
}

// A region in an image with associated executable type |exe_type|. If
// |exe_type == kExeTypeNoOp|, then the Element represents a region of raw data.
struct Element : public BufferRegion {
  Element() = default;
  constexpr Element(const BufferRegion& region_in, ExecutableType exe_type_in)
      : BufferRegion(region_in), exe_type(exe_type_in) {}
  constexpr explicit Element(const BufferRegion& region_in)
      : BufferRegion(region_in), exe_type(kExeTypeNoOp) {}

  // Similar to lo() and hi(), but returns values in offset_t.
  offset_t BeginOffset() const { return base::checked_cast<offset_t>(lo()); }
  offset_t EndOffset() const { return base::checked_cast<offset_t>(hi()); }

  BufferRegion region() const { return {offset, size}; }

  friend bool operator==(const Element& a, const Element& b) {
    return a.exe_type == b.exe_type && a.offset == b.offset && a.size == b.size;
  }

  ExecutableType exe_type;
};

// A matched pair of Elements.
struct ElementMatch {
  bool IsValid() const { return old_element.exe_type == new_element.exe_type; }
  ExecutableType exe_type() const { return old_element.exe_type; }

  // Represents match as "#+#=#+#", where "#" denotes the integers:
  //   [offset in "old", size in "old", offset in "new", size in "new"].
  // Note that element type is omitted.
  std::string ToString() const {
    return base::StringPrintf("%" PRIuS "+%" PRIuS "=%" PRIuS "+%" PRIuS "",
                              old_element.offset, old_element.size,
                              new_element.offset, new_element.size);
  }

  Element old_element;
  Element new_element;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_IMAGE_UTILS_H_
