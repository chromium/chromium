// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ABS32_UTILS_H_
#define COMPONENTS_ZUCCHINI_ABS32_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

// A class to represent an abs32 address (32-bit or 64-bit). Accessors are
// provided to translate from / to RVA, and to read / write the represented
// abs32 address from / to an image.
class AbsoluteAddress {
 public:
  AbsoluteAddress(Bitness bitness, uint64_t image_base);
  AbsoluteAddress(AbsoluteAddress&&);
  ~AbsoluteAddress();

  // Attempts to translate |rva| to an abs32 address. On success, assigns
  // |value_| to the result and returns true. On failure (invalid |rva| or
  // overflow), returns false.
  bool FromRva(rva_t rva);

  // Returns the RVA for |value_|, or |kInvalidRva| if the represented value
  // address does not correspond to a valid RVA.
  rva_t ToRva() const;

  // Attempts to read the abs32 address at |image[offset]| into |value_|. On
  // success, updates |value_| and returns true. On failure (invalid |offset|),
  // returns false.
  bool Read(offset_t offset, const ConstBufferView& image);

  // Attempts to write |value_| to to |(*image)[offset]|. On success, performs
  // the write and returns true. On failure (invalid |offset|), returns false.
  bool Write(offset_t offset, MutableBufferView* image);

  uint32_t width() const { return WidthOf(bitness_); }

  // Exposing |value_| for testing.
  uint64_t* mutable_value() { return &value_; }

 private:
  const Bitness bitness_;
  const uint64_t image_base_;  // Accommodates 32-bit and 64-bit.
  uint64_t value_;             // Accommodates 32-bit and 64-bit.
};

// A class to extract Win32 abs32 references from |abs32_locations| within
// |image_| bounded by |[lo, hi)|. GetNext() is used to successively return
// data as Units, which are locations and (potentially out-of-bound) RVAs.
// |addr| determines the bitness of abs32 values stored, and mediates all reads.
class Abs32RvaExtractorWin32 {
 public:
  struct Unit {
    offset_t location;
    rva_t target_rva;
  };

  // Requires |lo| <= |hi|, and they must not straddle a reference body (with
  // length |addr.width()|) in |abs32_locations|.
  Abs32RvaExtractorWin32(ConstBufferView image,
                         AbsoluteAddress&& addr,
                         const std::vector<offset_t>& abs32_locations,
                         offset_t lo,
                         offset_t hi);
  Abs32RvaExtractorWin32(Abs32RvaExtractorWin32&&);
  ~Abs32RvaExtractorWin32();

  // Visits given abs32 locations, rejects invalid locations and non-existent
  // RVAs, and returns reference as Unit, or base::nullopt on completion.
  base::Optional<Unit> GetNext();

 private:
  ConstBufferView image_;
  AbsoluteAddress addr_;
  std::vector<offset_t>::const_iterator cur_abs32_;
  std::vector<offset_t>::const_iterator end_abs32_;
};

// A reader for Win32 abs32 references that filters and translates results from
// |abs32_rva_extractor_|.
class Abs32ReaderWin32 : public ReferenceReader {
 public:
  Abs32ReaderWin32(Abs32RvaExtractorWin32&& abs32_rva_extractor,
                   const AddressTranslator& translator);
  ~Abs32ReaderWin32() override;

  // ReferenceReader:
  base::Optional<Reference> GetNext() override;

 private:
  Abs32RvaExtractorWin32 abs32_rva_extractor_;
  AddressTranslator::RvaToOffsetCache target_rva_to_offset_;

  DISALLOW_COPY_AND_ASSIGN(Abs32ReaderWin32);
};

// A writer for Win32 abs32 references. |addr| determines the bitness of the
// abs32 values stored, and mediates all writes.
class Abs32WriterWin32 : public ReferenceWriter {
 public:
  Abs32WriterWin32(MutableBufferView image,
                   AbsoluteAddress&& addr,
                   const AddressTranslator& translator);
  ~Abs32WriterWin32() override;

  // ReferenceWriter:
  void PutNext(Reference ref) override;

 private:
  MutableBufferView image_;
  AbsoluteAddress addr_;
  AddressTranslator::OffsetToRvaCache target_offset_to_rva_;

  DISALLOW_COPY_AND_ASSIGN(Abs32WriterWin32);
};

// Given a list of abs32 |locations|, removes all elements whose targets cannot
// be translated. Returns the number of elements removed.
size_t RemoveUntranslatableAbs32(ConstBufferView image,
                                 AbsoluteAddress&& addr,
                                 const AddressTranslator& translator,
                                 std::vector<offset_t>* locations);

// Given a sorted list of abs32 |locations|, removes all elements whose body
// (with |width| given) overlaps with the body of a previous element.
size_t RemoveOverlappingAbs32Locations(uint32_t width,
                                       std::vector<offset_t>* locations);

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ABS32_UTILS_H_
