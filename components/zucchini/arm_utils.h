// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ARM_UTILS_H_
#define COMPONENTS_ZUCCHINI_ARM_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/buffer_view.h"

namespace zucchini {

// References:
// * AArch32 (32-bit ARM, AKA ARM32):
//     https://static.docs.arm.com/ddi0406/c/DDI0406C_C_arm_architecture_reference_manual.pdf
// * AArch64 (64-bit ARM):
//     https://static.docs.arm.com/ddi0487/da/DDI0487D_a_armv8_arm.pdf

// Definitions (used in Zucchini):
// * |instr_rva|: Instruction RVA: The RVA where an instruction is located. In
//   ARM mode and for AArch64 this is 4-byte aligned; in THUMB2 mode this is
//   2-byte aligned.
// * |code|: Instruction code: ARM instruction code as seen in manual. In ARM
//   mode and for AArch64, this is a 32-bit int. In THUMB2 mode, this may be a
//   16-bit or 32-bit int.
// * |disp|: Displacement: For branch instructions (e.g.: B, BL, BLX, and
//   conditional varieties) this is the value encoded in instruction bytes.
// * PC: Program Counter: In ARM mode this is |instr_rva + 8|; in THUMB2 mode
//   this is |instr_rva + 4|; for AArch64 this is |instr_rva|.
// * |target_rva|: Target RVA: The RVA targeted by a branch instruction.
//
// These are related by:
//   |code| = Fetch(image data at offset(|instr_rva|)).
//   |disp| = Decode(|code|).
//   PC = |instr_rva| + {8 in ARM mode, 4 in THUMB2 mode, 0 for AArch64}.
//   |target_rva| = PC + |disp| - (see "BLX complication" below)
//
// Example 1 (ARM mode):
//   00103050: 00 01 02 EA    B     00183458
//   |instr_rva| = 0x00103050  (4-byte aligned).
//   |code| = 0xEA020100  (little endian fetched from data).
//   |disp| = 0x00080400  (decoded from |code| with A24 -> B encoding T1).
//   PC = |instr_rva| + 8 = 0x00103058  (ARM mode).
//   |target_rva| = PC + |disp| = 0x00183458.
//
// Example 2 (THUMB2 mode):
//   001030A2: 00 F0 01 FA    BL    001034A8
//   |instr_rva| = 0x001030A2  (2-byte aligned).
//   |code| = 0xF000FA01  (special THUMB2 mode data fetch).
//   |disp| = 0x00000402  (decoded from |code| with T24 -> BL encoding T1).
//   PC = |instr_rva| + 4 = 0x001030A6  (THUMB2 mode).
//   |target_rva| = PC + |disp| = 0x001034A8.
//
// Example 3 (AArch64):
//   0000000000305070: 03 02 01 14    B     000000000034587C
//   |instr_rva| = 0x00305070  (4-byte aligned, assumed to fit in 32-bit).
//   |code| = 0x14010203  (little endian fetchd from data).
//   |disp| = 0x0004080C  (decoded from |code| with Immd -> B).
//   PC = |instr_rva| = 0x00305070  (AArch64).
//   |target_rva| = PC + |disp| = 0x0034587C.

// BLX complication: BLX transits between ARM mode and THUMB2 mode, and branches
// to an address. Therefore |instr_rva| must align by the "old" mode, and
// |target_rva| must align by the "new" mode. In particular:
// * BLX encoding A2 (ARM -> THUMB2): |instr_rva| is 4-byte aligned with
//   PC = |instr_rva| + 8; |target_rva| is 2-byte aligned, and so |disp| is
//   2-byte aligned.
// * BLX encoding T2 (THUMB2 -> ARM): |instr_rva| is 2-byte aligned with
//   PC = |instr_rva| + 4; |target_rva| is 4-byte aligned. Complication: BLX
//   encoding T2 stores a bit |H| that corresponds to "2" in binary, but |H|
//   must be set to 0. Thus the encoded value is effectively 4-byte aligned. So
//   when computing |target_rva| by adding PC (2-byte aligned) to the stored
//   value (4-byte aligned), the result must be rounded down to the nearest
//   4-byte aligned address.
// The last situation creates ambiguity in how |disp| is defined! Alternatives:
// (1) |disp| := |target_rva| - PC: So |code| <-> |disp| for BLX encoding T2,
//     requires |instr_rva| % 4 to be determined, and adjustments made.
// (2) |disp| := Value stored in |code|: So |disp| <-> |target_rva| for BLX
//     encoding T2 requires adjustment: |disp| -> |target_rva| needs to round
//     down, whereas |target_rva| -> |disp| needs to round up.
// We adopt (2) to simplify |code| <-> |disp|, since that gets used.

using arm_disp_t = int32_t;

// Alignment requirement for |target_rva|, useful for |disp| <-> |target_rva|
// (also requires |instr_rva|). Alignment is determined by parsing |code| in
// *Decode() functions. kArmAlignFail is also defined to indicate parse failure.
// Alignments can be 2 or 4. These values are also used in the enum, so
// |x % align| with |x & (align - 1)| to compute alignment.
enum ArmAlign : uint32_t {
  kArmAlignFail = 0U,
  kArmAlign2 = 2U,
  kArmAlign4 = 4U,
};

// Traits for rel32 address types (technically rel64 for AArch64 -- but we
// assume values are small enough), which form collections of strategies to
// process each rel32 address type.
template <typename ENUM_ADDR_TYPE,
          ENUM_ADDR_TYPE ADDR_TYPE,
          typename CODE_T,
          CODE_T (*FETCH)(ConstBufferView, offset_t),
          void (*STORE)(MutableBufferView, offset_t, CODE_T),
          ArmAlign (*DECODE)(CODE_T, arm_disp_t*),
          bool (*ENCODE)(arm_disp_t, CODE_T*),
          bool (*READ)(rva_t, CODE_T, rva_t*),
          bool (*WRITE)(rva_t, rva_t, CODE_T*)>
class ArmAddrTraits {
 public:
  static constexpr ENUM_ADDR_TYPE addr_type = ADDR_TYPE;
  using code_t = CODE_T;
  static constexpr CODE_T (*Fetch)(ConstBufferView, offset_t) = FETCH;
  static constexpr void (*Store)(MutableBufferView, offset_t, CODE_T) = STORE;
  static constexpr ArmAlign (*Decode)(CODE_T, arm_disp_t*) = DECODE;
  static constexpr bool (*Encode)(arm_disp_t, CODE_T*) = ENCODE;
  static constexpr bool (*Read)(rva_t, CODE_T, rva_t*) = READ;
  static constexpr bool (*Write)(rva_t, rva_t, CODE_T*) = WRITE;
};

// Given THUMB2 instruction |code16|, returns 2 if it's from a 16-bit THUMB2
// instruction, or 4 if it's from a 32-bit THUMB2 instruction.
inline int GetThumb2InstructionSize(uint16_t code16) {
  return ((code16 & 0xF000) == 0xF000 || (code16 & 0xF800) == 0xE800) ? 4 : 2;
}

// A translator for ARM mode and THUMB2 mode with static functions that
// translate among |code|, |disp|, and |target_rva|.
class AArch32Rel32Translator {
 public:
  // Rel32 address types enumeration.
  enum AddrType : uint8_t {
    ADDR_NONE = 0xFF,
    // Naming: Here "A24" represents ARM mode instructions where |code|
    // dedicates 24 bits (including sign bit) to specify |disp|. Similarly, "T8"
    // represents THUMB2 mode instructions with 8 bits for |disp|. Currently
    // only {A24, T8, T11, T20, T24} are defined. These are not to be confused
    // with "B encoding A1", "B encoding T3", etc., which are specific encoding
    // schemes given by the manual for the "B" (or other) instructions (only
    // {A1, A2, T1, T2, T3, T4} are seen).
    ADDR_A24 = 0,
    ADDR_T8,
    ADDR_T11,
    ADDR_T20,
    ADDR_T24,
    NUM_ADDR_TYPE
  };

