// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/arm_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <initializer_list>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "components/zucchini/address_translator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace zucchini {

namespace {

// "Clean slate" |code|s for branch instruction encodings with |disp| = 0, and
// if applicable, |cond| = 0.
uint32_t kCleanSlateB_A1 = 0x0A000000;    // A24.
uint32_t kCleanSlateBL_A1 = 0x0B000000;   // A24.
uint32_t kCleanSlateBLX_A2 = 0xFA000000;  // A24.
uint16_t kCleanSlateB_T1 = 0xD000;        // T8.
uint16_t kCleanSlateB_T2 = 0xE000;        // T11.
uint32_t kCleanSlateB_T3 = 0xF0008000;    // T20.
// For T24 encodings, |disp| = 0 means J1 = J2 = 1, so include 0x00002800.
uint32_t kCleanSlateB_T4 = 0xF0009000 | 0x00002800;    // T24.
uint32_t kCleanSlateBL_T1 = 0xF000D000 | 0x00002800;   // T24.
uint32_t kCleanSlateBLX_T2 = 0xF000C000 | 0x00002800;  // T24.

// For AArch64.
uint32_t kCleanSlate64TBZw = 0x36000000;   // Immd14.
uint32_t kCleanSlate64TBZz = 0xB6000000;   // Immd14.
uint32_t kCleanSlate64TBNZw = 0x37000000;  // Immd14.
uint32_t kCleanSlate64TBNZz = 0xB7000000;  // Immd14.
uint32_t kCleanSlate64Bcond = 0x54000000;  // Immd19.
uint32_t kCleanSlate64CBZw = 0x34000000;   // Immd19.
uint32_t kCleanSlate64CBZz = 0xB4000000;   // Immd19.
uint32_t kCleanSlate64CBNZw = 0x35000000;  // Immd19.
uint32_t kCleanSlate64CBNZz = 0xB5000000;  // Immd19.
uint32_t kCleanSlate64B = 0x14000000;      // Immd26.
uint32_t kCleanSlate64BL = 0x94000000;     // Immd26.

// Special case: Cond = 0xE => AL.
uint32_t kCleanSlateBAL_A1 = kCleanSlateB_A1 | (0xE << 28);  //

// Test helper: Extracts |components| from |value| (may be |code| or |disp|)
// based on |pattern|. Also performs consistency checks. On success, writes to
// |*components| and returns true. Otherwise returns false.
// Example (all numbers are in binary):
//   |pattern| = "11110Scc cciiiiii 10(J1)0(J2)jjj jjjj...."
//     |value| =  11110111 00111000 10 1  0 0  111 11000101
// Result: Noting that all 0's and 1's are consistent, returns true with:
//   |*components| = {S: 1, c: 1100, i: 111000, J1: 1, J2: 0, j: 1111100}
// Rules for |pattern|:
// * Spaces are ignored.
// * '.' means "don't care".
// * '0' and '1' are expected literals; mismatch leads to failure.
// * A variable name is specified as:
//   * A single letter.
//   * "(var)", where "var" is a name that begins with a letter.
// * If a variable's first letter is uppercase, then it's a singleton bit.
//   * If repeated, consistency check is applied (must be identical).
// * If a variable's first letter is lowercase, then it spans multiple bits.
//   * These need not be contiguous, but order is preserved (big-endian).
static bool SplitBits(const std::string& pattern,
                      uint32_t value,
                      std::map<std::string, uint32_t>* components) {
  CHECK(components);

  // Split |pattern| into |token_list|.
  std::vector<std::string> token_list;
  size_t bracket_start = std::string::npos;
  for (size_t i = 0; i < pattern.size(); ++i) {
    char ch = pattern[i];
    if (bracket_start == std::string::npos) {
      if (ch == '(')
        bracket_start = i + 1;
      else if (ch != ' ')  // Ignore space.
        token_list.push_back(std::string(1, ch));
    } else if (ch == ')') {
      token_list.push_back(pattern.substr(bracket_start, i - bracket_start));
      bracket_start = std::string::npos;
    }
  }
  CHECK_EQ(std::string::npos, bracket_start);  // No dangling "(".

  // Process each token.
  size_t num_tokens = token_list.size();
  std::map<std::string, uint32_t> temp_components;
  CHECK(num_tokens == 32 || (num_tokens == 16 && value <= 0xFFFF));
  for (size_t i = 0; i < num_tokens; ++i) {
    const std::string& token = token_list[i];
    CHECK(!token.empty());
    uint32_t bit = (value >> (num_tokens - 1 - i)) & 1;
    if (token == "0" || token == "1") {
      if (token[0] != static_cast<char>('0' + bit))
        return false;  // Fail: Mismatch.
    } else if (absl::ascii_isupper(static_cast<unsigned char>(token[0]))) {
      if (temp_components.count(token)) {
        if (temp_components[token] != bit)
          return false;  // Fail: Singleton bit not uniform.
      } else {
        temp_components[token] = bit;
      }
    } else if (absl::ascii_islower(static_cast<unsigned char>(token[0]))) {
      temp_components[token] = (temp_components[token] << 1) | bit;
    } else if (token != ".") {
      return false;  // Fail: Unrecognized token.
    }
  }
  components->swap(temp_components);
  return true;
}

// AArch32 or AArch64 instruction specification for tests. May be 16-bit or
// 32-bit (determined by INT_T).
template <typename INT_T>
struct ArmRelInstruction {
  ArmRelInstruction(const std::string& code_pattern_in, INT_T code)
      : code_pattern(code_pattern_in), clean_slate_code(code) {}

  // Code pattern for SplitBits().
  std::string code_pattern;

  // "Clean slate" |code| encodes |disp| = 0.
  INT_T clean_slate_code;
};

// Tester for ARM Encode / Decode functions for |disp| <-> |code|.
template <typename TRAITS>
class ArmTranslatorEncodeDecodeTest {
 public:
  using CODE_T = typename TRAITS::code_t;

  ArmTranslatorEncodeDecodeTest() = default;

