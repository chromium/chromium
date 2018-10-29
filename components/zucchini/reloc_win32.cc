// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/reloc_win32.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "components/zucchini/algorithm.h"
#include "components/zucchini/io_utils.h"
#include "components/zucchini/type_win_pe.h"

namespace zucchini {

/******** RelocUnitWin32 ********/

RelocUnitWin32::RelocUnitWin32() = default;
RelocUnitWin32::RelocUnitWin32(uint8_t type_in,
                               offset_t location_in,
                               rva_t target_rva_in)
    : type(type_in), location(location_in), target_rva(target_rva_in) {}

bool operator==(const RelocUnitWin32& a, const RelocUnitWin32& b) {
  return std::tie(a.type, a.location, a.target_rva) ==
         std::tie(b.type, b.location, b.target_rva);
}

/******** RelocRvaReaderWin32 ********/

// static
bool RelocRvaReaderWin32::FindRelocBlocks(
    ConstBufferView image,
    BufferRegion reloc_region,
    std::vector<offset_t>* reloc_block_offsets) {
  CHECK_LT(reloc_region.size, kOffsetBound);
  ConstBufferView reloc_data = image[reloc_region];
  reloc_block_offsets->clear();
  while (reloc_data.size() >= sizeof(pe::RelocHeader)) {
    reloc_block_offsets->push_back(
        base::checked_cast<offset_t>(reloc_data.begin() - image.begin()));
    auto size = reloc_data.read<pe::RelocHeader>(0).size;
    // |size| must be aligned to 4-bytes.
    if (size < sizeof(pe::RelocHeader) || size % 4 || size > reloc_data.size())
      return false;
    reloc_data.remove_prefix(size);
  }
  return reloc_data.empty();  // Fail if trailing data exist.
}

RelocRvaReaderWin32::RelocRvaReaderWin32(
    ConstBufferView image,
    BufferRegion reloc_region,
    const std::vector<offset_t>& reloc_block_offsets,
    offset_t lo,
    offset_t hi)
    : image_(image) {
  CHECK_LE(lo, hi);
  lo = base::checked_cast<offset_t>(reloc_region.InclusiveClamp(lo));
  hi = base::checked_cast<offset_t>(reloc_region.InclusiveClamp(hi));
  end_it_ = image_.begin() + hi;

  // By default, get GetNext() to produce empty output.
  cur_reloc_units_ = BufferSource(end_it_, 0);
  if (reloc_block_offsets.empty())
    return;

  // Find the block that contains |lo|.
  auto block_it = std::upper_bound(reloc_block_offsets.begin(),
                                   reloc_block_offsets.end(), lo);
  DCHECK(block_it != reloc_block_offsets.begin());
  --block_it;

  // Initialize |cur_reloc_units_| and |rva_hi_bits_|.
  if (!LoadRelocBlock(image_.begin() + *block_it))
    return;  // Nothing left.

  // Skip |cur_reloc_units_| to |lo|, truncating up.
  offset_t cur_reloc_units_offset =
      base::checked_cast<offset_t>(cur_reloc_units_.begin() - image_.begin());
  if (lo > cur_reloc_units_offset) {
    offset_t delta =
        AlignCeil<offset_t>(lo - cur_reloc_units_offset, kRelocUnitSize);
    cur_reloc_units_.Skip(delta);
  }
}

RelocRvaReaderWin32::RelocRvaReaderWin32(RelocRvaReaderWin32&&) = default;

RelocRvaReaderWin32::~RelocRvaReaderWin32() = default;

// Unrolls a nested loop: outer = reloc blocks and inner = reloc entries.
base::Optional<RelocUnitWin32> RelocRvaReaderWin32::GetNext() {
  // "Outer loop" to find non-empty reloc block.
  while (cur_reloc_units_.Remaining() < kRelocUnitSize) {
    if (!LoadRelocBlock(cur_reloc_units_.end()))
      return base::nullopt;
  }
  if (end_it_ - cur_reloc_units_.begin() < kRelocUnitSize)
    return base::nullopt;
  // "Inner loop" to extract single reloc unit.
  offset_t location =
      base::checked_cast<offset_t>(cur_reloc_units_.begin() - image_.begin());
  uint16_t entry = cur_reloc_units_.read<uint16_t>(0);
  uint8_t type = static_cast<uint8_t>(entry >> 12);
  rva_t rva = rva_hi_bits_ + (entry & 0xFFF);
  cur_reloc_units_.Skip(kRelocUnitSize);
  return RelocUnitWin32{type, location, rva};
}

bool RelocRvaReaderWin32::LoadRelocBlock(
    ConstBufferView::const_iterator block_begin) {
  ConstBufferView header_buf(block_begin, sizeof(pe::RelocHeader));
  if (header_buf.end() >= end_it_ ||
      end_it_ - header_buf.end() < kRelocUnitSize) {
    return false;
  }
  const auto& header = header_buf.read<pe::RelocHeader>(0);
  rva_hi_bits_ = header.rva_hi;
  uint32_t block_size = header.size;
  if (block_size < sizeof(pe::RelocHeader))
    return false;
  if ((block_size - sizeof(pe::RelocHeader)) % kRelocUnitSize != 0)
    return false;
  cur_reloc_units_ = BufferSource(block_begin, block_size);
  cur_reloc_units_.Skip(sizeof(pe::RelocHeader));
  return true;
}

/******** RelocReaderWin32 ********/

RelocReaderWin32::RelocReaderWin32(RelocRvaReaderWin32&& reloc_rva_reader,
                                   uint16_t reloc_type,
                                   offset_t offset_bound,
                                   const AddressTranslator& translator)
    : reloc_rva_reader_(std::move(reloc_rva_reader)),
      reloc_type_(reloc_type),
      offset_bound_(offset_bound),
      entry_rva_to_offset_(translator) {}

RelocReaderWin32::~RelocReaderWin32() = default;

// ReferenceReader:
base::Optional<Reference> RelocReaderWin32::GetNext() {
  for (base::Optional<RelocUnitWin32> unit = reloc_rva_reader_.GetNext();
       unit.has_value(); unit = reloc_rva_reader_.GetNext()) {
    if (unit->type != reloc_type_)
      continue;
    offset_t target = entry_rva_to_offset_.Convert(unit->target_rva);
    if (target == kInvalidOffset)
      continue;
    // Ensure that |target| (abs32 reference) lies entirely within the image.
    if (target >= offset_bound_)
      continue;
    offset_t location = unit->location;
    return Reference{location, target};
  }
  return base::nullopt;
}

/******** RelocWriterWin32 ********/

RelocWriterWin32::RelocWriterWin32(
    uint16_t reloc_type,
    MutableBufferView image,
    BufferRegion reloc_region,
    const std::vector<offset_t>& reloc_block_offsets,
    const AddressTranslator& translator)
    : reloc_type_(reloc_type),
      image_(image),
      reloc_region_(reloc_region),
      reloc_block_offsets_(reloc_block_offsets),
      target_offset_to_rva_(translator) {}

RelocWriterWin32::~RelocWriterWin32() = default;

void RelocWriterWin32::PutNext(Reference ref) {
  DCHECK_GE(ref.location, reloc_region_.lo());
  DCHECK_LT(ref.location, reloc_region_.hi());
  auto block_it = std::upper_bound(reloc_block_offsets_.begin(),
                                   reloc_block_offsets_.end(), ref.location);
  --block_it;
  rva_t rva_hi_bits = image_.read<pe::RelocHeader>(*block_it).rva_hi;
  rva_t target_rva = target_offset_to_rva_.Convert(ref.target);
  rva_t rva_lo_bits = (target_rva - rva_hi_bits) & 0xFFF;
  if (target_rva != rva_hi_bits + rva_lo_bits) {
    LOG(ERROR) << "Invalid RVA at " << AsHex<8>(ref.location) << ".";
    return;
  }
  image_.write<uint16_t>(ref.location, rva_lo_bits | (reloc_type_ << 12));
}

}  // namespace zucchini