  AArch32Rel32Translator();
  AArch32Rel32Translator(const AArch32Rel32Translator&) = delete;
  const AArch32Rel32Translator& operator=(const AArch32Rel32Translator&) =
      delete;

  // Fetches the 32-bit ARM instruction |code| at |view[idx]|.
  static inline uint32_t FetchArmCode32(ConstBufferView view, offset_t idx) {
    return view.read<uint32_t>(idx);
  }

  // Fetches the 16-bit THUMB2 instruction |code| at |view[idx]|.
  static inline uint16_t FetchThumb2Code16(ConstBufferView view, offset_t idx) {
    return view.read<uint16_t>(idx);
  }

  // Fetches the 32-bit THUMB2 instruction |code| at |view[idx]|.
  static inline uint32_t FetchThumb2Code32(ConstBufferView view, offset_t idx) {
    // By convention, 32-bit THUMB2 instructions are written (as seen later) as:
    //   [byte3, byte2, byte1, byte0].
    // However (assuming little-endian ARM) the in-memory representation is
    //   [byte2, byte3, byte0, byte1].
    return (static_cast<uint32_t>(view.read<uint16_t>(idx)) << 16) |
           view.read<uint16_t>(idx + 2);
  }

  // Stores the 32-bit ARM instruction |code| to |mutable_view[idx]|.
  static inline void StoreArmCode32(MutableBufferView mutable_view,
                                    offset_t idx,
                                    uint32_t code) {
    mutable_view.write<uint32_t>(idx, code);
  }

