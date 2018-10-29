// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/abs32_utils.h"

#include <algorithm>
#include <type_traits>
#include <utility>

#include "base/logging.h"
#include "components/zucchini/io_utils.h"

namespace zucchini {

namespace {

// Templated helper for AbsoluteAddress::Read().
template <typename T>
bool ReadAbs(ConstBufferView image, offset_t offset, uint64_t* value) {
  static_assert(std::is_unsigned<T>::value, "Value type must be unsigned.");
  if (!image.can_access<T>(offset))
    return false;
  *value = static_cast<uint64_t>(image.read<T>(offset));
  return true;
}

// Templated helper for AbsoluteAddress::Write().
template <typename T>
bool WriteAbs(offset_t offset, T value, MutableBufferView* image) {
  static_assert(std::is_unsigned<T>::value, "Value type must be unsigned.");
  if (!image->can_access<T>(offset))
    return false;
  image->write<T>(offset, value);
  return true;
}

}  // namespace

/******** AbsoluteAddress ********/

AbsoluteAddress::AbsoluteAddress(Bitness bitness, uint64_t image_base)
    : bitness_(bitness), image_base_(image_base), value_(image_base) {
  CHECK(bitness_ == kBit64 || image_base_ < 0x100000000ULL);
}

AbsoluteAddress::AbsoluteAddress(AbsoluteAddress&&) = default;

AbsoluteAddress::~AbsoluteAddress() = default;

bool AbsoluteAddress::FromRva(rva_t rva) {
  if (rva >= kRvaBound)
    return false;
  uint64_t value = image_base_ + rva;
  // Check overflow, which manifests as |value| "wrapping around", resulting in
  // |value| less than |image_base_| (preprocessing needed for 32-bit).
  if (((bitness_ == kBit32) ? (value & 0xFFFFFFFFU) : value) < image_base_)
    return false;
  value_ = value;
  return true;
}

rva_t AbsoluteAddress::ToRva() const {
  if (value_ < image_base_)
    return kInvalidRva;
  uint64_t raw_rva = value_ - image_base_;
  if (raw_rva >= kRvaBound)
    return kInvalidRva;
  return static_cast<rva_t>(raw_rva);
}

bool AbsoluteAddress::Read(offset_t offset, const ConstBufferView& image) {
  // Read raw data; |value_| is not guaranteed to represent a valid RVA.
  if (bitness_ == kBit32)
    return ReadAbs<uint32_t>(image, offset, &value_);
  DCHECK_EQ(kBit64, bitness_);
  return ReadAbs<uint64_t>(image, offset, &value_);
}

bool AbsoluteAddress::Write(offset_t offset, MutableBufferView* image) {
  if (bitness_ == kBit32)
    return WriteAbs<uint32_t>(offset, static_cast<uint32_t>(value_), image);
  DCHECK_EQ(kBit64, bitness_);
  return WriteAbs<uint64_t>(offset, value_, image);
}

/******** Abs32RvaExtractorWin32 ********/

Abs32RvaExtractorWin32::Abs32RvaExtractorWin32(
    ConstBufferView image,
    AbsoluteAddress&& addr,
    const std::vector<offset_t>& abs32_locations,
    offset_t lo,
    offset_t hi)
    : image_(image), addr_(std::move(addr)) {
  CHECK_LE(lo, hi);
  auto find_and_check = [this](const std::vector<offset_t>& locations,
                               offset_t offset) {
    auto it = std::lower_bound(locations.begin(), locations.end(), offset);
    // Ensure that |offset| does not straddle a reference body.
    CHECK(it == locations.begin() || offset - *(it - 1) >= addr_.width());
    return it;
  };
  cur_abs32_ = find_and_check(abs32_locations, lo);
  end_abs32_ = find_and_check(abs32_locations, hi);
}

Abs32RvaExtractorWin32::Abs32RvaExtractorWin32(Abs32RvaExtractorWin32&&) =
    default;

Abs32RvaExtractorWin32::~Abs32RvaExtractorWin32() = default;

base::Optional<Abs32RvaExtractorWin32::Unit> Abs32RvaExtractorWin32::GetNext() {
  while (cur_abs32_ < end_abs32_) {
    offset_t location = *(cur_abs32_++);
    if (!addr_.Read(location, image_))
      continue;
    rva_t target_rva = addr_.ToRva();
    if (target_rva == kInvalidRva)
      continue;
    return Unit{location, target_rva};
  }
  return base::nullopt;
}

/******** Abs32ReaderWin32 ********/

Abs32ReaderWin32::Abs32ReaderWin32(Abs32RvaExtractorWin32&& abs32_rva_extractor,
                                   const AddressTranslator& translator)
    : abs32_rva_extractor_(std::move(abs32_rva_extractor)),
      target_rva_to_offset_(translator) {}

Abs32ReaderWin32::~Abs32ReaderWin32() = default;

base::Optional<Reference> Abs32ReaderWin32::GetNext() {
  for (auto unit = abs32_rva_extractor_.GetNext(); unit.has_value();
       unit = abs32_rva_extractor_.GetNext()) {
    offset_t location = unit->location;
    offset_t unsafe_target = target_rva_to_offset_.Convert(unit->target_rva);
    if (unsafe_target != kInvalidOffset)
      return Reference{location, unsafe_target};
  }
  return base::nullopt;
}

/******** Abs32WriterWin32 ********/

Abs32WriterWin32::Abs32WriterWin32(MutableBufferView image,
                                   AbsoluteAddress&& addr,
                                   const AddressTranslator& translator)
    : image_(image),
      addr_(std::move(addr)),
      target_offset_to_rva_(translator) {}

Abs32WriterWin32::~Abs32WriterWin32() = default;

void Abs32WriterWin32::PutNext(Reference ref) {
  rva_t target_rva = target_offset_to_rva_.Convert(ref.target);
  if (target_rva != kInvalidRva) {
    addr_.FromRva(target_rva);
    addr_.Write(ref.location, &image_);
  }
}

/******** Exported Functions ********/

size_t RemoveUntranslatableAbs32(ConstBufferView image,
                                 AbsoluteAddress&& addr,
                                 const AddressTranslator& translator,
                                 std::vector<offset_t>* locations) {
  AddressTranslator::RvaToOffsetCache target_rva_checker(translator);
  Abs32RvaExtractorWin32 extractor(image, std::move(addr), *locations, 0,
                                   image.size());
  Abs32ReaderWin32 reader(std::move(extractor), translator);
  std::vector<offset_t>::iterator write_it = locations->begin();
  // |reader| reads |locations| while |write_it| modifies it. However, there's
  // no conflict since read occurs before write, and can skip ahead.
  for (auto ref = reader.GetNext(); ref.has_value(); ref = reader.GetNext())
    *(write_it++) = ref->location;
  DCHECK(write_it <= locations->end());
  size_t num_removed = locations->end() - write_it;
  locations->erase(write_it, locations->end());
  return num_removed;
}

size_t RemoveOverlappingAbs32Locations(uint32_t width,
                                       std::vector<offset_t>* locations) {
  if (locations->size() <= 1)
    return 0;

  auto slow = locations->begin();
  auto fast = locations->begin() + 1;
  for (;;) {
    // Find next good location.
    while (fast != locations->end() && *fast - *slow < width)
      ++fast;
    // Advance |slow|. For the last iteration this becomes the new sentinel.
    ++slow;
    if (fast == locations->end())
      break;
    // Compactify good locations (potentially overwrite bad locations).
    if (slow != fast)
      *slow = *fast;
    ++fast;
  }
  size_t num_removed = locations->end() - slow;
  locations->erase(slow, locations->end());
  return num_removed;
}

}  // namespace zucchini
