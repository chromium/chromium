// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/reference_bytes_mixer.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/disassembler_elf.h"

namespace zucchini {

/******** ReferenceBytesMixer ********/

// Default implementation is a stub, i.e., for architectures whose references
// have operation bits and payload bits stored in separate bytes. So during
// patch application, payload bits are copied for matched blocks, ignored by
// bytewise corrections, and fixed by reference target corrections.
ReferenceBytesMixer::ReferenceBytesMixer() {}

ReferenceBytesMixer::~ReferenceBytesMixer() = default;

// static.
std::unique_ptr<ReferenceBytesMixer> ReferenceBytesMixer::Create(
    const Disassembler& src_dis,
    const Disassembler& dst_dis) {
  ExecutableType exe_type = src_dis.GetExeType();
  DCHECK_EQ(exe_type, dst_dis.GetExeType());
  if (exe_type == kExeTypeElfAArch32)
    return std::make_unique<ReferenceBytesMixerElfArm>(exe_type);
  if (exe_type == kExeTypeElfAArch64)
    return std::make_unique<ReferenceBytesMixerElfArm>(exe_type);
  return std::make_unique<ReferenceBytesMixer>();
}

// Stub implementation.
int ReferenceBytesMixer::NumBytes(uint8_t type) const {
  return 0;
}

// Base class implementation is a stub that should not be called.
ConstBufferView ReferenceBytesMixer::Mix(uint8_t type,
                                         ConstBufferView old_view,
                                         offset_t old_offset,
                                         ConstBufferView new_view,
                                         offset_t new_offset) {
  NOTREACHED() << "Stub.";
  return ConstBufferView();
}

/******** ReferenceBytesMixerElfArm ********/

ReferenceBytesMixerElfArm::ReferenceBytesMixerElfArm(ExecutableType exe_type)
    : exe_type_(exe_type), out_buffer_(4) {}  // 4 is a bound on NumBytes().

ReferenceBytesMixerElfArm::~ReferenceBytesMixerElfArm() = default;

int ReferenceBytesMixerElfArm::NumBytes(uint8_t type) const {
  if (exe_type_ == kExeTypeElfAArch32) {
    switch (type) {
      case AArch32ReferenceType::kRel32_A24:  // Falls through.
      case AArch32ReferenceType::kRel32_T20:
      case AArch32ReferenceType::kRel32_T24:
        return 4;
      case AArch32ReferenceType::kRel32_T8:  // Falls through.
      case AArch32ReferenceType::kRel32_T11:
        return 2;
    }
  } else if (exe_type_ == kExeTypeElfAArch64) {
    switch (type) {
      case AArch64ReferenceType::kRel32_Immd14:  // Falls through.
      case AArch64ReferenceType::kRel32_Immd19:
      case AArch64ReferenceType::kRel32_Immd26:
        return 4;
    }
  }
  return 0;
}

ConstBufferView ReferenceBytesMixerElfArm::Mix(uint8_t type,
                                               ConstBufferView old_view,
                                               offset_t old_offset,
                                               ConstBufferView new_view,
                                               offset_t new_offset) {
  int num_bytes = NumBytes(type);
  ConstBufferView::const_iterator new_it = new_view.begin() + new_offset;
  DCHECK_LE(static_cast<size_t>(num_bytes), out_buffer_.size());
  MutableBufferView out_buffer_view(&out_buffer_[0], num_bytes);
  std::copy(new_it, new_it + num_bytes, out_buffer_view.begin());

  ArmCopyDispFun copier = GetCopier(type);
  DCHECK_NE(copier, nullptr);

  if (!copier(old_view, old_offset, out_buffer_view, 0U)) {
    // Failed to mix old payload bits with new operation bits. The main cause of
    // of this rare failure is when BL (encoding T1) with payload bits
    // representing disp % 4 == 2 transforms into BLX (encoding T2). Error
    // arises because BLX requires payload bits to have disp == 0 (mod 4).
    // Mixing failures are not fatal to patching; we simply fall back to direct
    // copy and forgo benefits from mixing for these cases.
    // TODO(huangs, etiennep): Ongoing discussion on whether we should just
    // nullify all payload disp so we won't have to deal with this case, but at
    // the cost of having Zucchini-apply do more work.
    static int output_quota = 10;
    if (output_quota > 0) {
      LOG(WARNING) << "Reference byte mix failed with type = "
                   << static_cast<uint32_t>(type) << "." << std::endl;
      --output_quota;
      if (!output_quota)
        LOG(WARNING) << "(Additional output suppressed)";
    }
    // Fall back to direct copy.
    std::copy(new_it, new_it + num_bytes, out_buffer_view.begin());
  }
  return ConstBufferView(out_buffer_view);
}

ArmCopyDispFun ReferenceBytesMixerElfArm::GetCopier(uint8_t type) const {
  if (exe_type_ == kExeTypeElfAArch32) {
    switch (type) {
      case AArch32ReferenceType::kRel32_A24:
        return ArmCopyDisp<AArch32Rel32Translator::AddrTraits_A24>;
      case AArch32ReferenceType::kRel32_T8:
        return ArmCopyDisp<AArch32Rel32Translator::AddrTraits_T8>;
      case AArch32ReferenceType::kRel32_T11:
        return ArmCopyDisp<AArch32Rel32Translator::AddrTraits_T11>;
      case AArch32ReferenceType::kRel32_T20:
        return ArmCopyDisp<AArch32Rel32Translator::AddrTraits_T20>;
      case AArch32ReferenceType::kRel32_T24:
        return ArmCopyDisp<AArch32Rel32Translator::AddrTraits_T24>;
    }
  } else if (exe_type_ == kExeTypeElfAArch64) {
    switch (type) {
      case AArch64ReferenceType::kRel32_Immd14:
        return ArmCopyDisp<AArch64Rel32Translator::AddrTraits_Immd14>;
      case AArch64ReferenceType::kRel32_Immd19:
        return ArmCopyDisp<AArch64Rel32Translator::AddrTraits_Immd19>;
      case AArch64ReferenceType::kRel32_Immd26:
        return ArmCopyDisp<AArch64Rel32Translator::AddrTraits_Immd26>;
    }
  }
  DLOG(FATAL) << "NOTREACHED";
  return nullptr;
}

}  // namespace zucchini
