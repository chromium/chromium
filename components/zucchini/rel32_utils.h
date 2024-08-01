// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_ZUCCHINI_REL32_UTILS_H_
#define COMPONENTS_ZUCCHINI_REL32_UTILS_H_

#include <algorithm>
#include <array>
#include <deque>
#include <memory>
#include <optional>

#include "base/logging.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/arm_utils.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/io_utils.h"

namespace zucchini {

// Reader that emits x86 / x64 References (locations and target) from a list of
// valid locations, constrained by a portion of an image.
class Rel32ReaderX86 : public ReferenceReader {
 public:
  // |image| is an image containing x86 / x64 code in [|lo|, |hi|).
  // |locations| is a sorted list of offsets of rel32 reference locations.
  // |translator| (for |image|) is embedded into |target_rva_to_offset_| and
  // |location_offset_to_rva_| for address translation, and therefore must
  // outlive |*this|.
  Rel32ReaderX86(ConstBufferView image,
                 offset_t lo,
                 offset_t hi,
                 const std::deque<offset_t>* locations,
                 const AddressTranslator& translator);
  Rel32ReaderX86(const Rel32ReaderX86&) = delete;
  const Rel32ReaderX86& operator=(const Rel32ReaderX86&) = delete;
  ~Rel32ReaderX86() override;

  // Returns the next reference, or std::nullopt if exhausted.
  std::optional<Reference> GetNext() override;

 private:
  ConstBufferView image_;
  AddressTranslator::RvaToOffsetCache target_rva_to_offset_;
  AddressTranslator::OffsetToRvaCache location_offset_to_rva_;
  const offset_t hi_;
  const std::deque<offset_t>::const_iterator last_;
  std::deque<offset_t>::const_iterator current_;
};

// Writer for x86 / x64 rel32 References.
class Rel32WriterX86 : public ReferenceWriter {
 public:
  // |image| wraps the raw bytes of a binary in which rel32 references will be
  // written. |translator| (for |image|) is embedded into
  // |target_offset_to_rva_| and |location_offset_to_rva_| for address
  // translation, and therefore must outlive |*this|.
  Rel32WriterX86(MutableBufferView image, const AddressTranslator& translator);
  Rel32WriterX86(const Rel32WriterX86&) = delete;
  const Rel32WriterX86& operator=(const Rel32WriterX86&) = delete;
  ~Rel32WriterX86() override;

  void PutNext(Reference ref) override;

 private:
  MutableBufferView image_;
  AddressTranslator::OffsetToRvaCache target_offset_to_rva_;
  AddressTranslator::OffsetToRvaCache location_offset_to_rva_;
};

// Reader that emits x86 / x64 References (locations and target) of a spcific
// type from a list of valid locations, constrained by a portion of an image.
template <class ADDR_TRAITS>
class Rel32ReaderArm : public ReferenceReader {
 public:
  using CODE_T = typename ADDR_TRAITS::code_t;

  Rel32ReaderArm(const AddressTranslator& translator,
                 ConstBufferView view,
                 const std::deque<offset_t>& rel32_locations,
                 offset_t lo,
                 offset_t hi)
      : view_(view),
        offset_to_rva_(translator),
        rva_to_offset_(translator),
        hi_(hi) {
    cur_it_ =
        std::lower_bound(rel32_locations.begin(), rel32_locations.end(), lo);
    rel32_end_ = rel32_locations.end();
  }

  Rel32ReaderArm(const Rel32ReaderArm&) = delete;
  const Rel32ReaderArm& operator=(const Rel32ReaderArm&) = delete;

  std::optional<Reference> GetNext() override {
    while (cur_it_ < rel32_end_ && *cur_it_ < hi_) {
      offset_t location = *(cur_it_++);
      CODE_T code = ADDR_TRAITS::Fetch(view_, location);
      rva_t instr_rva = offset_to_rva_.Convert(location);
      rva_t target_rva = kInvalidRva;
      if (ADDR_TRAITS::Read(instr_rva, code, &target_rva)) {
        offset_t target = rva_to_offset_.Convert(target_rva);
        if (target != kInvalidOffset)
          return Reference{location, target};
      }
    }
    return std::nullopt;
  }

