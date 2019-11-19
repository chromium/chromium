// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_REL32_FINDER_H_
#define COMPONENTS_ZUCCHINI_REL32_FINDER_H_

#include <stddef.h>

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
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
                 const std::vector<offset_t>& abs32_locations,
                 size_t abs32_width);
  ~Abs32GapFinder();

  // Returns the next available gap, or nullopt if exhausted.
  base::Optional<ConstBufferView> GetNext();

 private:
  const ConstBufferView::const_iterator base_;
  const ConstBufferView::const_iterator region_end_;
  ConstBufferView::const_iterator current_lo_;
  std::vector<offset_t>::const_iterator abs32_current_;
  std::vector<offset_t>::const_iterator abs32_end_;
  size_t abs32_width_;

  DISALLOW_COPY_AND_ASSIGN(Abs32GapFinder);
};

// A class to scan regions within an image to find successive rel32 references.
// Architecture-specific parsing and result extraction are delegated to
// inherited classes. This is typically used along with Abs32GapFinder to find
// search regions.
class Rel32Finder {
 public:
  Rel32Finder();
  virtual ~Rel32Finder();

  // Assigns the scan |region| for rel32 references to enable FindNext() use.
  void SetRegion(ConstBufferView region);

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

  // Scans for the next rel32 reference, and returns whether any is found, so a
  // "while" loop can be used for iterative rel32 extraction. The results are
  // cached in Rel32Finder_Impl and obtained by Rel32Finder_Impl::GetRel32().
  bool FindNext();

  // Detects and extracts architecture-specific rel32 reference. For each one
  // found, the implementation should cache the necessary data to be retrieved
  // via accessors. Returns a NextIterators that stores alternatives for where
  // to continue the scan. If no rel32 reference is found then the returned
  // NextIterators are nulls.
  virtual NextIterators Scan(ConstBufferView region) = 0;

 private:
  ConstBufferView region_;
  ConstBufferView::const_iterator accept_it_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Rel32Finder);
};

// Parsing for X86 or X64: we perform naive scan for opcodes that have rel32 as
// an argument, and disregard instruction alignment.
class Rel32FinderIntel : public Rel32Finder {
 public:
  // Struct to store GetNext() results.
  struct Result {
    ConstBufferView::const_iterator location;

    // Some references must have their target in the same section as location,
    // which we use this to heuristically reject rel32 reference candidates.
    // When true, this constraint is relaxed.
    bool can_point_outside_section;
  };

  using Rel32Finder::Rel32Finder;

  // Returns the next available Result, or nullopt if exhausted.
  base::Optional<Result> GetNext() {
    if (FindNext())
      return rel32_;
    return base::nullopt;
  }

 protected:
  // Helper for Scan() that also assigns |rel32_|.
  Rel32Finder::NextIterators SetResult(ConstBufferView::const_iterator cursor,
                                       uint32_t code_size,
                                       bool can_point_outside_section);

  // Cached results.
  Result rel32_;

  // Rel32Finder:
  NextIterators Scan(ConstBufferView region) override = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Rel32FinderIntel);
};

// X86 instructions.
class Rel32FinderX86 : public Rel32FinderIntel {
 public:
  using Rel32FinderIntel::Rel32FinderIntel;

 private:
  // Rel32Finder:
  NextIterators Scan(ConstBufferView region) override;

  DISALLOW_COPY_AND_ASSIGN(Rel32FinderX86);
};

// X64 instructions.
class Rel32FinderX64 : public Rel32FinderIntel {
 public:
  using Rel32FinderIntel::Rel32FinderIntel;

 private:
  // Rel32Finder:
  NextIterators Scan(ConstBufferView region) override;

  DISALLOW_COPY_AND_ASSIGN(Rel32FinderX64);
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_REL32_FINDER_H_
