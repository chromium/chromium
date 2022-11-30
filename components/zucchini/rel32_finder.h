// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_REL32_FINDER_H_
#define COMPONENTS_ZUCCHINI_REL32_FINDER_H_

#include <stddef.h>

#include <deque>

#include "components/zucchini/address_translator.h"
#include "components/zucchini/arm_utils.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

// See README.md for definitions on abs32 and rel32 references. The following
// are assumed:
// * Abs32 reference bodies have fixed widths.
// * Rel32 locations can be identified by heuristically disassembling machine
//   code, and errors are tolerated.
// * The collection all abs32 and rel32 reference bodies do not overlap.

// A class to visit non-empty contiguous gaps in |region| that lie outside of
// |abs32_locations| elements, each with a body that spans |abs32_width_| bytes.
// For example, given:
//   region = [base_ + 4, base_ + 26),
//   abs32_locations = {2, 6, 15, 20, 27},
//   abs32_width_ = 4,
// the following is obtained:
//             111111111122222222223   -> offsets
//   0123456789012345678901234567890
//   ....**********************.....   -> region = *
//     ^   ^        ^    ^      ^      -> abs32 locations
//     aaaaaaaa     aaaa aaaa   aaaa   -> abs32 bodies
//   ....------*****----*----**.....   -> regions excluding abs32 -> 3 gaps
// The resulting gaps (non-empty, so [6, 6) is excluded) are:
//   [10, 15), [19, 20), [24, 26).
// These gaps can then be passed to Rel32Finder (below) to find rel32 references
// with bodies that are guaranteed to not overlap with any abs32 bodies.
class Abs32GapFinder {
 public:
  // |abs32_locations| is a sorted list of non-overlapping abs32 locations in
  // |image|, each spanning |abs32_width| bytes. Gaps are searched in |region|,
  // which must be part of |image|.
  Abs32GapFinder(ConstBufferView image,
                 ConstBufferView region,
                 const std::deque<offset_t>& abs32_locations,
                 size_t abs32_width);
  Abs32GapFinder(const Abs32GapFinder&) = delete;
  const Abs32GapFinder& operator=(const Abs32GapFinder&) = delete;
  ~Abs32GapFinder();

  // Searches for the next available gap, and returns successfulness.
  bool FindNext();

  // Returns the cached result from the last successful FindNext().
  ConstBufferView GetGap() const { return gap_; }

 private:
  const ConstBufferView::const_iterator base_;
  const ConstBufferView::const_iterator region_end_;
  ConstBufferView::const_iterator cur_lo_;
  const std::deque<offset_t>::const_iterator abs32_end_;
  std::deque<offset_t>::const_iterator abs32_cur_;
  const size_t abs32_width_;
  ConstBufferView gap_;
};

// A class to scan regions within an image to find successive rel32 references.
// Architecture-specific parsing and result extraction are delegated to
// inherited classes (say, Rel32Finder_Impl). Sample extraction loop, combined
// with Abs32GapFinder usage:
//
//   Abs32GapFinder gap_finder(...);
//   Rel32Finder_Impl finder(...);
//   while (gap_finder.FindNext()) {
//     rel_finder.SetRegion(gap_finder.GetGap());
//     while (rel_finder.FindNext()) {
//       auto rel32 = rel_finder.GetRel32();  // In Rel32Finder_Impl.
//       if (architecture_specific_validation(rel32)) {
//         rel_finder.Accept();
//         // Store rel32.
//       }
//     }
//   }
class Rel32Finder {
 public:
  Rel32Finder(ConstBufferView image, const AddressTranslator& translator);
  Rel32Finder(const Rel32Finder&) = delete;
  const Rel32Finder& operator=(const Rel32Finder&) = delete;
  virtual ~Rel32Finder();

  // Assigns the scan |region| for rel32 references to enable FindNext() use.
  void SetRegion(ConstBufferView region);

  // Scans for the next rel32 reference, and returns whether any is found, so a
  // "while" loop can be used for iterative rel32 extraction. The results are
  // cached in Rel32Finder_Impl and obtained by Rel32Finder_Impl::GetRel32().
  bool FindNext();

  // When a rel32 reference is found, the caller needs to decide whether to keep
  // the result (perhaps following more validation). If it decides to keep the
  // result, then it must call Accept(), so the next call to FindNext() can skip
  // the accepted rel32 reference.
  void Accept();

  // Accessors for unittest.
  ConstBufferView::const_iterator accept_it() const { return accept_it_; }
  ConstBufferView region() const { return region_; }

 protected:
  // Alternatives for where to continue the next scan when a rel32 reference is
  // found. nulls indicate that no rel32 references remain.
  struct NextIterators {
    // The next iterator if the caller does not call Accept().
    ConstBufferView::const_iterator reject;

    // The next iterator if the caller calls Accept().
    ConstBufferView::const_iterator accept;
  };

  // Detects and extracts architecture-specific rel32 reference. For each one
  // found, the implementation should cache the necessary data to be retrieved
  // via accessors. Returns a NextIterators that stores alternatives for where
  // to continue the scan. If no rel32 reference is found then the returned
  // NextIterators are nulls.
  virtual NextIterators Scan(ConstBufferView region) = 0;