  // Stores the 16-bit THUMB2 instruction |code| to |mutable_view[idx]|.
  static inline void StoreThumb2Code16(MutableBufferView mutable_view,
                                       offset_t idx,
                                       uint16_t code) {
    mutable_view.write<uint16_t>(idx, code);
  }

  // Stores the next 32-bit THUMB2 instruction |code| to |mutable_view[idx]|.
  static inline void StoreThumb2Code32(MutableBufferView mutable_view,
                                       offset_t idx,
                                       uint32_t code) {
    mutable_view.write<uint16_t>(idx, static_cast<uint16_t>(code >> 16));
    mutable_view.write<uint16_t>(idx + 2, static_cast<uint16_t>(code & 0xFFFF));
  }

  // The following functions convert |code| (16-bit or 32-bit) from/to |disp|
  // or |target_rva|, for specific branch instruction types.
  // Read*() and write*() functions convert between |code| and |target_rva|.
  // * Decode*() determines whether |code16/code32| is a branch instruction
  //   of a specific type. If so, then extracts |*disp| and returns the required
  //   ArmAlign. Otherwise returns kArmAlignFail.
  // * Encode*() determines whether |*code16/*code32| is a branch instruction of
  //   a specific type, and whether it can accommodate |disp|. If so, then
  //   re-encodes |*code32| using |disp|, and returns true. Otherwise returns
  //   false.
  // * Read*() is similar to Decode*(), but on success, extracts |*target_rva|
  //   using |instr_rva| as aid, performs the proper alignment, and returns
  //   true. Otherwise returns false.
  // * Write*() is similar to Encode*(), takes |target_rva| instead, and uses
  //   |instr_rva| as aid.
  static ArmAlign DecodeA24(uint32_t code32, arm_disp_t* disp);
  static bool EncodeA24(arm_disp_t disp, uint32_t* code32);
  // TODO(huangs): Refactor the Read*() functions: These are identical
  // except for Decode*() and Get*TargetRvaFromDisp().
  static bool ReadA24(rva_t instr_rva, uint32_t code32, rva_t* target_rva);
  static bool WriteA24(rva_t instr_rva, rva_t target_rva, uint32_t* code32);

  static ArmAlign DecodeT8(uint16_t code16, arm_disp_t* disp);
  static bool EncodeT8(arm_disp_t disp, uint16_t* code16);
  static bool ReadT8(rva_t instr_rva, uint16_t code16, rva_t* target_rva);
  static bool WriteT8(rva_t instr_rva, rva_t target_rva, uint16_t* code16);

  static ArmAlign DecodeT11(uint16_t code16, arm_disp_t* disp);
  static bool EncodeT11(arm_disp_t disp, uint16_t* code16);
  static bool ReadT11(rva_t instr_rva, uint16_t code16, rva_t* target_rva);
  static bool WriteT11(rva_t instr_rva, rva_t target_rva, uint16_t* code16);