  // For each instruction (with |clean_slate_code| in |instr_list|) and for each
  // |disp| in |good_disp_list|, forms |code| with |encode_fun()| and checks for
  // success. Extracts |disp_out| with |decode_fun()| and checks that it's the
  // original |disp|. For each (|disp|, |code|) pair, extracts components using
  // SplitBits(), and checks that components from |toks_list| are identical. For
  // each |disp| in |bad_disp_list|, checks that |decode_fun_()| fails.
  void Run(const std::string& disp_pattern,
           const std::vector<std::string>& toks_list,
           const std::vector<ArmRelInstruction<CODE_T>>& instr_list,
           const std::vector<arm_disp_t>& good_disp_list,
           const std::vector<arm_disp_t>& bad_disp_list) {
    ArmAlign (*decode_fun)(CODE_T, arm_disp_t*) = TRAITS::Decode;
    bool (*encode_fun)(arm_disp_t, CODE_T*) = TRAITS::Encode;

    for (const ArmRelInstruction<CODE_T> instr : instr_list) {
      // Parse clean slate code bytes, and ensure it's well-formed.
      std::map<std::string, uint32_t> clean_slate_code_components;
      EXPECT_TRUE(SplitBits(instr.code_pattern, instr.clean_slate_code,
                            &clean_slate_code_components));

      for (arm_disp_t disp : good_disp_list) {
        CODE_T code = instr.clean_slate_code;
        // Encode |disp| to |code|.
        EXPECT_TRUE((*encode_fun)(disp, &code)) << disp;
        arm_disp_t disp_out = 0;

        // Extract components (performs consistency checks) and compare.
        std::map<std::string, uint32_t> disp_components;
        EXPECT_TRUE(SplitBits(disp_pattern, static_cast<uint32_t>(disp),
                              &disp_components));
        std::map<std::string, uint32_t> code_components;
        EXPECT_TRUE(SplitBits(instr.code_pattern, code, &code_components));
        for (const std::string& tok : toks_list) {
          EXPECT_EQ(1U, disp_components.count(tok)) << tok;
          EXPECT_EQ(1U, code_components.count(tok)) << tok;
          EXPECT_EQ(disp_components[tok], code_components[tok]) << tok;
        }

        // Decode |code| to |disp_out|, check fidelity.
        EXPECT_NE(kArmAlignFail, (*decode_fun)(code, &disp_out));
        EXPECT_EQ(disp, disp_out);

        // Sanity check: Re-encode |disp| into |code|, ensure no change.
        CODE_T code_copy = code;
        EXPECT_TRUE((*encode_fun)(disp, &code));
        EXPECT_EQ(code_copy, code);

        // Encode 0, ensure we get clean slate |code| back.
        EXPECT_TRUE((*encode_fun)(0, &code));
        EXPECT_EQ(instr.clean_slate_code, code);
      }

      for (arm_disp_t disp : bad_disp_list) {
        CODE_T code = instr.clean_slate_code;
        EXPECT_FALSE((*encode_fun)(disp, &code)) << disp;
        // Value does not get modified after failure.
        EXPECT_EQ(instr.clean_slate_code, code);
      }
    }
  }
};

// Tester for ARM Write / Read functions for |target_rva| <-> |code|.
template <typename TRAITS>
class ArmTranslatorWriteReadTest {
 public:
  using CODE_T = typename TRAITS::code_t;

  ArmTranslatorWriteReadTest() = default;

  // Expects successful Write() to |clean_slate_code| for each |target_rva_list|
  // RVA, using each |instr_rva_list| RVA, and that the resulting |code| leads
  // to successful Read(), which recovers |instr_rva|.
  void Accept(CODE_T clean_slate_code,
              const std::vector<rva_t>& instr_rva_list,
              const std::vector<rva_t>& target_rva_list) {
    bool (*read_fun)(rva_t, CODE_T, rva_t*) = TRAITS::Read;
    bool (*write_fun)(rva_t, rva_t, CODE_T*) = TRAITS::Write;

    for (rva_t instr_rva : instr_rva_list) {
      for (rva_t target_rva : target_rva_list) {
        CODE_T code = clean_slate_code;
        // Write |target_rva| to |code|.
        EXPECT_TRUE((*write_fun)(instr_rva, target_rva, &code)) << target_rva;
        rva_t target_rva_out = kInvalidRva;

        // Read |code| to |target_rva_out|, check fidelity.
        EXPECT_TRUE((*read_fun)(instr_rva, code, &target_rva_out));
        EXPECT_EQ(target_rva, target_rva_out);

        // Sanity check: Rewrite |target_rva| into |code|, ensure no change.
        CODE_T code_copy = code;
        EXPECT_TRUE((*write_fun)(instr_rva, target_rva, &code));
        EXPECT_EQ(code_copy, code);
      }
    }
  }

  // Expects failed Write() to |clean_slate_code| for each |target_rva_list|
  // RVA, using each |instr_rva_list| RVA.
  void Reject(CODE_T clean_slate_code,
              const std::vector<rva_t>& instr_rva_list,
              const std::vector<rva_t>& target_rva_list) {
    bool (*write_fun)(rva_t, rva_t, CODE_T*) = TRAITS::Write;

    for (rva_t instr_rva : instr_rva_list) {
      for (rva_t target_rva : target_rva_list) {
        CODE_T code = clean_slate_code;
        EXPECT_FALSE((*write_fun)(instr_rva, target_rva, &code)) << target_rva;
        // Output variable is unmodified after failure.
        EXPECT_EQ(clean_slate_code, code);
      }
    }
  }
};

}  // namespace