  const ConstBufferView image_;
  AddressTranslator::OffsetToRvaCache offset_to_rva_;

 private:
  ConstBufferView region_;
  ConstBufferView::const_iterator accept_it_ = nullptr;
};

// Parsing for X86 or X64: we perform naive scan for opcodes that have rel32 as
// an argument, and disregard instruction alignment.
class Rel32FinderIntel : public Rel32Finder {
 public:
  Rel32FinderIntel(const Rel32FinderIntel&) = delete;
  const Rel32FinderIntel& operator=(const Rel32FinderIntel&) = delete;

  // Struct to store GetRel32() results.
  struct Result {
    offset_t location;
    rva_t target_rva;

    // Some references must have their target in the same section as location,
    // which we use this to heuristically reject rel32 reference candidates.
    // When true, this constraint is relaxed.
    bool can_point_outside_section;
  };

  using Rel32Finder::Rel32Finder;

  // Returns the cached result from the last successful FindNext().
  const Result& GetRel32() { return rel32_; }

 protected:
  // Helper for Scan() that also assigns |rel32_|.
  Rel32Finder::NextIterators SetResult(ConstBufferView::const_iterator cursor,
                                       uint32_t code_size,
                                       bool can_point_outside_section);

  // Cached results.
  Result rel32_;

  // Rel32Finder:
  NextIterators Scan(ConstBufferView region) override = 0;
};

// X86 instructions.
class Rel32FinderX86 : public Rel32FinderIntel {
 public:
  using Rel32FinderIntel::Rel32FinderIntel;

  Rel32FinderX86(const Rel32FinderX86&) = delete;
  const Rel32FinderX86& operator=(const Rel32FinderX86&) = delete;

 private:
  // Rel32Finder:
  NextIterators Scan(ConstBufferView region) override;
};

// X64 instructions.
class Rel32FinderX64 : public Rel32FinderIntel {
 public:
  using Rel32FinderIntel::Rel32FinderIntel;

  Rel32FinderX64(const Rel32FinderX64&) = delete;
  const Rel32FinderX64& operator=(const Rel32FinderX64&) = delete;

 private:
  // Rel32Finder:
  NextIterators Scan(ConstBufferView region) override;
};

// Base class for ARM (AArch32 and AArch64) instructions.
template <typename ADDR_TYPE>
class Rel32FinderArm : public Rel32Finder {
 public:
  struct Result {
    offset_t location;
    rva_t target_rva;
    ADDR_TYPE type;

    // For testing.
    bool operator==(const Result& other) const {
      return location == other.location && target_rva == other.target_rva &&
             type == other.type;
    }
  };

  Rel32FinderArm(ConstBufferView image, const AddressTranslator& translator);
  Rel32FinderArm(const Rel32FinderArm&) = delete;
  const Rel32FinderArm& operator=(const Rel32FinderArm&) = delete;
  ~Rel32FinderArm() override;

  // Helper for Scan*() that also assigns |rel32_|.
  NextIterators SetResult(Result&& result,
                          ConstBufferView::const_iterator cursor,
                          int instr_size);

  // SetResult() for end of scan.
  NextIterators SetEmptyResult();

 protected:
  // Cached results.
  Result rel32_;
};

// AArch32 instructions.
class Rel32FinderAArch32
    : public Rel32FinderArm<AArch32Rel32Translator::AddrType> {
 public:
  Rel32FinderAArch32(ConstBufferView image,
                     const AddressTranslator& translator,
                     bool is_thumb2);
  Rel32FinderAArch32(const Rel32FinderAArch32&) = delete;
  const Rel32FinderAArch32& operator=(const Rel32FinderAArch32&) = delete;
  ~Rel32FinderAArch32() override;

  const Result& GetRel32() const { return rel32_; }

 private:
  // Rel32 extraction, assuming segment is in ARM mode.
  NextIterators ScanA32(ConstBufferView region);

  // Rel32 extraction, assuming segment is in THUMB2 mode.
  NextIterators ScanT32(ConstBufferView region);

  // Rel32Finder:
  NextIterators Scan(ConstBufferView region) override;

  // Indicates whether segment is in THUMB2 or ARM mod. In general this can
  // change throughout a section. However, currently we assume that this is
  // constant for an entire section.
  const bool is_thumb2_;
};

// AArch64 instructions.
class Rel32FinderAArch64
    : public Rel32FinderArm<AArch64Rel32Translator::AddrType> {
 public:
  Rel32FinderAArch64(ConstBufferView image,
                     const AddressTranslator& translator);
  Rel32FinderAArch64(const Rel32FinderAArch64&) = delete;
  const Rel32FinderAArch64& operator=(const Rel32FinderAArch64&) = delete;
  ~Rel32FinderAArch64() override;

  const Result& GetRel32() const { return rel32_; }

 private:
  // Rel32Finder:
  NextIterators Scan(ConstBufferView region) override;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_REL32_FINDER_H_