  static ArmAlign DecodeT20(uint32_t code32, arm_disp_t* disp);
  static bool EncodeT20(arm_disp_t disp, uint32_t* code32);
  static bool ReadT20(rva_t instr_rva, uint32_t code32, rva_t* target_rva);
  static bool WriteT20(rva_t instr_rva, rva_t target_rva, uint32_t* code32);

  static ArmAlign DecodeT24(uint32_t code32, arm_disp_t* disp);
  static bool EncodeT24(arm_disp_t disp, uint32_t* code32);
  static bool ReadT24(rva_t instr_rva, uint32_t code32, rva_t* target_rva);
  static bool WriteT24(rva_t instr_rva, rva_t target_rva, uint32_t* code32);

  // Computes |target_rva| from |instr_rva| and |disp| in ARM mode.
  static inline rva_t GetArmTargetRvaFromDisp(rva_t instr_rva,
                                              arm_disp_t disp,
                                              ArmAlign align) {
    rva_t ret = static_cast<rva_t>(instr_rva + 8 + disp);
    // Align down.
    DCHECK_NE(align, kArmAlignFail);
    return ret - (ret & static_cast<rva_t>(align - 1));
  }

  // Computes |target_rva| from |instr_rva| and |disp| in THUMB2 mode.
  static inline rva_t GetThumb2TargetRvaFromDisp(rva_t instr_rva,
                                                 arm_disp_t disp,
                                                 ArmAlign align) {
    rva_t ret = static_cast<rva_t>(instr_rva + 4 + disp);
    // Align down.
    DCHECK_NE(align, kArmAlignFail);
    return ret - (ret & static_cast<rva_t>(align - 1));
  }

  // Computes |disp| from |instr_rva| and |target_rva| in ARM mode.
  static inline arm_disp_t GetArmDispFromTargetRva(rva_t instr_rva,
                                                   rva_t target_rva,
                                                   ArmAlign align) {
    // Assumes that |instr_rva + 8| does not overflow.
    arm_disp_t ret = static_cast<arm_disp_t>(target_rva) -
                     static_cast<arm_disp_t>(instr_rva + 8);
    // Align up.
    DCHECK_NE(align, kArmAlignFail);
    return ret + ((-ret) & static_cast<arm_disp_t>(align - 1));
  }

  // Computes |disp| from |instr_rva| and |target_rva| in THUMB2 mode.
  static inline arm_disp_t GetThumb2DispFromTargetRva(rva_t instr_rva,
                                                      rva_t target_rva,
                                                      ArmAlign align) {
    // Assumes that |instr_rva + 4| does not overflow.
    arm_disp_t ret = static_cast<arm_disp_t>(target_rva) -
                     static_cast<arm_disp_t>(instr_rva + 4);
    // Align up.
    DCHECK_NE(align, kArmAlignFail);
    return ret + ((-ret) & static_cast<arm_disp_t>(align - 1));
  }

  // Strategies to process each rel32 address type.
  using AddrTraits_A24 = ArmAddrTraits<AddrType,
                                       ADDR_A24,
                                       uint32_t,
                                       FetchArmCode32,
                                       StoreArmCode32,
                                       DecodeA24,
                                       EncodeA24,
                                       ReadA24,
                                       WriteA24>;
  using AddrTraits_T8 = ArmAddrTraits<AddrType,
                                      ADDR_T8,
                                      uint16_t,
                                      FetchThumb2Code16,
                                      StoreThumb2Code16,
                                      DecodeT8,
                                      EncodeT8,
                                      ReadT8,
                                      WriteT8>;
  using AddrTraits_T11 = ArmAddrTraits<AddrType,
                                       ADDR_T11,
                                       uint16_t,
                                       FetchThumb2Code16,
                                       StoreThumb2Code16,
                                       DecodeT11,
                                       EncodeT11,
                                       ReadT11,
                                       WriteT11>;
  using AddrTraits_T20 = ArmAddrTraits<AddrType,
                                       ADDR_T20,
                                       uint32_t,
                                       FetchThumb2Code32,
                                       StoreThumb2Code32,
                                       DecodeT20,
                                       EncodeT20,
                                       ReadT20,
                                       WriteT20>;
  using AddrTraits_T24 = ArmAddrTraits<AddrType,
                                       ADDR_T24,
                                       uint32_t,
                                       FetchThumb2Code32,
                                       StoreThumb2Code32,
                                       DecodeT24,
                                       EncodeT24,
                                       ReadT24,
                                       WriteT24>;
};

// Translator for AArch64, which is simpler than 32-bit ARM. Although pointers
// are 64-bit, displacements are within 32-bit.
class AArch64Rel32Translator {
 public:
  // Rel64 address types enumeration.
  enum AddrType : uint8_t {
    ADDR_NONE = 0xFF,
    ADDR_IMMD14 = 0,
    ADDR_IMMD19,
    ADDR_IMMD26,
    NUM_ADDR_TYPE
  };

