// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_DISASSEMBLER_ZTF_H_
#define COMPONENTS_ZUCCHINI_DISASSEMBLER_ZTF_H_

#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/zucchini/disassembler.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/type_ztf.h"

namespace zucchini {

// Disassembler for text based files. This file format is supported for
// debugging Zucchini and is not intended for production usage.
//
// A valid Zucchini Text Format (ZTF) file is specified as follows:
//
// Header:
//   The first four bytes must be - 'Z' 'T' 'x' 't'
// Footer:
//   The last five bytes must be  - 't' 'x' 'T' 'Z' '\n'
//   (note that terminating new line is required).
// Content:
//   The content can be any sequence of printable ASCII characters and new line
//   (but not carriage return). This excludes the sequence that comprises the
//   Footer.
// References:
//   A reference is either Absolute or Relative. All references must begin and
//   end with a pair of enclosing characters <open>, <close>. The options are:
//     - Angles:      '<' and '>'
//     - Braces:      '{' and '}'
//     - Brackets:    '[' and ']'
//     - Parentheses: '(' and ')'
//
//   A reference contains three items:
//     - A line number       <line>
//     - A delimiter     ',' <delimiter>
//     - A column number     <col>
//     <line> and <col> may contain 1-3 digits and both must contain the same
//     number of digits. If a number is too short then it can be left-padded
//     with '0'.
//
//   For Absolute references, <line> and <col> are 1-based (i.e. positive)
//   index of line and column numbers of a character in the ZTF. This follows
//   standard convention for text editors. Note that "\n" is considered to be
//   part of a preceding line.
//
//     <open><line><delimiter><col><close>
//
//   For Relative references, <line> and <col> are integer offsets deltas of the
//   target's (absolute) line and column relative to the line and column of the
//   reference's first byte (i.e. <open>). Relative references have <sign> ('+'
//   or '-') before <line> and <col>. For the special case of "0", "00", etc.,
//   <sign> must be "+".
//
//     <open><sign><line><delimiter><sign><col><close>
//
//   If a reference points outside the target either in writing or reading it is
//   considered invalid and ignored. Similarly if it overflows a line. i.e. if a
//   line is 10 characters long and a references targets character 11 of that
//   line it is rejected. Lines are delimited with '\n' which is counted toward
//   the line length.
//
//   If a reference is to be written that would overwrite a '\n' character it is
//   ignored as this would break all other line values.

enum : size_t { kMaxDigitCount = 3 };

// Helper class for translating among offset_t, ztf::LineCol and
// ztf::DeltaLineCol.
class ZtfTranslator {
 public:
  ZtfTranslator();
  ZtfTranslator(const ZtfTranslator&) = delete;
  const ZtfTranslator& operator=(const ZtfTranslator&) = delete;
  ~ZtfTranslator();

  // Initializes |line_starts_| with the contents of |image|.
  bool Init(ConstBufferView image);

  // Checks if |lc| is a valid location in the file.
  bool IsValid(ztf::LineCol lc) const;

  // Checks if |dlc| relative to |offset| is a valid location in the file.
  bool IsValid(offset_t offset, ztf::DeltaLineCol dlc) const;

  // Returns the offset corresponding to |line_col| if it is valid. Otherwise
  // returns |kInvalidOffset|.
  offset_t LineColToOffset(ztf::LineCol line_col) const;

  // Returns the ztf::LineCol for an |offset| if it is valid. Otherwise returns
  // std::nullopt.
  std::optional<ztf::LineCol> OffsetToLineCol(offset_t offset) const;

 private:
  // Returns an iterator to the range containing |offset|. Which is represented
  // by the starting offset. The next element will contain the upper bound of
  // the range.
  std::vector<offset_t>::const_iterator SearchForRange(offset_t offset) const;

  // Returns the length of a 1-indexed line. The caller is expected to check
  // that the requested line exists.
  offset_t LineLength(uint16_t line) const;

  offset_t NumLines() const {
    return static_cast<offset_t>(line_starts_.size() - 1);
  }

  // |line_starts_| is a sorted list of each line's starting offset, along with
  // the image size as the sentinel; it looks like {0, ..., image.size}.
  std::vector<offset_t> line_starts_;
};

// Disassembler for Zucchini Text Format (ZTF).
class DisassemblerZtf : public Disassembler {
 public:
  static constexpr uint16_t kVersion = 1;

  // Target Pools
  enum ReferencePool : uint8_t {
    kAngles,      // <>
    kBraces,      // {}
    kBrackets,    // []
    kParentheses  // ()
  };

  // Type breakdown. Should contain all permutations of ReferencePool, Abs|Rel
  // and the possible number of digits (1-3).
  enum ReferenceType : uint8_t {
    kAnglesAbs1,
    kAnglesAbs2,
    kAnglesAbs3,
    kAnglesRel1,
    kAnglesRel2,
    kAnglesRel3,
    kBracesAbs1,
    kBracesAbs2,
    kBracesAbs3,
    kBracesRel1,
    kBracesRel2,
    kBracesRel3,
    kBracketsAbs1,
    kBracketsAbs2,
    kBracketsAbs3,
    kBracketsRel1,
    kBracketsRel2,
    kBracketsRel3,
    kParenthesesAbs1,
    kParenthesesAbs2,
    kParenthesesAbs3,
    kParenthesesRel1,
    kParenthesesRel2,
    kParenthesesRel3,
    kNumTypes
  };

  DisassemblerZtf();
  DisassemblerZtf(const DisassemblerZtf&) = delete;
  const DisassemblerZtf& operator=(const DisassemblerZtf&) = delete;
  ~DisassemblerZtf() override;

  // Applies quick checks to determine if |image| *may* point to the start of a
  // ZTF file. Returns true on success.
  static bool QuickDetect(ConstBufferView image);

  // Disassembler:
  ExecutableType GetExeType() const override;
  std::string GetExeTypeString() const override;
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

  // Reference Readers, templated to allow configurable digit count and pool.
  template <uint8_t digits, ReferencePool pool>
  std::unique_ptr<ReferenceReader> MakeReadAbs(offset_t lo, offset_t hi);
  template <uint8_t digits, ReferencePool pool>
  std::unique_ptr<ReferenceReader> MakeReadRel(offset_t lo, offset_t hi);

  // Reference Writers, templated to allow configurable digit count and pool.
  template <uint8_t digits, ReferencePool pool>
  std::unique_ptr<ReferenceWriter> MakeWriteAbs(MutableBufferView image);
  template <uint8_t digits, ReferencePool pool>
  std::unique_ptr<ReferenceWriter> MakeWriteRel(MutableBufferView image);

 private:
  friend Disassembler;

  // Disassembler:
  bool Parse(ConstBufferView image) override;

  ZtfTranslator translator_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_DISASSEMBLER_ZTF_H_
