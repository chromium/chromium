// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/address_translator.h"

#include <algorithm>
#include <utility>

#include "base/stl_util.h"

namespace zucchini {

/******** AddressTranslator::OffsetToRvaCache ********/

AddressTranslator::OffsetToRvaCache::OffsetToRvaCache(
    const AddressTranslator& translator)
    : translator_(translator) {}

rva_t AddressTranslator::OffsetToRvaCache::Convert(offset_t offset) const {
  if (offset >= translator_.fake_offset_begin_) {
    // Rely on |translator_| to handle this special case.
    return translator_.OffsetToRva(offset);
  }
  if (cached_unit_ && cached_unit_->CoversOffset(offset))
    return cached_unit_->OffsetToRvaUnsafe(offset);
  const AddressTranslator::Unit* unit = translator_.OffsetToUnit(offset);
  if (!unit)
    return kInvalidRva;
  cached_unit_ = unit;
  return unit->OffsetToRvaUnsafe(offset);
}

/******** AddressTranslator::RvaToOffsetCache ********/

AddressTranslator::RvaToOffsetCache::RvaToOffsetCache(
    const AddressTranslator& translator)
    : translator_(translator) {}

bool AddressTranslator::RvaToOffsetCache::IsValid(rva_t rva) const {
  if (rva == kInvalidRva)
    return false;
  if (!cached_unit_ || !cached_unit_->CoversRva(rva)) {
    const AddressTranslator::Unit* unit = translator_.RvaToUnit(rva);
    if (!unit)
      return false;
    cached_unit_ = unit;
  }
  return true;
}

offset_t AddressTranslator::RvaToOffsetCache::Convert(rva_t rva) const {
  if (!cached_unit_ || !cached_unit_->CoversRva(rva)) {
    const AddressTranslator::Unit* unit = translator_.RvaToUnit(rva);
    if (!unit)
      return kInvalidOffset;
    cached_unit_ = unit;
  }
  return cached_unit_->RvaToOffsetUnsafe(rva, translator_.fake_offset_begin_);
}

/******** AddressTranslator ********/

AddressTranslator::AddressTranslator() = default;

AddressTranslator::~AddressTranslator() = default;

AddressTranslator::Status AddressTranslator::Initialize(
    std::vector<Unit>&& units) {
  for (Unit& unit : units) {
    // Check for overflows and fail if found.
    if (!RangeIsBounded<offset_t>(unit.offset_begin, unit.offset_size,
                                  kOffsetBound) ||
        !RangeIsBounded<rva_t>(unit.rva_begin, unit.rva_size, kRvaBound)) {
      return kErrorOverflow;
    }
    // If |rva_size < offset_size|: Just shrink |offset_size| to accommodate.
    unit.offset_size = std::min(unit.offset_size, unit.rva_size);
    // Now |rva_size >= offset_size|. Note that |rva_size > offset_size| is
    // allowed; these lead to dangling RVA.
  }

  // Remove all empty units.
  base::EraseIf(units, [](const Unit& unit) { return unit.IsEmpty(); });

  // Sort |units| by RVA, then uniquefy.
  std::sort(units.begin(), units.end(), [](const Unit& a, const Unit& b) {
    return std::tie(a.rva_begin, a.rva_size) <
           std::tie(b.rva_begin, b.rva_size);
  });
  units.erase(std::unique(units.begin(), units.end()), units.end());

  // Scan for RVA range overlaps, validate, and merge wherever possible.
  if (units.size() > 1) {
    // Traverse with two iterators: |slow| stays behind and modifies Units that
    // absorb all overlapping (or tangent if suitable) Units; |fast| explores
    // new Units as candidates for consistency checks and potential merge into
    // |slow|.
    auto slow = units.begin();

    // All |it| with |slow| < |it| < |fast| contain garbage.
    for (auto fast = slow + 1; fast != units.end(); ++fast) {
      // Comment notation: S = slow offset, F = fast offset, O = overlap offset,
      // s = slow RVA, f = fast RVA, o = overlap RVA.
      DCHECK_GE(fast->rva_begin, slow->rva_begin);
      if (slow->rva_end() < fast->rva_begin) {
        // ..ssssss..ffffff..: Disjoint: Can advance |slow|.
        *(++slow) = *fast;
        continue;
      }

      // ..ssssffff..: Tangent: Merge is optional.
      // ..sssooofff.. / ..sssooosss..: Overlap: Merge is required.
      bool merge_is_optional = slow->rva_end() == fast->rva_begin;

      // Check whether |fast| and |slow| have identical RVA -> offset shift.
      // If not, then merge cannot be resolved. Examples:
      // ..ssssffff.. -> ..SSSSFFFF..: Good, can merge.
      // ..ssssffff.. -> ..SSSS..FFFF..: Non-fatal: don't merge.
      // ..ssssffff.. -> ..FFFF..SSSS..: Non-fatal: don't merge.
      // ..ssssffff.. -> ..SSOOFF..: Fatal: Ignore for now (handled later).
      // ..sssooofff.. -> ..SSSOOOFFF..: Good, can merge.
      // ..sssooofff.. -> ..SSSSSOFFFFF..: Fatal.
      // ..sssooofff.. -> ..FFOOOOSS..: Fatal.
      // ..sssooofff.. -> ..SSSOOOF..: Good, notice |fast| has dangling RVAs.
      // ..oooooo.. -> ..OOOOOO..: Good, can merge.
      if (fast->offset_begin < slow->offset_begin ||
          fast->offset_begin - slow->offset_begin !=
              fast->rva_begin - slow->rva_begin) {
        if (merge_is_optional) {
          *(++slow) = *fast;
          continue;
        }
        return kErrorBadOverlap;
      }

      // Check whether dangling RVAs (if they exist) are consistent. Examples:
      // ..sssooofff.. -> ..SSSOOOF..: Good, can merge.
      // ..sssooosss.. -> ..SSSOOOS..: Good, can merge.
      // ..sssooofff.. -> ..SSSOO..: Good, can merge.
      // ..sssooofff.. -> ..SSSOFFF..: Fatal.
      // ..sssooosss.. -> ..SSSOOFFFF..: Fatal.
      // ..oooooo.. -> ..OOO..: Good, can merge.
      // Idea of check: Suppose |fast| has dangling RVA, then
      // |[fast->rva_start, fast->rva_start + fast->offset_start)| ->
      // |[fast->offset_start, **fast->offset_end()**)|, with remaining RVA
      // mapping to fake offsets. This means |fast->offset_end()| must be >=
      // |slow->offset_end()|, and failure to do so resluts in error. The
      // argument for |slow| havng dangling RVA is symmetric.
      if ((fast->HasDanglingRva() && fast->offset_end() < slow->offset_end()) ||
          (slow->HasDanglingRva() && slow->offset_end() < fast->offset_end())) {
        if (merge_is_optional) {
          *(++slow) = *fast;
          continue;
        }
        return kErrorBadOverlapDanglingRva;
      }

      // Merge |fast| into |slow|.
      slow->rva_size =
          std::max(slow->rva_size, fast->rva_end() - slow->rva_begin);
      slow->offset_size =
          std::max(slow->offset_size, fast->offset_end() - slow->offset_begin);
    }
    ++slow;
    units.erase(slow, units.end());
  }

  // After resolving RVA overlaps, any offset overlap would imply error.
  std::sort(units.begin(), units.end(), [](const Unit& a, const Unit& b) {
    return a.offset_begin < b.offset_begin;
  });

  if (units.size() > 1) {
    auto previous = units.begin();
    for (auto current = previous + 1; current != units.end(); ++current) {
      if (previous->offset_end() > current->offset_begin)
        return kErrorBadOverlap;
      previous = current;
    }
  }

  // For to fake offset heuristics: Compute exclusive upper bounds for offsets
  // and RVAs.
  offset_t offset_bound = 0;
  rva_t rva_bound = 0;
  for (const Unit& unit : units) {
    offset_bound = std::max(offset_bound, unit.offset_end());
    rva_bound = std::max(rva_bound, unit.rva_end());
  }

  // Compute pessimistic range and see if it still fits within space of valid
  // offsets. This limits image size to one half of |kOffsetBound|, and is a
  // main drawback for the current heuristic to convert dangling RVA to fake
  // offsets.
  if (!RangeIsBounded(offset_bound, rva_bound, kOffsetBound))
    return kErrorFakeOffsetBeginTooLarge;

  // Success. Store results. |units| is currently sorted by offset, so assign.
  units_sorted_by_offset_.assign(units.begin(), units.end());

  // Sort |units| by RVA, and just store it directly
  std::sort(units.begin(), units.end(), [](const Unit& a, const Unit& b) {
    return a.rva_begin < b.rva_begin;
  });
  units_sorted_by_rva_ = std::move(units);

  fake_offset_begin_ = offset_bound;
  return kSuccess;
}

rva_t AddressTranslator::OffsetToRva(offset_t offset) const {
  if (offset >= fake_offset_begin_) {
    // Handle dangling RVA: First shift it to regular RVA space.
    rva_t rva = offset - fake_offset_begin_;
    // If result is indeed a dangling RVA, return it; else return |kInvalidRva|.
    const Unit* unit = RvaToUnit(rva);
    return (unit && unit->HasDanglingRva() && unit->CoversDanglingRva(rva))
               ? rva
               : kInvalidRva;
  }
  const Unit* unit = OffsetToUnit(offset);
  return unit ? unit->OffsetToRvaUnsafe(offset) : kInvalidRva;
}

offset_t AddressTranslator::RvaToOffset(rva_t rva) const {
  const Unit* unit = RvaToUnit(rva);
  // This also handles dangling RVA.
  return unit ? unit->RvaToOffsetUnsafe(rva, fake_offset_begin_)
              : kInvalidOffset;
}

const AddressTranslator::Unit* AddressTranslator::OffsetToUnit(
    offset_t offset) const {
  // Finds first Unit with |offset_begin| > |offset|, rewind by 1 to find the
  // last Unit with |offset_begin| >= |offset| (if it exists).
  auto it = std::upper_bound(
      units_sorted_by_offset_.begin(), units_sorted_by_offset_.end(), offset,
      [](offset_t a, const Unit& b) { return a < b.offset_begin; });
  if (it == units_sorted_by_offset_.begin())
    return nullptr;
  --it;
  return it->CoversOffset(offset) ? &(*it) : nullptr;
}

const AddressTranslator::Unit* AddressTranslator::RvaToUnit(rva_t rva) const {
  auto it = std::upper_bound(
      units_sorted_by_rva_.begin(), units_sorted_by_rva_.end(), rva,
      [](rva_t a, const Unit& b) { return a < b.rva_begin; });
  if (it == units_sorted_by_rva_.begin())
    return nullptr;
  --it;
  return it->CoversRva(rva) ? &(*it) : nullptr;
}

}  // namespace zucchini