 private:
  ConstBufferView view_;
  AddressTranslator::OffsetToRvaCache offset_to_rva_;
  AddressTranslator::RvaToOffsetCache rva_to_offset_;
  std::deque<offset_t>::const_iterator cur_it_;
  std::deque<offset_t>::const_iterator rel32_end_;
  offset_t hi_;
};

// Writer for ARM rel32 References of a specific type.
template <class ADDR_TRAITS>
class Rel32WriterArm : public ReferenceWriter {
 public:
  using CODE_T = typename ADDR_TRAITS::code_t;

  Rel32WriterArm(const AddressTranslator& translator,
                 MutableBufferView mutable_view)
      : mutable_view_(mutable_view), offset_to_rva_(translator) {}

  Rel32WriterArm(const Rel32WriterArm&) = delete;
  const Rel32WriterArm& operator=(const Rel32WriterArm&) = delete;

  void PutNext(Reference ref) override {
    CODE_T code = ADDR_TRAITS::Fetch(mutable_view_, ref.location);
    rva_t instr_rva = offset_to_rva_.Convert(ref.location);
    rva_t target_rva = offset_to_rva_.Convert(ref.target);
    if (ADDR_TRAITS::Write(instr_rva, target_rva, &code)) {
      ADDR_TRAITS::Store(mutable_view_, ref.location, code);
    } else {
      LOG(ERROR) << "Write error: " << AsHex<8>(ref.location) << ": "
                 << AsHex<static_cast<int>(sizeof(CODE_T)) * 2>(code)
                 << " <= " << AsHex<8>(target_rva) << ".";
    }
  }

 private:
  MutableBufferView mutable_view_;
  AddressTranslator::OffsetToRvaCache offset_to_rva_;
};

// Copier that makes |*dst_it| similar to |*src_it| (both assumed to point to
// rel32 instructions of type ADDR_TRAITS) by copying the displacement (i.e.,
// payload bits) from |src_it| to |dst_it|. If successful, updates |*dst_it|,
// and returns true. Otherwise returns false. Note that alignment is not an
// issue since the displacement is not translated to target RVA!
template <class ADDR_TRAITS>
bool ArmCopyDisp(ConstBufferView src_view,
                 offset_t src_idx,
                 MutableBufferView dst_view,
                 offset_t dst_idx) {
  using CODE_T = typename ADDR_TRAITS::code_t;
  CODE_T src_code = ADDR_TRAITS::Fetch(src_view, src_idx);
  arm_disp_t disp = 0;
  if (ADDR_TRAITS::Decode(src_code, &disp)) {
    CODE_T dst_code = ADDR_TRAITS::Fetch(dst_view, dst_idx);
    if (ADDR_TRAITS::Encode(disp, &dst_code)) {
      ADDR_TRAITS::Store(dst_view, dst_idx, dst_code);
      return true;
    }
  }
  return false;
}

// Outputs error message (throttled) on ArmCopyDisp failure.
void OutputArmCopyDispFailure(uint32_t addr_type);

// Mixer for ARM rel32 References of a specific type.
template <class ADDR_TRAITS>
class Rel32MixerArm : public ReferenceMixer {
  using code_t = typename ADDR_TRAITS::code_t;
  static constexpr size_t kCodeWidth = sizeof(code_t);

 public:
  Rel32MixerArm(ConstBufferView src_image, ConstBufferView dst_image)
      : src_image_(src_image), dst_image_(dst_image) {}
  ~Rel32MixerArm() override = default;

  ConstBufferView Mix(offset_t src_offset, offset_t dst_offset) override {
    ConstBufferView::const_iterator new_it = dst_image_.begin() + dst_offset;
    MutableBufferView out_buffer_view(out_buffer_.data(), kCodeWidth);
    std::copy(new_it, new_it + kCodeWidth, out_buffer_view.begin());

    if (!ArmCopyDisp<ADDR_TRAITS>(src_image_, src_offset, out_buffer_view,
                                  0U)) {
      OutputArmCopyDispFailure(static_cast<uint32_t>(ADDR_TRAITS::addr_type));
      // Fall back to direct copy.
      std::copy(new_it, new_it + kCodeWidth, out_buffer_.begin());
    }
    return ConstBufferView(out_buffer_.data(), kCodeWidth);
  }

 private:
  ConstBufferView src_image_;
  ConstBufferView dst_image_;

  std::array<uint8_t, sizeof(code_t)> out_buffer_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_REL32_UTILS_H_