// Test for test helper.
TEST(ArmUtilsTest, SplitBits) {
  // If |expected| == "BAD" then we expect failure.
  auto run_test = [](const std::string& expected, const std::string& pattern,
                     uint32_t value) {
    std::map<std::string, uint32_t> components;
    if (expected == "BAD") {
      EXPECT_FALSE(SplitBits(pattern, value, &components));
      EXPECT_TRUE(components.empty());
    } else {
      EXPECT_TRUE(SplitBits(pattern, value, &components));
      std::ostringstream oss;
      // Not using AsHex<>, since number of digits is not fixed.
      oss << std::uppercase << std::hex;
      std::string sep = "";
      for (auto it : components) {
        oss << sep << it.first << "=" << it.second;
        sep = ",";
      }
      EXPECT_EQ(expected, oss.str());
    }
  };

  run_test("a=ABCD0123", "aaaaaaaa aaaaaaaa aaaaaaaa aaaaaaaa", 0xABCD0123);
  run_test("a=ABCD,b=123", "aaaaaaaa aaaaaaaa bbbbbbbb bbbbbbbb", 0xABCD0123);
  run_test("a=23,b=1,c=CD,d=AB", "dddddddd cccccccc bbbbbbbb aaaaaaaa",
           0xABCD0123);
  run_test("", "........ ........ ........ ........", 0xABCD0123);
  run_test("t=AC02", " tttt.... tt tt.... tttt....tttt....  ", 0xABCD0123);

  run_test("a=8,b=C,c=E,d1=F", "aaaabbbb cccc(d1)(d1)(d1)(d1)", 0x8CEF);
  run_test("a=F,b=7,c=3,d1=1", "abc(d1)abc(d1) abc(d1)abc(d1)", 0x8CEF);

  run_test("A1=0,X=1", "(A1)XX(A1) X(A1)(A1)(A1)   (X)(A1)(X)X(X)(X)X(A1)",
           0x68BE);
  run_test("BAD", "(A1)XX(A1) X(A1)(A1)(A1)   (X)(A1)(X)X(X)(X)X(A1)", 0x68BF);
  run_test("BAD", "(A1)XX(A1) X(A1)(A1)(A1)   (X)(A1)(X)X(X)(X)X(A1)", 0x683E);

  run_test("A=1,B=0,a=C", "AAAAaaaa BBBB01..", 0xFC06);
  run_test("A=1,B=0,a=4", "AAAAaaaa BBBB01..", 0xF406);
  run_test("A=0,B=1,a=C", "AAAAaaaa BBBB01..", 0x0CF5);
  run_test("BAD", "AAAAaaaa BBBB01..", 0xEC06);  // Non-uniform A.
  run_test("BAD", "AAAAaaaa BBBB01..", 0xFC16);  // Non-uniform B.
  run_test("BAD", "AAAAaaaa BBBB01..", 0xFC02);  // Constant mismatch.
}

TEST(AArch32Rel32Translator, Fetch) {
  std::vector<uint8_t> bytes = {0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE};
  ConstBufferView region(&bytes[0], bytes.size());
  AArch32Rel32Translator translator;
  EXPECT_EQ(0x76543210U, translator.FetchArmCode32(region, 0U));
  EXPECT_EQ(0xFEDCBA98U, translator.FetchArmCode32(region, 4U));

  EXPECT_EQ(0x3210U, translator.FetchThumb2Code16(region, 0U));
  EXPECT_EQ(0xFEDCU, translator.FetchThumb2Code16(region, 6U));

  EXPECT_EQ(0x32107654U, translator.FetchThumb2Code32(region, 0U));
  EXPECT_EQ(0xBA98FEDCU, translator.FetchThumb2Code32(region, 4U));
}

TEST(AArch32Rel32Translator, Store) {
  std::vector<uint8_t> expected = {
      0xFF, 0xFF, 0xFF, 0xFF,  // Padding.
      0x10, 0x32, 0x54, 0x76,  // ARM 32-bit.
      0xFF, 0xFF,              // Padding.
      0x42, 0x86,              // THUMB2 16-bit.
      0xFF, 0xFF,              // Padding.
      0xDC, 0xFE, 0x98, 0xBA,  // THUMB2 32-bit.
      0xFF, 0xFF, 0xFF, 0xFF   // Padding.
  };

  std::vector<uint8_t> bytes(4 * 2 + 2 * 3 + 4 * 2, 0xFF);
  MutableBufferView region(&bytes[0], bytes.size());
  CHECK_EQ(expected.size(), bytes.size());

  AArch32Rel32Translator translator;
  translator.StoreArmCode32(region, 4U, 0x76543210U);
  translator.StoreThumb2Code16(region, 10U, 0x8642U);
  translator.StoreThumb2Code32(region, 14U, 0xFEDCBA98U);

  EXPECT_EQ(expected, bytes);
}

