// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_REFERENCE_BYTES_MIXER_H_
#define COMPONENTS_ZUCCHINI_REFERENCE_BYTES_MIXER_H_

#include <stdint.h>

#include <memory>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/rel32_utils.h"

namespace zucchini {

class Disassembler;

// References encoding may be quite complex in some architectures (e.g., ARM),
// requiring bit-level manipulation. In general, bits in a reference body fall
// under 2 categories:
// - Operation bits: Instruction op code, conditionals, or structural data.
// - Payload bits: Actual target data of the reference. These may be absolute,
//   or be displacements relative to instruction pointer / program counter.
// During patch application,
//   Old reference bytes = {old operation, old payload},
// is transformed to
//   New reference bytes = {new operation, new payload}.
// New image bytes are written by three sources:
//   (1) Direct copy from old image to new image for matched blocks.
//   (2) Bytewise diff correction.
//   (3) Dedicated reference target correction.
//
// For references whose operation and payload bits are stored in easily
// separable bytes (e.g., rel32 reference in X86), (2) can exclude payload bits.
// So during patch application, (1) naively copies everything, (2) fixes
// operation bytes only, and (3) fixes payload bytes only.
//
// For architectures with references whose operation and payload bits may mix
// within shared bytes (e.g., ARM rel32), a dilemma arises:
// - (2) cannot ignores shared bytes, since otherwise new operation bits not
//   properly transfer.
// - Having (2) always overwrite these bytes would reduce the benefits of
//   reference correction, since references are likely to change.
//
// Our solution applies a hybrid approach: For each matching old / new reference
// pair, define:
//   Mixed reference bytes = {new operation, old payload},
//
// During patch generation, we compute bytewise correction from old reference
// bytes to the mixed reference bytes. So during patch application, (2) only
// corrects operation bit changes (and skips if they don't change), and (3)
// overwrites old payload bits to new payload bits.

// A base class for (stateful) mixed reference byte generation. This base class
// serves as a stub. Architectures whose references store operation bits and
// payload bits can share common bytes (e.g., ARM rel32) should override this.
class ReferenceBytesMixer {
 public:
  ReferenceBytesMixer();
  ReferenceBytesMixer(const ReferenceBytesMixer&) = delete;
  const ReferenceBytesMixer& operator=(const ReferenceBytesMixer&) = delete;
  virtual ~ReferenceBytesMixer();

  // Returns a new ReferenceBytesMixer instance that's owned by the caller.
  static std::unique_ptr<ReferenceBytesMixer> Create(
      const Disassembler& src_dis,
      const Disassembler& dst_dis);

  // Returns the number of bytes that need to be mixed for references with given
  // |type|. Returns 0 if no mixing is required.
  virtual int NumBytes(uint8_t type) const;

  // Computes mixed reference bytes by combining (a) "payload bits" from an
  // "old" reference of |type| at |old_view[old_offset]| with (b) "operation
  // bits" from a "new" reference of |type| at |new_view[new_offset]|. Returns
  // the result as ConstBufferView, which is valid only until the next call to
  // Mix().
  virtual ConstBufferView Mix(uint8_t type,
                              ConstBufferView old_view,
                              offset_t old_offset,
                              ConstBufferView new_view,
                              offset_t new_offset);
};

// In AArch32 and AArch64, instructions mix operation bits and payload bits in
// complex ways. This is the main use case of ReferenceBytesMixer.
class ReferenceBytesMixerElfArm : public ReferenceBytesMixer {
 public:
  // |exe_type| must be EXE_TYPE_ELF_ARM or EXE_TYPE_ELF_AARCH64.
  explicit ReferenceBytesMixerElfArm(ExecutableType exe_type);
  ReferenceBytesMixerElfArm(const ReferenceBytesMixerElfArm&) = delete;
  const ReferenceBytesMixerElfArm& operator=(const ReferenceBytesMixerElfArm&) =
      delete;
  ~ReferenceBytesMixerElfArm() override;

  // ReferenceBytesMixer:
  int NumBytes(uint8_t type) const override;
  ConstBufferView Mix(uint8_t type,
                      ConstBufferView old_view,
                      offset_t old_offset,
                      ConstBufferView new_view,
                      offset_t new_offset) override;

 private:
  ArmCopyDispFun GetCopier(uint8_t type) const;

  // For simplicity, 32-bit vs. 64-bit distinction is represented by state
  // |exe_type_|, instead of creating derived classes.
  const ExecutableType exe_type_;

  std::vector<uint8_t> out_buffer_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_REFERENCE_BYTES_MIXER_H_