  // Although RVA for 64-bit architecture can be 64-bit in length, we make the
  // bold assumption that for ELF images that RVA will stay nicely in 32-bit!
  AArch64Rel32Translator();
  AArch64Rel32Translator(const AArch64Rel32Translator&) = delete;
  const AArch64Rel32Translator& operator=(const AArch64Rel32Translator&) =
      delete;

  static inline uint32_t FetchCode32(ConstBufferView view, offset_t idx) {
    return view.read<uint32_t>(idx);
  }

  static inline void StoreCode32(MutableBufferView mutable_view,
                                 offset_t idx,
                                 uint32_t code) {
    mutable_view.write<uint32_t>(idx, code);
  }

  // Conversion functions for |code32| from/to |disp| or |target_rva|, similar
  // to the counterparts in AArch32Rel32Translator.
  static ArmAlign DecodeImmd14(uint32_t code32, arm_disp_t* disp);
  static bool EncodeImmd14(arm_disp_t disp, uint32_t* code32);
  // TODO(huangs): Refactor the Read*() functions: These are identical
  // except for Decode*().
  static bool ReadImmd14(rva_t instr_rva, uint32_t code32, rva_t* target_rva);
  static bool WriteImmd14(rva_t instr_rva, rva_t target_rva, uint32_t* code32);

  static ArmAlign DecodeImmd19(uint32_t code32, arm_disp_t* disp);
  static bool EncodeImmd19(arm_disp_t disp, uint32_t* code32);
  static bool ReadImmd19(rva_t instr_rva, uint32_t code32, rva_t* target_rva);
  static bool WriteImmd19(rva_t instr_rva, rva_t target_rva, uint32_t* code32);

  static ArmAlign DecodeImmd26(uint32_t code32, arm_disp_t* disp);
  static bool EncodeImmd26(arm_disp_t disp, uint32_t* code32);
  static bool ReadImmd26(rva_t instr_rva, uint32_t code32, rva_t* target_rva);
  static bool WriteImmd26(rva_t instr_rva, rva_t target_rva, uint32_t* code32);

  static inline rva_t GetTargetRvaFromDisp(rva_t instr_rva, arm_disp_t disp) {
    return static_cast<rva_t>(instr_rva + disp);
  }

  static inline arm_disp_t GetDispFromTargetRva(rva_t instr_rva,
                                                rva_t target_rva) {
    return static_cast<arm_disp_t>(target_rva - instr_rva);
  }

  // Strategies to process each rel32 address type.
  using AddrTraits_Immd14 = ArmAddrTraits<AddrType,
                                          ADDR_IMMD14,
                                          uint32_t,
                                          FetchCode32,
                                          StoreCode32,
                                          DecodeImmd14,
                                          EncodeImmd14,
                                          ReadImmd14,
                                          WriteImmd14>;
  using AddrTraits_Immd19 = ArmAddrTraits<AddrType,
                                          ADDR_IMMD19,
                                          uint32_t,
                                          FetchCode32,
                                          StoreCode32,
                                          DecodeImmd19,
                                          EncodeImmd19,
                                          ReadImmd19,
                                          WriteImmd19>;
  using AddrTraits_Immd26 = ArmAddrTraits<AddrType,
                                          ADDR_IMMD26,
                                          uint32_t,
                                          FetchCode32,
                                          StoreCode32,
                                          DecodeImmd26,
                                          EncodeImmd26,
                                          ReadImmd26,
                                          WriteImmd26>;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ARM_UTILS_H_