// Detailed test of Encode/Decode: Check valid and invalid |disp| for various
// clean slate |code| cases. Also check |disp| and |code| binary components,
// which in AArch32Rel32Translator comments.
TEST(AArch32Rel32Translator, EncodeDecode) {
  // A24 tests.
  ArmTranslatorEncodeDecodeTest<AArch32Rel32Translator::AddrTraits_A24>
      test_A24;
  for (int cond = 0; cond <= 0x0E; ++cond) {
    ArmRelInstruction<uint32_t> B_A1_cond("cccc1010 Siiiiiii iiiiiiii iiiiiiii",
                                          kCleanSlateB_A1 | (cond << 28));
    ArmRelInstruction<uint32_t> BL_A1_cond(
        "cccc1011 Siiiiiii iiiiiiii iiiiiiii", kCleanSlateBL_A1 | (cond << 28));
    test_A24.Run("SSSSSSSi iiiiiiii iiiiiiii iiiiii00", {"S", "i"},
                 {B_A1_cond, BL_A1_cond},
                 {0x01FFFFFC, -0x02000000, 0, 4, -4, 0x40, 0x44},
                 {2, -2, 0x41, 0x42, 0x43, 0x02000000, -0x02000004});
  }
  // BLX encoding A2, which has 2-byte alignment.
  ArmRelInstruction<uint32_t> BLX_A2("1111101H Siiiiiii iiiiiiii iiiiiiii",
                                     kCleanSlateBLX_A2);
  test_A24.Run("SSSSSSSi iiiiiiii iiiiiiii iiiiiiH0", {"S", "i", "H"}, {BLX_A2},
               {0x01FFFFFC, 0x01FFFFFE, -0x02000000, 0, 2, -2, 4, 0x40, 0x42},
               {1, -1, 0x41, 0x43, 0x02000000, -0x02000002});

  // T8 tests.
  ArmTranslatorEncodeDecodeTest<AArch32Rel32Translator::AddrTraits_T8> test_T8;
  for (int cond = 0; cond <= 0x0E; ++cond) {
    ArmRelInstruction<uint16_t> B_T1_cond("1101cccc Siiiiiii",
                                          kCleanSlateB_T1 | (cond << 8));
    test_T8.Run("SSSSSSSS SSSSSSSS SSSSSSSS iiiiiii0", {"S", "i"}, {B_T1_cond},
                {0x00FE, -0x0100, 0, 2, -2, 4, 0x40, 0x42},
                {1, -1, 0x41, 0x43, 0x0100, -0x0102});
  }
  ArmRelInstruction<uint16_t> B_T1_invalid("11011111 ........",
                                           kCleanSlateB_T1 | (0x0F << 8));
  test_T8.Run("........ ........ ........ ........", std::vector<std::string>(),
              {B_T1_invalid}, std::vector<arm_disp_t>(),
              {0x00FE, -0x0100, 0, 2, 4, 0x40, 0x41, 0x0100, -0x0102});

  // T11 tests.
  ArmTranslatorEncodeDecodeTest<AArch32Rel32Translator::AddrTraits_T11>
      test_T11;
  ArmRelInstruction<uint16_t> B_T2("11100Sii iiiiiiii", kCleanSlateB_T2);
  test_T11.Run("SSSSSSSS SSSSSSSS SSSSSiii iiiiiii0", {"S", "i"}, {B_T2},
               {0x07FE, -0x0800, 0, 2, -2, 4, 0x40, 0x42},
               {1, -1, 0x41, 0x43, 0x0800, -0x0802});

  // T20 tests.
  ArmTranslatorEncodeDecodeTest<AArch32Rel32Translator::AddrTraits_T20>
      test_T20;
  for (int cond = 0; cond <= 0x0E; ++cond) {
    ArmRelInstruction<uint32_t> B_T3_cond(
        "11110Scc cciiiiii 10(J1)0(J2)jjj jjjjjjjj",
        kCleanSlateB_T3 | (cond << 22));
    test_T20.Run("SSSSSSSS SSSS(J2)(J1)ii iiiijjjj jjjjjjj0",
                 {"S", "J2", "J1", "i", "j"}, {B_T3_cond},
                 {0x000FFFFE, -0x00100000, 0, 2, -2, 4, 0x40, 0x42},
                 {1, -1, 0x41, 0x43, 0x00100000, -0x00100002});
  }
  ArmRelInstruction<uint32_t> B_T3_invalid(
      "11110.11 11...... 10.0.... ........", kCleanSlateB_T3 | (0x0F << 22));
  test_T20.Run("........ ........ ........ ........",
               std::vector<std::string>(), {B_T3_invalid},
               std::vector<arm_disp_t>(),
               {0x000FFFFE, -0x00100000, 0, 2, 4, 0x40, 0x42, 1, 0x41, 0x43,
                0x00100000, -0x00100002});

  // T24 tests.
  ArmTranslatorEncodeDecodeTest<AArch32Rel32Translator::AddrTraits_T24>
      test_T24;
  // "Clean slate" means J1 = J2 = 1, so we include 0x00002800.
  ArmRelInstruction<uint32_t> B_T4("11110Sii iiiiiiii 10(J1)1(J2)jjj jjjjjjjj",
                                   kCleanSlateB_T4);
  ArmRelInstruction<uint32_t> BL_T1("11110Sii iiiiiiii 11(J1)1(J2)jjj jjjjjjjj",
                                    kCleanSlateBL_T1);
  test_T24.Run("SSSSSSSS (I1)(I2)iiiiii iiiijjjj jjjjjjj0",
               {"S", "i", "j"},  // Skip "J1", "J2", "I1", "I2" checks.
               {B_T4, BL_T1},
               {0x00FFFFFE, -0x01000000, 0, 2, -2, 4, -4, 0x40, 0x42},
               {1, -1, 0x41, 0x43, 0x01000000, -0x01000002});

  // For BLX encoding T2, |disp| must be multiple of 4.
  ArmRelInstruction<uint32_t> BLX_T2(
      "11110Sii iiiiiiii 11(J1)0(J2)jjj jjjjjjj0", kCleanSlateBLX_T2);
  test_T24.Run(
      "SSSSSSSS (I1)(I2)iiiiii iiiijjjj jjjjjj00",
      {"S", "i", "j"},  // Skip "J1", "J2", "I1", "I2" checks.
      {BLX_T2}, {0x00FFFFFC, -0x01000000, 0, 4, -4, 0x40},
      {1, -1, 2, -2, 0x41, 0x42, 0x43, 0x00FFFFFE, 0x01000000, -0x01000002});
}

