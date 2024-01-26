// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_RELOC_WIN32_H_
#define COMPONENTS_ZUCCHINI_RELOC_WIN32_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <vector>

#include "base/memory/raw_ref.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/buffer_source.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

// Win32 PE relocation table stores a list of (type, RVA) pairs. The table is
// organized into "blocks" for RVAs with common high-order bits (12-31). Each
// block consists of a list (even length) of 2-byte "units". Each unit stores
// type (in bits 12-15) and low-order bits (0-11) of an RVA (in bits 0-11). In
// pseudo-struct:
//   struct Block {
//     uint32_t rva_hi;
//     uint32_t block_size_in_bytes;  // 8 + multiple of 4.
//     struct {
//       uint16_t rva_lo:12, type:4;  // Little-endian.
//     } units[(block_size_in_bytes - 8) / 2];  // Size must be even.
//   } reloc_table[num_blocks];  // May have padding (type = 0).

// Extracted Win32 reloc Unit data.
struct RelocUnitWin32 {
  RelocUnitWin32();
  RelocUnitWin32(uint8_t type_in, offset_t location_in, rva_t target_rva_in);
  friend bool operator==(const RelocUnitWin32& a, const RelocUnitWin32& b);

  uint8_t type;
  offset_t location;
  rva_t target_rva;
};

// A reader that parses Win32 PE relocation data and emits RelocUnitWin32 for
// each reloc unit that lies strictly inside |[lo, hi)|.
class RelocRvaReaderWin32 {
 public:
  enum : ptrdiff_t { kRelocUnitSize = sizeof(uint16_t) };

  // Parses |image| at |reloc_region| to find beginning offsets of each reloc
  // block. On success, writes the result to |reloc_block_offsets| and returns
  // true. Otherwise leaves |reloc_block_offsets| in an undetermined state, and
  // returns false.
  static bool FindRelocBlocks(ConstBufferView image,
                              BufferRegion reloc_region,
                              std::vector<offset_t>* reloc_block_offsets);

  // |reloc_block_offsets| should be precomputed from FindRelBlocks().
  RelocRvaReaderWin32(ConstBufferView image,
                      BufferRegion reloc_region,
                      const std::vector<offset_t>& reloc_block_offsets,
                      offset_t lo,
                      offset_t hi);
  RelocRvaReaderWin32(RelocRvaReaderWin32&&);
  ~RelocRvaReaderWin32();

  // Successively visits and returns data for each reloc unit, or std::nullopt
  // when all reloc units are found. Encapsulates block transition details.
  std::optional<RelocUnitWin32> GetNext();

 private:
  // Assuming that |block_begin| points to the beginning of a reloc block, loads
  // |rva_hi_bits_| and assigns |cur_reloc_units_| as the region containing the
  // associated units, potentially truncated by |end_it_|. Returns true if reloc
  // data are available for read, and false otherwise.
  bool LoadRelocBlock(ConstBufferView::const_iterator block_begin);

  const ConstBufferView image_;

  // End iterator.
  ConstBufferView::const_iterator end_it_;

  // Unit data of the current reloc block.
  BufferSource cur_reloc_units_;

  // High-order bits (12-31) for all relocs of the current reloc block.
  rva_t rva_hi_bits_;
};

// A reader for Win32 reloc References, implemented as a filtering and
// translation adaptor of RelocRvaReaderWin32.
class RelocReaderWin32 : public ReferenceReader {
 public:
  // Takes ownership of |reloc_rva_reader|. |offset_bound| specifies the
  // exclusive upper bound of reloc target offsets, taking account of widths of
  // targets (which are abs32 References).
  RelocReaderWin32(RelocRvaReaderWin32&& reloc_rva_reader,
                   uint16_t reloc_type,
                   offset_t offset_bound,
                   const AddressTranslator& translator);
  ~RelocReaderWin32() override;

  // ReferenceReader:
  std::optional<Reference> GetNext() override;

 private:
  RelocRvaReaderWin32 reloc_rva_reader_;
  const uint16_t reloc_type_;  // uint16_t to simplify shifting (<< 12).
  const offset_t offset_bound_;
  AddressTranslator::RvaToOffsetCache entry_rva_to_offset_;
};

// A writer for Win32 reloc References. This is simpler than the reader since:
// - No iteration is required.
// - High-order bits of reloc target RVAs are assumed to be handled elsewhere,
//   so only low-order bits need to be written.
class RelocWriterWin32 : public ReferenceWriter {
 public:
  RelocWriterWin32(uint16_t reloc_type,
                   MutableBufferView image,
                   BufferRegion reloc_region,
                   const std::vector<offset_t>& reloc_block_offsets,
                   const AddressTranslator& translator);
  ~RelocWriterWin32() override;

  // ReferenceWriter:
  void PutNext(Reference ref) override;

 private:
  const uint16_t reloc_type_;
  MutableBufferView image_;
  BufferRegion reloc_region_;
  const raw_ref<const std::vector<offset_t>> reloc_block_offsets_;
  AddressTranslator::OffsetToRvaCache target_offset_to_rva_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_RELOC_WIN32_H_
