// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ADDRESS_TRANSLATOR_H_
#define COMPONENTS_ZUCCHINI_ADDRESS_TRANSLATOR_H_

#include <stdint.h>

#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/zucchini/algorithm.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

// There are several ways to reason about addresses in an image:
// - Offset: Position relative to start of image.
// - VA (Virtual Address): Virtual memory address of a loaded image. This is
//   subject to relocation by the OS.
// - RVA (Relative Virtual Address): VA relative to some base address. This is
//   the preferred way to specify pointers in an image.
//
// Zucchini is primarily concerned with offsets and RVAs. Executable images like
// PE and ELF are organized into sections. Each section specifies offset and RVA
// ranges as:
//   {Offset start, offset size, RVA start, RVA size}.
// This constitutes a basic unit to translate between offsets and RVAs. Note:
// |offset size| < |RVA size| is possible. For example, the .bss section can can
// have zero-filled statically-allocated data that have no corresponding bytes
// on image (to save space). This poses a problem for Zucchini, which stores
// addresses as offsets: now we'd have "dangling RVAs" that don't map to
// offsets! Some ways to handling this are:
// 1. Ignore all dangling RVAs. This simplifies the algorithm, but also means
//    some reference targets would escape detection and processing.
// 2. Create distinct "fake offsets" to accommodate dangling RVAs. Image data
//    must not be read on these fake offsets, which are only valid as target
//    addresses for reference matching.
// As for |RVA size| < |offset size|, the extra portion just gets ignored.
//
// Status: Zucchini implements (2) in a simple way: dangling RVAs are mapped to
// fake offsets by adding a large value. This value can be chosen as an
// exclusive upper bound of all offsets (i.e., image size). This allows them to
// be easily detected and processed as a special-case.
// TODO(huangs): Investigate option (1), now that the refactored code makes
// experimentation easier.
// TODO(huangs): Make AddressTranslator smarter: Allocate unused |offset_t|
// ranges and create "fake" units to accommodate dangling RVAs. Then
// AddressTranslator can be simplified.

// Virtual Address relative to some base address (RVA). There's distinction
// between "valid RVA" and "existent RVA":
// - Valid RVA: An RVA that's reasonably small, i.e., below |kRvaBound|.
// - Existent RVA: An RVA that has semantic meaning in an image, and may
//   translate to an offset in an image or (if a dangling RVA) a fake offset.
//   All existent RVAs are valid RVAs.
using rva_t = uint32_t;
// Divide by 2 to match |kOffsetBound|.
constexpr rva_t kRvaBound = static_cast<rva_t>(-1) / 2;
constexpr rva_t kInvalidRva = static_cast<rva_t>(-2);

// A utility to translate between offsets and RVAs in an image.
class AddressTranslator {
 public:
  // A basic unit for address translation, roughly maps to a section, but may
  // be processed (e.g., merged) as an optimization.
  struct Unit {
    offset_t offset_end() const { return offset_begin + offset_size; }
    rva_t rva_end() const { return rva_begin + rva_size; }
    bool IsEmpty() const {
      // |rva_size == 0| and |offset_size > 0| means Unit hasn't been trimmed
      // yet, and once it is then it's empty.
      // |rva_size > 0| and |offset_size == 0| means Unit has dangling RVA, but
      // is not empty.
      return rva_size == 0;
    }
    bool CoversOffset(offset_t offset) const {
      return RangeCovers(offset_begin, offset_size, offset);
    }
    bool CoversRva(rva_t rva) const {
      return RangeCovers(rva_begin, rva_size, rva);
    }
    bool CoversDanglingRva(rva_t rva) const {
      return CoversRva(rva) && rva - rva_begin >= offset_size;
    }
    // Assumes valid |offset| (*cannot* be fake offset).
    rva_t OffsetToRvaUnsafe(offset_t offset) const {
      return offset - offset_begin + rva_begin;
    }
    // Assumes valid |rva| (*can* be danging RVA).
    offset_t RvaToOffsetUnsafe(rva_t rva, offset_t fake_offset_begin) const {
      rva_t delta = rva - rva_begin;
      return delta < offset_size ? delta + offset_begin
                                 : fake_offset_begin + rva;
    }
    bool HasDanglingRva() const { return rva_size > offset_size; }
    friend bool operator==(const Unit& a, const Unit& b) {
      return std::tie(a.offset_begin, a.offset_size, a.rva_begin, a.rva_size) ==
             std::tie(b.offset_begin, b.offset_size, b.rva_begin, b.rva_size);
    }