TEST(AArch32Rel32Translator, WriteRead) {
  std::vector<rva_t> aligned4;
  std::vector<rva_t> misaligned4;
  std::vector<rva_t> aligned2;
  std::vector<rva_t> misaligned2;
  for (rva_t rva = 0x1FFC; rva <= 0x2010; ++rva) {
    ((rva % 4 == 0) ? aligned4 : misaligned4).push_back(rva);
    ((rva % 2 == 0) ? aligned2 : misaligned2).push_back(rva);
  }
  CHECK_EQ(6U, aligned4.size());
  CHECK_EQ(15U, misaligned4.size());
  CHECK_EQ(11U, aligned2.size());
  CHECK_EQ(10U, misaligned2.size());

  // Helpers to convert an instruction's RVA to PC.
  auto pcArm = [](rva_t instr_rva) -> rva_t { return instr_rva + 8; };
  auto pcThumb2 = [](rva_t instr_rva) -> rva_t { return instr_rva + 4; };

  // A24 tests.
  ArmTranslatorWriteReadTest<AArch32Rel32Translator::AddrTraits_A24> test_A24;
  for (uint32_t clean_slate_code : {kCleanSlateB_A1, kCleanSlateBL_A1}) {
    test_A24.Accept(clean_slate_code, aligned4, aligned4);
    test_A24.Reject(clean_slate_code, aligned4, misaligned4);
    test_A24.Reject(clean_slate_code, misaligned4, aligned4);
    test_A24.Reject(clean_slate_code, misaligned4, misaligned4);
    // Signed (24 + 2)-bit range, 4-byte aligned: [-0x02000000, 0x01FFFFFC].
    test_A24.Accept(clean_slate_code, {0x15000000},
                    {pcArm(0x13000000), pcArm(0x16FFFFFC)});
    test_A24.Reject(clean_slate_code, {0x15000000},
                    {pcArm(0x13000000 - 4), pcArm(0x16FFFFFC + 4)});
  }

  // BLX complication: ARM -> THUMB2.
  test_A24.Accept(kCleanSlateBLX_A2, aligned4, aligned2);
  test_A24.Reject(kCleanSlateBLX_A2, aligned4, misaligned2);
  test_A24.Reject(kCleanSlateBLX_A2, misaligned4, aligned2);
  test_A24.Reject(kCleanSlateBLX_A2, misaligned4, misaligned2);
  test_A24.Accept(kCleanSlateBLX_A2, {0x15000000},
                  {pcArm(0x13000000), pcArm(0x16FFFFFE)});
  test_A24.Reject(kCleanSlateBLX_A2, {0x15000000},
                  {pcArm(0x13000000 - 4), pcArm(0x13000000 - 2),
                   pcArm(0x16FFFFFE + 2), pcArm(0x16FFFFFE + 4)});

  // T8 tests.
  ArmTranslatorWriteReadTest<AArch32Rel32Translator::AddrTraits_T8> test_T8;
  test_T8.Accept(kCleanSlateB_T1, aligned2, aligned2);
  test_T8.Reject(kCleanSlateB_T1, aligned2, misaligned2);
  test_T8.Reject(kCleanSlateB_T1, misaligned2, aligned2);
  test_T8.Reject(kCleanSlateB_T1, misaligned2, misaligned2);
  // Signed (8 + 1)-bit range, 2-byte aligned: [-0x0100, 0x00FE].
  test_T8.Accept(kCleanSlateB_T1, {0x10000500},
                 {pcThumb2(0x10000400), pcThumb2(0x100005FE)});
  test_T8.Reject(kCleanSlateB_T1, {0x10000500},
                 {pcThumb2(0x10000400 - 2), pcThumb2(0x100005FE + 2)});

  // T11 tests.
  ArmTranslatorWriteReadTest<AArch32Rel32Translator::AddrTraits_T11> test_T11;
  test_T11.Accept(kCleanSlateB_T2, aligned2, aligned2);
  test_T11.Reject(kCleanSlateB_T2, aligned2, misaligned2);
  test_T11.Reject(kCleanSlateB_T2, misaligned2, aligned2);
  test_T11.Reject(kCleanSlateB_T2, misaligned2, misaligned2);
  // Signed (11 + 1)-bit range, 2-byte aligned: [-0x0800, 0x07FE].
  test_T11.Accept(kCleanSlateB_T2, {0x10003000},
                  {pcThumb2(0x10002800), pcThumb2(0x100037FE)});
  test_T11.Reject(kCleanSlateB_T2, {0x10003000},
                  {pcThumb2(0x10002800 - 2), pcThumb2(0x100037FE + 2)});

  // T20 tests.
  ArmTranslatorWriteReadTest<AArch32Rel32Translator::AddrTraits_T20> test_T20;
  test_T20.Accept(kCleanSlateB_T3, aligned2, aligned2);
  test_T20.Reject(kCleanSlateB_T3, aligned2, misaligned2);
  test_T20.Reject(kCleanSlateB_T3, misaligned2, aligned2);
  test_T20.Reject(kCleanSlateB_T3, misaligned2, misaligned2);
  // Signed (20 + 1)-bit range, 2-byte aligned: [-0x00100000, 0x000FFFFE].
  test_T20.Accept(kCleanSlateB_T3, {0x10300000},
                  {pcThumb2(0x10200000), pcThumb2(0x103FFFFE)});
  test_T20.Reject(kCleanSlateB_T3, {0x10300000},
                  {pcThumb2(0x10200000 - 2), pcThumb2(0x103FFFFE + 2)});

  // T24 tests.
  ArmTranslatorWriteReadTest<AArch32Rel32Translator::AddrTraits_T24> test_T24;
  for (uint32_t clean_slate_code : {kCleanSlateB_T4, kCleanSlateBL_T1}) {
    test_T24.Accept(clean_slate_code, aligned2, aligned2);
    test_T24.Reject(clean_slate_code, aligned2, misaligned2);
    test_T24.Reject(clean_slate_code, misaligned2, aligned2);
    test_T24.Reject(clean_slate_code, misaligned2, misaligned2);
    // Signed (24 + 1)-bit range, 2-byte aligned: [-0x01000000, 0x00FFFFFE].
    test_T24.Accept(clean_slate_code, {0x16000000},
                    {pcThumb2(0x15000000), pcThumb2(0x16FFFFFE)});
    test_T24.Reject(clean_slate_code, {0x16000000},
                    {pcThumb2(0x15000000 - 2), pcThumb2(0x16FFFFFE + 2)});
  }

  // BLX complication: THUMB2 -> ARM.
  test_T24.Accept(kCleanSlateBLX_T2, aligned2, aligned4);
  test_T24.Reject(kCleanSlateBLX_T2, aligned2, misaligned4);
  test_T24.Reject(kCleanSlateBLX_T2, misaligned2, aligned4);
  test_T24.Reject(kCleanSlateBLX_T2, misaligned2, misaligned4);
  test_T24.Accept(kCleanSlateBLX_T2, {0x16000000},
                  {pcThumb2(0x15000000), pcThumb2(0x16FFFFFC)});
  test_T24.Reject(kCleanSlateBLX_T2, {0x16000000},
                  {pcThumb2(0x15000000 - 4), pcThumb2(0x15000000 - 2),
                   pcThumb2(0x16FFFFFC + 2), pcThumb2(0x16FFFFFC + 4)});
}

