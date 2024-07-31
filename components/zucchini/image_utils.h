// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_ZUCCHINI_IMAGE_UTILS_H_
#define COMPONENTS_ZUCCHINI_IMAGE_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>

#include "base/format_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/typed_value.h"

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
  virtual std::optional<Reference> GetNext() = 0;
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

// References encoding may be quite complex in some architectures (e.g., ARM),
// requiring bit-level manipulation. In general, bits in a reference body fall
// under 2 categories:
// * Operation bits: Instruction op code, conditionals, or structural data.
// * Payload bits: Actual target data of the reference. These may be absolute,
//   or be displacements relative to instruction pointer / program counter.
// During patch application,
//   Old reference bytes = {old operation, old payload},
// is transformed to
//   New reference bytes = {new operation, new payload}.
// New image bytes are written by three sources:
//   (1) Direct copy from old image to new image for matched blocks.
//   (2) Bytewise diff correction.
//   (3) Dedicated reference target correction.
//
// For references whose operation and payload bits are stored in easily
// separable bytes (e.g., rel32 reference in X86), (2) can exclude payload bits.
// So during patch application, (1) naively copies everything, (2) fixes
// operation bytes only, and (3) fixes payload bytes only.
//
// For architectures with references whose operation and payload bits may mix
// within shared bytes (e.g., ARM rel32), a dilemma arises:
// * (2) cannot ignores shared bytes, since otherwise new operation bits would
//   not properly transfer.
// * Having (2) always overwrite these bytes would reduce the benefits of
//   reference correction, since references are likely to change.
//
// Our solution applies a hybrid approach: For each matching old / new reference
// pair, define:
//   Mixed reference bytes = {new operation, old payload},
//
// During patch generation, we compute bytewise correction from old reference
// bytes to the mixed reference bytes. So during patch application, (2) only
// corrects operation bit changes (and skips if they don't change), and (3)
// overwrites old payload bits to new payload bits.

// Interface for mixed reference byte generation. This base class
// serves as a stub. Architectures whose references store operation bits and
// payload bits can share common bytes (e.g., ARM rel32) should override this.
class ReferenceMixer {
 public:
  virtual ~ReferenceMixer() = default;

  // Computes mixed reference bytes by combining (a) "payload bits" from an
  // "old" reference at |old_offset| with (b) "operation bits" from a "new"
  // reference at |new_offset|. Returns the result as ConstBufferView, which is
  // valid only until the next call to Mix().
  virtual ConstBufferView Mix(offset_t old_offset, offset_t new_offset) = 0;
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
  kExeTypeElfAArch32 = ExeTypeToUint32("EA32"),
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
    case kExeTypeElfAArch32:  // Falls through.
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
  char result[] = {static_cast<char>(v), static_cast<char>(v >> 8),
                   static_cast<char>(v >> 16), static_cast<char>(v >> 24), 0};
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