    offset_t offset_begin;
    offset_t offset_size;
    rva_t rva_begin;
    rva_t rva_size;
  };

  // An adaptor for AddressTranslator::OffsetToRva() that caches the last Unit
  // found, to reduce the number of OffsetToUnit() calls for clustered queries.
  class OffsetToRvaCache {
   public:
    // Embeds |translator| for use. Now object lifetime is tied to |translator|
    // lifetime.
    explicit OffsetToRvaCache(const AddressTranslator& translator);
    OffsetToRvaCache(const OffsetToRvaCache&) = delete;
    const OffsetToRvaCache& operator=(const OffsetToRvaCache&) = delete;

    rva_t Convert(offset_t offset) const;

   private:
    const raw_ref<const AddressTranslator> translator_;
    mutable raw_ptr<const AddressTranslator::Unit> cached_unit_ = nullptr;
  };

  // An adaptor for AddressTranslator::RvaToOffset() that caches the last Unit
  // found, to reduce the number of RvaToUnit() calls for clustered queries.
  class RvaToOffsetCache {
   public:
    // Embeds |translator| for use. Now object lifetime is tied to |translator|
    // lifetime.
    explicit RvaToOffsetCache(const AddressTranslator& translator);
    RvaToOffsetCache(const RvaToOffsetCache&) = delete;
    const RvaToOffsetCache& operator=(const RvaToOffsetCache&) = delete;

    bool IsValid(rva_t rva) const;

    offset_t Convert(rva_t rva) const;

   private:
    const raw_ref<const AddressTranslator> translator_;
    mutable raw_ptr<const AddressTranslator::Unit> cached_unit_ = nullptr;
  };

  enum Status {
    kSuccess = 0,
    kErrorOverflow,
    kErrorBadOverlap,
    kErrorBadOverlapDanglingRva,
    kErrorFakeOffsetBeginTooLarge,
  };

  AddressTranslator();
  AddressTranslator(AddressTranslator&&);
  AddressTranslator(const AddressTranslator&) = delete;
  const AddressTranslator& operator=(const AddressTranslator&) = delete;
  ~AddressTranslator();

  // Consumes |units| to populate data in this class. Performs consistency
  // checks and overlapping Units. Returns Status to indicate success.
  Status Initialize(std::vector<Unit>&& units);

  // Returns the (possibly dangling) RVA corresponding to |offset|, or
  // kInvalidRva if not found.
  rva_t OffsetToRva(offset_t offset) const;

  // Returns the (possibly fake) offset corresponding to |rva|, or
  // kInvalidOffset if not found (i.e., |rva| is non-existent).
  offset_t RvaToOffset(rva_t rva) const;

  // For testing.
  offset_t fake_offset_begin() const { return fake_offset_begin_; }

  const std::vector<Unit>& units_sorted_by_offset() const {
    return units_sorted_by_offset_;
  }

  const std::vector<Unit>& units_sorted_by_rva() const {
    return units_sorted_by_rva_;
  }

 private:
  // Helper to find the Unit that contains given |offset| or |rva|. Returns null
  // if not found.
  const Unit* OffsetToUnit(offset_t offset) const;
  const Unit* RvaToUnit(rva_t rva) const;

  // Storage of Units. All offset ranges are non-empty and disjoint. Likewise
  // for all RVA ranges.
  std::vector<Unit> units_sorted_by_offset_;
  std::vector<Unit> units_sorted_by_rva_;

  // Conversion factor to translate between dangling RVAs and fake offsets.
  offset_t fake_offset_begin_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ADDRESS_TRANSLATOR_H_