// Typical usage in |target_rva| extraction.
TEST(AArch32Rel32Translator, Main) {
  // ARM mode (32-bit).
  // 00103050: 00 01 02 EA    B     00183458 ; B encoding A1 (cond = AL).
  {
    rva_t instr_rva = 0x00103050U;
    AArch32Rel32Translator translator;
    std::vector<uint8_t> bytes = {0x00, 0x01, 0x02, 0xEA};
    MutableBufferView region(&bytes[0], bytes.size());
    uint32_t code = translator.FetchArmCode32(region, 0U);
    EXPECT_EQ(0xEA020100U, code);

    // |code| <-> |disp|.
    arm_disp_t disp = 0;
    EXPECT_EQ(kArmAlign4, translator.DecodeA24(code, &disp));
    EXPECT_EQ(+0x00080400, disp);

    uint32_t code_from_disp = kCleanSlateBAL_A1;
    EXPECT_TRUE(translator.EncodeA24(disp, &code_from_disp));
    EXPECT_EQ(code, code_from_disp);

    // |code| <-> |target_rva|.
    rva_t target_rva = kInvalidRva;
    EXPECT_TRUE(translator.ReadA24(instr_rva, code, &target_rva));
    // 0x00103050 + 8 + 0x00080400.
    EXPECT_EQ(0x00183458U, target_rva);

    uint32_t code_from_rva = kCleanSlateBAL_A1;
    EXPECT_TRUE(translator.WriteA24(instr_rva, target_rva, &code_from_rva));
    EXPECT_EQ(code, code_from_rva);
  }

  // THUMB2 mode (16-bit).
  // 001030A2: F3 E7          B     0010308C ; B encoding T2.
  {
    rva_t instr_rva = 0x001030A2U;
    AArch32Rel32Translator translator;
    std::vector<uint8_t> bytes = {0xF3, 0xE7};
    MutableBufferView region(&bytes[0], bytes.size());
    uint16_t code = translator.FetchThumb2Code16(region, 0U);
    // Sii iiiiiiii = 111 11110011 = -1101 = -0x0D.
    EXPECT_EQ(0xE7F3U, code);

    // |code| <-> |disp|.
    arm_disp_t disp = 0;
    EXPECT_EQ(kArmAlign2, translator.DecodeT11(code, &disp));
    EXPECT_EQ(-0x0000001A, disp);  // -0x0D * 2 = -0x1A.

    uint16_t code_from_disp = kCleanSlateB_T2;
    EXPECT_TRUE(translator.EncodeT11(disp, &code_from_disp));
    EXPECT_EQ(code, code_from_disp);

    // |code| <-> |target_rva|.
    rva_t target_rva = kInvalidRva;
    EXPECT_TRUE(translator.ReadT11(instr_rva, code, &target_rva));
    // 0x001030A2 + 4 - 0x0000001A.
    EXPECT_EQ(0x0010308CU, target_rva);

    uint16_t code_from_rva = kCleanSlateB_T2;
    EXPECT_TRUE(translator.WriteT11(instr_rva, target_rva, &code_from_rva));
    EXPECT_EQ(code, code_from_rva);
  }

  // THUMB2 mode (32-bit).
  // 001030A2: 00 F0 01 FA    BL    001034A8 ; BL encoding T1.
  {
    rva_t instr_rva = 0x001030A2U;
    AArch32Rel32Translator translator;
    std::vector<uint8_t> bytes = {0x00, 0xF0, 0x01, 0xFA};
    MutableBufferView region(&bytes[0], bytes.size());
    uint32_t code = translator.FetchThumb2Code32(region, 0U);
    EXPECT_EQ(0xF000FA01U, code);

    // |code| <-> |disp|.
    arm_disp_t disp = 0;
    EXPECT_EQ(kArmAlign2, translator.DecodeT24(code, &disp));
    EXPECT_EQ(+0x00000402, disp);

    uint32_t code_from_disp = kCleanSlateBL_T1;
    EXPECT_TRUE(translator.EncodeT24(disp, &code_from_disp));
    EXPECT_EQ(code, code_from_disp);

    // |code| <-> |target_rva|.
    rva_t target_rva = kInvalidRva;
    EXPECT_TRUE(translator.ReadT24(instr_rva, code, &target_rva));
    // 0x001030A2 + 4 + 0x00000002.
    EXPECT_EQ(0x001034A8U, target_rva);

    uint32_t code_from_rva = kCleanSlateBL_T1;
    EXPECT_TRUE(translator.WriteT24(instr_rva, target_rva, &code_from_rva));
    EXPECT_EQ(code, code_from_rva);
  }
}

TEST(AArch32Rel32Translator, BLXComplication) {
  auto run_test = [](rva_t instr_rva,
                     std::vector<uint8_t> bytes,  // Pass by value.
                     uint32_t expected_code, arm_disp_t expected_disp,
                     uint32_t clean_slate_code, rva_t expected_target_rva) {
    AArch32Rel32Translator translator;
    MutableBufferView region(&bytes[0], bytes.size());
    uint32_t code = translator.FetchThumb2Code32(region, 0U);
    EXPECT_EQ(expected_code, code);

    // |code| <-> |disp|.
    arm_disp_t disp = 0;
    EXPECT_TRUE(translator.DecodeT24(code, &disp));
    EXPECT_EQ(expected_disp, disp);

    uint32_t code_from_disp = clean_slate_code;
    EXPECT_TRUE(translator.EncodeT24(disp, &code_from_disp));
    EXPECT_EQ(code, code_from_disp);

    // |code| <-> |target_rva|.
    rva_t target_rva = kInvalidRva;
    EXPECT_TRUE(translator.ReadT24(instr_rva, code, &target_rva));
    EXPECT_EQ(expected_target_rva, target_rva);

    uint32_t code_from_rva = clean_slate_code;
    EXPECT_TRUE(translator.WriteT24(instr_rva, target_rva, &code_from_rva));
    EXPECT_EQ(code, code_from_rva);
  };

  // No complication, 4-byte aligned.
  // 001030A0: 01 F0 06 B0    B     005040B0 ; B encoding T4.
  run_test(0x001030A0U,  // Multiple of 4.
           {0x01, 0xF0, 0x06, 0xB0}, 0xF001B006U, 0x0040100C, kCleanSlateB_T4,
           // "Canonical" |target_rva|: 0x001030A0 + 4 + 0x0040100C.
           0x005040B0U);

  // No complication, not 4-byte aligned.
  // 001030A2: 01 F0 06 B0    B     005040B2 ; B encoding T4.
  run_test(0x001030A2U,  // Shift by 2: Not multiple of 4.
           {0x01, 0xF0, 0x06, 0xB0}, 0xF001B006U, 0x0040100C, kCleanSlateB_T4,
           // Shifted by 2: 0x001030A2 + 4 + 0x0040100C.
           0x005040B2U);

  // Repeat the above, but use BLX instead of B.

  // BLX complication, 4-byte aligned.
  // 001030A0: 01 F0 06 E0    BLX   005040B0 ; BLX encoding T2.
  run_test(0x001030A0U,  // Multiple of 4.
           {0x01, 0xF0, 0x06, 0xE0}, 0xF001E006U, 0x0040100C, kCleanSlateBLX_T2,
           // Canonical again: align_down_4(0x001030A0 + 4 + 0x0040100C).
           0x005040B0U);

  // BLX complication, not 4-byte aligned.
  // 001030A2: 01 F0 06 E0    BLX   005040B0 ; BLX encoding T2.
  run_test(0x001030A2U,  // Shift by 2: Not multiple of 4.
           {0x01, 0xF0, 0x06, 0xE0}, 0xF001E006U, 0x0040100C, kCleanSlateBLX_T2,
           // No shift: align_down_4(0x001030A2 + 4 + 0x0040100C).
           0x005040B0U);
}

TEST(AArch64Rel32Translator, FetchStore) {
  std::vector<uint8_t> bytes = {0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE};
  std::vector<uint8_t> expected = {0xAB, 0x33, 0x22, 0x11,
                                   0x69, 0x5A, 0xFF, 0x00};
  MutableBufferView region(&bytes[0], bytes.size());
  AArch64Rel32Translator translator;
  EXPECT_EQ(0x76543210U, translator.FetchCode32(region, 0U));
  EXPECT_EQ(0xFEDCBA98U, translator.FetchCode32(region, 4U));

  translator.StoreCode32(region, 0U, 0x112233ABU);
  translator.StoreCode32(region, 4U, 0x00FF5A69);
  EXPECT_EQ(expected, bytes);
}

TEST(AArch64Rel32Translator, EncodeDecode) {
  // Immd14 tests.
  ArmTranslatorEncodeDecodeTest<AArch64Rel32Translator::AddrTraits_Immd14>
      test_immd14;
  for (int b40 : {0, 1, 7, 31}) {
    uint32_t b40_mask = b40 << 19;
    for (int Rt : {0, 1, 15, 30}) {
      uint32_t mask = b40_mask | Rt;
      ArmRelInstruction<uint32_t> TBZw_Rt("00110110 bbbbbSii iiiiiiii iiittttt",
                                          kCleanSlate64TBZw | mask);
      ArmRelInstruction<uint32_t> TBZz_Rt("10110110 bbbbbSii iiiiiiii iiittttt",
                                          kCleanSlate64TBZz | mask);
      ArmRelInstruction<uint32_t> TBNZw_Rt(
          "00110111 bbbbbSii iiiiiiii iiittttt", kCleanSlate64TBNZw | mask);
      ArmRelInstruction<uint32_t> TBNZz_Rt(
          "10110111 bbbbbSii iiiiiiii iiittttt", kCleanSlate64TBNZz | mask);
      test_immd14.Run("SSSSSSSS SSSSSSSS Siiiiiii iiiiii00", {"S", "i"},
                      {TBZw_Rt, TBZz_Rt, TBNZw_Rt, TBNZz_Rt},
                      {0x00007FFC, -0x00008000, 0, 4, -4, 0x40, 0x44},
                      {2, -2, 0x41, 0x42, 0x43, 0x00008000, -0x00008004});
    }
  }

  // Immd19 tests.
  ArmTranslatorEncodeDecodeTest<AArch64Rel32Translator::AddrTraits_Immd19>
      test_immd19;
  for (int cond = 0; cond <= 0x0E; ++cond) {
    ArmRelInstruction<uint32_t> B_cond("01010100 Siiiiiii iiiiiiii iii0cccc",
                                       kCleanSlate64Bcond | cond);
    test_immd19.Run("SSSSSSSS SSSSiiii iiiiiiii iiiiii00", {"S", "i"}, {B_cond},
                    {0x000FFFFC, -0x00100000, 0, 4, -4, 0x40, 0x44},
                    {2, -2, 0x41, 0x42, 0x43, 0x00100000, -0x00100004});
  }
  for (int Rt : {0, 1, 15, 30}) {
    ArmRelInstruction<uint32_t> CBZw_Rt("00110100 Siiiiiii iiiiiiii iiittttt",
                                        kCleanSlate64CBZw | Rt);
    ArmRelInstruction<uint32_t> CBZz_Rt("10110100 Siiiiiii iiiiiiii iiittttt",
                                        kCleanSlate64CBZz | Rt);
    ArmRelInstruction<uint32_t> CBNZw_Rt("00110101 Siiiiiii iiiiiiii iiittttt",
                                         kCleanSlate64CBNZw | Rt);
    ArmRelInstruction<uint32_t> CBNZz_Rt("10110101 Siiiiiii iiiiiiii iiittttt",
                                         kCleanSlate64CBNZz | Rt);
    test_immd19.Run("SSSSSSSS SSSSiiii iiiiiiii iiiiii00", {"S", "i"},
                    {CBZw_Rt, CBZz_Rt, CBNZw_Rt, CBNZz_Rt},
                    {0x000FFFFC, -0x00100000, 0, 4, -4, 0x40, 0x44},
                    {2, -2, 0x41, 0x42, 0x43, 0x00100000, -0x00100004});
  }

  // Immd26 tests.
  ArmTranslatorEncodeDecodeTest<AArch64Rel32Translator::AddrTraits_Immd26>
      test_immd26;
  ArmRelInstruction<uint32_t> B("000101Si iiiiiiii iiiiiiii iiiiiiii",
                                kCleanSlate64B);
  ArmRelInstruction<uint32_t> BL("100101Si iiiiiiii iiiiiiii iiiiiiii",
                                 kCleanSlate64BL);
  test_immd26.Run("SSSSSiii iiiiiiii iiiiiiii iiiiii00", {"S", "i"}, {B, BL},
                  {0x07FFFFFC, -0x08000000, 0, 4, -4, 0x40, 0x44},
                  {2, -2, 0x41, 0x42, 0x43, 0x08000000, -0x08000004});
}

TEST(AArch64Rel32Translator, WriteRead) {
  std::vector<rva_t> aligned4;
  std::vector<rva_t> misaligned4;
  for (rva_t rva = 0x1FFC; rva <= 0x2010; ++rva) {
    ((rva % 4 == 0) ? aligned4 : misaligned4).push_back(rva);
  }
  CHECK_EQ(6U, aligned4.size());
  CHECK_EQ(15U, misaligned4.size());

  // Helper to convert an instruction's RVA to PC.
  auto pcAArch64 = [](rva_t instr_rva) -> rva_t { return instr_rva; };

  // Immd14 tests.
  ArmTranslatorWriteReadTest<AArch64Rel32Translator::AddrTraits_Immd14>
      test_immd14;
  for (uint32_t clean_slate_code : {kCleanSlate64TBZw, kCleanSlate64TBZz,
                                    kCleanSlate64TBNZw, kCleanSlate64TBNZz}) {
    test_immd14.Accept(clean_slate_code, aligned4, aligned4);
    test_immd14.Reject(clean_slate_code, aligned4, misaligned4);
    test_immd14.Reject(clean_slate_code, misaligned4, aligned4);
    test_immd14.Reject(clean_slate_code, misaligned4, misaligned4);
    // Signed (14 + 2)-bit range, 4-byte aligned: [-0x00008000, 0x00007FFC].
    test_immd14.Accept(clean_slate_code, {0x10040000},
                       {pcAArch64(0x10038000), pcAArch64(0x10047FFC)});
    test_immd14.Reject(clean_slate_code, {0x15000000},
                       {pcAArch64(0x10038000 - 4), pcAArch64(0x10047FFC + 4)});
  }

  // Immd19 tests.
  ArmTranslatorWriteReadTest<AArch64Rel32Translator::AddrTraits_Immd19>
      test_immd19;
  for (uint32_t clean_slate_code :
       {kCleanSlate64Bcond, kCleanSlate64CBZw, kCleanSlate64CBZz,
        kCleanSlate64CBNZw, kCleanSlate64CBNZz}) {
    test_immd19.Accept(clean_slate_code, aligned4, aligned4);
    test_immd19.Reject(clean_slate_code, aligned4, misaligned4);
    test_immd19.Reject(clean_slate_code, misaligned4, aligned4);
    test_immd19.Reject(clean_slate_code, misaligned4, misaligned4);
    // Signed (19 + 2)-bit range, 4-byte aligned: [-0x00100000, 0x000FFFFC].
    test_immd19.Accept(clean_slate_code, {0x10300000},
                       {pcAArch64(0x10200000), pcAArch64(0x103FFFFC)});
    test_immd19.Reject(clean_slate_code, {0x10300000},
                       {pcAArch64(0x10200000 - 4), pcAArch64(0x103FFFFC + 4)});
  }

  // Immd26 tests.
  ArmTranslatorWriteReadTest<AArch64Rel32Translator::AddrTraits_Immd26>
      test_immd26;
  for (uint32_t clean_slate_code : {kCleanSlate64B, kCleanSlate64BL}) {
    test_immd26.Accept(clean_slate_code, aligned4, aligned4);
    test_immd26.Reject(clean_slate_code, aligned4, misaligned4);
    test_immd26.Reject(clean_slate_code, misaligned4, aligned4);
    test_immd26.Reject(clean_slate_code, misaligned4, misaligned4);
    // Signed (26 + 2)-bit range, 4-byte aligned: [-0x08000000, 0x07FFFFFC].
    test_immd26.Accept(clean_slate_code, {0x30000000},
                       {pcAArch64(0x28000000), pcAArch64(0x37FFFFFC)});
    test_immd26.Reject(clean_slate_code, {0x30000000},
                       {pcAArch64(0x28000000 - 4), pcAArch64(0x37FFFFFC + 4)});
  }
}

// Typical usage in |target_rva| extraction.
TEST(AArch64Rel32Translator, Main) {
  // 00103050: 02 01 02 14    B     00183458
  rva_t instr_rva = 0x00103050U;
  AArch64Rel32Translator translator;
  std::vector<uint8_t> bytes = {0x02, 0x01, 0x02, 0x14};
  MutableBufferView region(&bytes[0], bytes.size());
  uint32_t code = translator.FetchCode32(region, 0U);
  EXPECT_EQ(0x14020102U, code);

  // |code| <-> |disp|.
  arm_disp_t disp = 0;
  EXPECT_TRUE(translator.DecodeImmd26(code, &disp));
  EXPECT_EQ(+0x00080408, disp);

  uint32_t code_from_disp = kCleanSlate64B;
  EXPECT_TRUE(translator.EncodeImmd26(disp, &code_from_disp));
  EXPECT_EQ(code, code_from_disp);

  // |code| <-> |target_rva|.
  rva_t target_rva = kInvalidRva;
  EXPECT_TRUE(translator.ReadImmd26(instr_rva, code, &target_rva));
  // 0x00103050 + 0 + 0x00080408.
  EXPECT_EQ(0x00183458U, target_rva);

  uint32_t code_from_rva = kCleanSlate64B;
  EXPECT_TRUE(translator.WriteImmd26(instr_rva, target_rva, &code_from_rva));
  EXPECT_EQ(code, code_from_rva);
}

}  // namespace zucchini
