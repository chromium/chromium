// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/disassembler_ztf.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numeric>

#include "base/check_op.h"
#include "base/memory/raw_ref.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "components/zucchini/algorithm.h"
#include "components/zucchini/buffer_source.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/io_utils.h"

namespace zucchini {

namespace {

constexpr uint8_t kDelimiter = ',';

constexpr int kHeaderMagicSize = 4;
constexpr int kFooterMagicSize = 5;
constexpr int kTotalMagicSize = kHeaderMagicSize + kFooterMagicSize;

// Number of characters that aren't digits in each type of reference.
constexpr int kNumConstCharInAbs = 3;
constexpr int kNumConstCharInRel = 5;

/******** ZtfConfig ********/

// For passing around metadata about the type of reference to match.
// - |digits_per_dim| is the length of the offset in lines/cols of a
//   reference.
// - |open_char| is an ASCII character representing the opening char.
// - |close_char| is an ASCII character representing the closing char.
struct ZtfConfig {
  uint8_t digits_per_dim;
  uint8_t open_char;
  uint8_t close_char;

  constexpr uint8_t abs_width() const {
    return digits_per_dim * 2 + kNumConstCharInAbs;
  }

  constexpr uint8_t rel_width() const {
    return digits_per_dim * 2 + kNumConstCharInRel;
  }

  uint8_t Width(ztf::LineCol /* lc */) const { return abs_width(); }

  uint8_t Width(ztf::DeltaLineCol /* dlc */) const { return rel_width(); }
};

// Creates a ZtfConfig for parsing or writing based on the desired |digits| and
// |pool|.
template <DisassemblerZtf::ReferencePool pool>
constexpr ZtfConfig MakeZtfConfig(uint8_t digits) {
  switch (pool) {
    case DisassemblerZtf::kAngles:
      return ZtfConfig{digits, '<', '>'};
    case DisassemblerZtf::kBraces:
      return ZtfConfig{digits, '{', '}'};
    case DisassemblerZtf::kBrackets:
      return ZtfConfig{digits, '[', ']'};
    case DisassemblerZtf::kParentheses:
      break;  // Handled below.
  }
  return ZtfConfig{digits, '(', ')'};
}

/******** ZtfParser ********/

// ZtfParser is used to extract (absolute) LineCol and (relative) DeltaLineCol
// from a ZTF file, and contains various helpers for character, digits, and sign
// matching.
class ZtfParser {
 public:
  ZtfParser(offset_t hi, ConstBufferView image, ZtfConfig config)
      : image_(image), hi_(hi), config_(config) {
    DCHECK_LE(static_cast<size_t>(std::pow(10U, config_.digits_per_dim)),
              ztf::kMaxDimValue);
  }

  ZtfParser(const ZtfParser&) = delete;
  const ZtfParser& operator=(const ZtfParser&) = delete;

  // Attempts to match an absolute reference at |offset|. If successful then
  // assigns the result to |abs_lc| and returns true. Otherwise returns false.
  // An absolute reference takes the form:
  // <open><digits><delimiter><digits><close>
  bool MatchAtOffset(offset_t offset, ztf::LineCol* abs_lc) {
    if (hi_ < config_.abs_width() || offset > hi_ - config_.abs_width())
      return false;
    offset_ = offset;
    return MatchChar(config_.open_char) && MatchDigits(+1, &abs_lc->line) &&
           MatchChar(kDelimiter) && MatchDigits(+1, &abs_lc->col) &&
           MatchChar(config_.close_char);
  }

  // Attempts to match an absolute reference at |offset|. If successful then
  // assigns the result to |rel_lc| and returns true. Otherwise returns false. A
  // relative reference takes the form:
  // <open><sign><digits><delimiter><sign><digits><close>
  bool MatchAtOffset(offset_t offset, ztf::DeltaLineCol* rel_dlc) {
    if (hi_ < config_.rel_width() || offset > hi_ - config_.rel_width())
      return false;
    offset_ = offset;
    ztf::dim_t line_sign;
    ztf::dim_t col_sign;
    return MatchChar(config_.open_char) && MatchSign(&line_sign) &&
           MatchDigits(line_sign, &rel_dlc->line) && MatchChar(kDelimiter) &&
           MatchSign(&col_sign) && MatchDigits(col_sign, &rel_dlc->col) &&
           MatchChar(config_.close_char);
  }

 private:
  // The Match*() functions below can advance |offset_|, and return a bool to
  // indicate success to allow chaining using &&.

  // Returns true if |character| is at location |offset_| in |image_| and
  // increments |offset_|.
  bool MatchChar(uint8_t character) {
    return character == image_.read<uint8_t>(offset_++);
  }

  // Looks for '+' or '-' at |offset_|. If found, stores +1 or -1 in |sign| and
  // returns true. Otherwise returns false.
  bool MatchSign(ztf::dim_t* sign) {
    uint8_t val = image_.read<uint8_t>(offset_++);
    if (val == static_cast<uint8_t>(ztf::SignChar::kMinus)) {
      *sign = -1;
      return true;
    }
    if (val == static_cast<uint8_t>(ztf::SignChar::kPlus)) {
      *sign = 1;
      return true;
    }
    return false;
  }

  // Attempts to extract a number with the number of base 10 digits equal to
  // |config_.digits_per_dim| from |image_| starting from |offset_|. Returns
  // true and assigns the integer value to |value| if successful.
  bool MatchDigits(ztf::dim_t sign, ztf::dim_t* value) {
    ztf::dim_t output = 0;
    for (int i = 0; i < config_.digits_per_dim; ++i) {
      auto digit = image_.read<uint8_t>(offset_++);
      if (digit >= '0' && digit < '0' + 10)
        output = output * 10 + digit - '0';
      else
        return false;
    }
    if (!output && sign < 0)  // Disallow "-0", "-00", etc.
      return false;
    *value = sign * output;
    return true;
  }

  ConstBufferView image_;
  const offset_t hi_;
  const ZtfConfig config_;
  offset_t offset_ = 0;
};

/******** ZtfWriter ********/

// ZtfWriter is used to write references to an image. This includes writing
// the enclosing characters around the reference.
class ZtfWriter {
 public:
  ZtfWriter(MutableBufferView image, ZtfConfig config)
      : image_(image),
        config_(config),
        val_bound_(
            static_cast<ztf::dim_t>(std::pow(10, config_.digits_per_dim))) {}

  ZtfWriter(const ZtfWriter&) = delete;
  const ZtfWriter& operator=(const ZtfWriter&) = delete;

  // Write an absolute reference |abs_ref| at |offset|. Note that references
  // that would overwrite a newline are skipped as this would invalidate all
  // the other reference line numbers.
  void Write(offset_t offset, ztf::LineCol abs_ref) {
    offset_ = offset;
    if (!SafeToWriteNumber(abs_ref.line) || !SafeToWriteNumber(abs_ref.col) ||
        !SafeToWriteData(offset_, offset_ + config_.abs_width())) {
      return;
    }
    WriteChar(config_.open_char);
    WriteNumber(abs_ref.line);
    WriteChar(kDelimiter);
    WriteNumber(abs_ref.col);
    WriteChar(config_.close_char);
  }

  // Write a relative reference |rel_ref| at |offset|. Note that references
  // that would overwrite a newline are skipped as this would invalidate all
  // the other reference line numbers.
  void Write(offset_t offset, ztf::DeltaLineCol rel_ref) {
    offset_ = offset;
    if (!SafeToWriteNumber(rel_ref.line) || !SafeToWriteNumber(rel_ref.col) ||
        !SafeToWriteData(offset_, offset_ + config_.rel_width())) {
      return;
    }
    WriteChar(config_.open_char);
    WriteSign(rel_ref.line);
    WriteNumber(rel_ref.line);
    WriteChar(kDelimiter);
    WriteSign(rel_ref.col);
    WriteNumber(rel_ref.col);
    WriteChar(config_.close_char);
  }

 private:
  // Returns whether it is safe to modify bytes in |[lo, hi)| in |image_| for
  // Reference correction. Failure cases are:
  // - Out-of-bound writes.
  // - Overwriting '\n'. This is a ZTF special case since '\n' dictates file
  //   structure, and Reference correction should never mess with this.
  bool SafeToWriteData(offset_t lo, offset_t hi) const {
    DCHECK_LE(lo, hi);
    // Out of bounds.
    if (hi > image_.size())
      return false;
    for (offset_t i = lo; i < hi; ++i) {
      if (image_.read<uint8_t>(i) == '\n')
        return false;
    }
    return true;
  }

  // Checks whether it is safe to write a |val| based on
  // |config_.digits_per_dim|.
  bool SafeToWriteNumber(ztf::dim_t val) const {
    return std::abs(val) < val_bound_;
  }

  // The Write*() functions each advance |offset_| by a fixed distance. The
  // caller should ensure there's enough space to write data.

  // Write |character| at |offset_| and increment |offset_|.
  void WriteChar(uint8_t character) { image_.write(offset_++, character); }

  // Write the sign of |value| at |offset_| and increment |offset_|.
  void WriteSign(ztf::dim_t value) {
    image_.write(offset_++,
                 value >= 0 ? ztf::SignChar::kPlus : ztf::SignChar::kMinus);
  }

  // Writes the absolute value of the number represented by |value| at |offset_|
  // using zero padding to fill |config_.digits_per_dim|.
  void WriteNumber(ztf::dim_t value) {
    size_t size = config_.digits_per_dim + 1;
    DCHECK_LE(size, kMaxDigitCount + 1);
    char digits[kMaxDigitCount + 1];  // + 1 for terminator.
    int len =
        snprintf(digits, size, "%0*u", config_.digits_per_dim, std::abs(value));
    DCHECK_EQ(len, config_.digits_per_dim);
    for (int i = 0; i < len; ++i)
      image_.write(offset_++, digits[i]);
  }

  MutableBufferView image_;
  const ZtfConfig config_;
  // Bound on numeric values, as limited by |config_.digits_per_dim|.
  const ztf::dim_t val_bound_;
  offset_t offset_ = 0;
};

// Specialization of ReferenceReader for reading text references.
template <typename T>
class ZtfReferenceReader : public ReferenceReader {
 public:
  ZtfReferenceReader(offset_t lo,
                     offset_t hi,
                     ConstBufferView image,
                     const ZtfTranslator& translator,
                     ZtfConfig config)
      : offset_(lo),
        hi_(hi),
        translator_(translator),
        config_(config),
        parser_(hi_, image, config_) {
    DCHECK_LE(hi_, image.size());
  }

  // Walks |offset_| from |lo| to |hi_| running |parser_|. If any matches are
  // found they are returned.
  std::optional<Reference> GetNext() override {
    T line_col;
    for (; offset_ < hi_; ++offset_) {
      if (!parser_.MatchAtOffset(offset_, &line_col))
        continue;

      auto target = ConvertToTargetOffset(offset_, line_col);
      // Ignore targets that point outside the file.
      if (target == kInvalidOffset)
        continue;
      offset_t location = offset_;
      offset_ += config_.Width(line_col);
      return Reference{location, target};
    }
    return std::nullopt;
  }

 private:
  // Converts |lc| (an absolute reference) to an offset using |translator_|.
  offset_t ConvertToTargetOffset(offset_t /* location */,
                                 ztf::LineCol lc) const {
    return translator_->LineColToOffset(lc);
  }

  // Converts |dlc| (a relative reference) to an offset using |translator_|.
  // This requires converting the |dlc| to a ztf::LineCol to find the offset.
  offset_t ConvertToTargetOffset(offset_t location,
                                 ztf::DeltaLineCol dlc) const {
    auto lc = translator_->OffsetToLineCol(location);
    if (!lc.has_value())
      return kInvalidOffset;
    return translator_->LineColToOffset(lc.value() + dlc);
  }

  offset_t offset_;
  const offset_t hi_;
  const raw_ref<const ZtfTranslator> translator_;
  const ZtfConfig config_;
  ZtfParser parser_;
};

// Specialization of ReferenceWriter for writing text references.
template <typename T>
class ZtfReferenceWriter : public ReferenceWriter {
 public:
  ZtfReferenceWriter(MutableBufferView image,
                     const ZtfTranslator& translator,
                     ZtfConfig config)
      : translator_(translator), writer_(image, config) {}

  void PutNext(Reference reference) override {
    T line_col;
    if (!ConvertToTargetLineCol(reference, &line_col))
      return;

    writer_.Write(reference.location, line_col);
  }

 private:
  // Converts |reference| to an absolute reference to be stored in |out_lc|.
  // Returns true on success.
  bool ConvertToTargetLineCol(Reference reference, ztf::LineCol* out_lc) {
    auto temp_lc = translator_->OffsetToLineCol(reference.target);
    if (!temp_lc.has_value() || !translator_->IsValid(temp_lc.value()))
      return false;

    *out_lc = temp_lc.value();
    return true;
  }

  // Converts |reference| to a relative reference to be stored in |out_dlc|.
  // Will return true on success.
  bool ConvertToTargetLineCol(Reference reference, ztf::DeltaLineCol* out_dlc) {
    auto location_lc = translator_->OffsetToLineCol(reference.location);
    if (!location_lc.has_value())
      return false;

    auto target_lc = translator_->OffsetToLineCol(reference.target);
    if (!target_lc.has_value())
      return false;

    *out_dlc = target_lc.value() - location_lc.value();
    return translator_->IsValid(reference.location, *out_dlc);
  }

  const raw_ref<const ZtfTranslator> translator_;
  ZtfWriter writer_;
};

// Reads a text header to check for the magic string "ZTxt" at the start
// indicating the file should be treated as a Zucchini text file.
bool ReadZtfHeader(ConstBufferView image) {
  BufferSource source(image);
  // Reject empty images and "ZTxtxTZ\n" (missing 't').
  if (source.size() < kTotalMagicSize)
    return false;
  if (source.size() > std::numeric_limits<offset_t>::max())
    return false;
  return source.CheckNextBytes({'Z', 'T', 'x', 't'});
}

}  // namespace

/******** ZtfTranslator ********/

ZtfTranslator::ZtfTranslator() = default;

ZtfTranslator::~ZtfTranslator() = default;

bool ZtfTranslator::Init(ConstBufferView image) {
  line_starts_.clear();
  // Record the starting offset of every line in |image_| into |line_start_|.
  line_starts_.push_back(0);
  for (size_t i = 0; i < image.size(); ++i) {
    if (image.read<uint8_t>(i) == '\n') {
      // Maximum number of entries is |ztf::kMaxDimValue|, including the end
      // sentinel.
      if (line_starts_.size() >= ztf::kMaxDimValue)
        return false;
      line_starts_.push_back(base::checked_cast<offset_t>(i + 1));
      // Check that the line length is reachable from an absolute reference.
      if (line_starts_.back() - *std::next(line_starts_.rbegin()) >=
          ztf::kMaxDimValue) {
        return false;
      }
    }
  }
  // Since the last character of ZTF file is always '\n', |line_starts_| will
  // always contain the file length as the last element, which serves as a
  // sentinel.
  CHECK_EQ(image.size(), static_cast<size_t>(line_starts_.back()));
  return true;
}

bool ZtfTranslator::IsValid(ztf::LineCol lc) const {
  DCHECK(!line_starts_.empty());
  return lc.line >= 1 && lc.col >= 1 &&
         static_cast<offset_t>(lc.line) <= NumLines() &&
         static_cast<offset_t>(lc.col) <= LineLength(lc.line);
}

bool ZtfTranslator::IsValid(offset_t offset, ztf::DeltaLineCol dlc) const {
  DCHECK(!line_starts_.empty());
  auto abs_lc = OffsetToLineCol(offset);
  if (!abs_lc.has_value())
    return false;

  if (!base::CheckAdd(abs_lc->line, dlc.line).IsValid() ||
      !base::CheckAdd(abs_lc->col, dlc.col).IsValid()) {
    return false;
  }
  return IsValid(abs_lc.value() + dlc);
}

offset_t ZtfTranslator::LineColToOffset(ztf::LineCol lc) const {
  // Guard against out of bounds access to |line_starts_| and ensure the
  // |lc| falls within the file.
  DCHECK(!line_starts_.empty());
  if (!IsValid(lc))
    return kInvalidOffset;

  offset_t target = line_starts_[lc.line - 1] + lc.col - 1;
  DCHECK_LT(target, line_starts_.back());
  return target;
}

std::optional<ztf::LineCol> ZtfTranslator::OffsetToLineCol(
    offset_t offset) const {
  DCHECK(!line_starts_.empty());
  // Don't place a target outside the image.
  if (offset >= line_starts_.back())
    return std::nullopt;
  auto it = SearchForRange(offset);
  ztf::LineCol lc;
  lc.line = std::distance(line_starts_.cbegin(), it) + 1;
  lc.col = offset - line_starts_[lc.line - 1] + 1;
  DCHECK_LE(static_cast<offset_t>(lc.col), LineLength(lc.line));
  return lc;
}

std::vector<offset_t>::const_iterator ZtfTranslator::SearchForRange(
    offset_t offset) const {
  DCHECK(!line_starts_.empty());
  auto it =
      std::upper_bound(line_starts_.cbegin(), line_starts_.cend(), offset);
  DCHECK(it != line_starts_.cbegin());
  return --it;
}

offset_t ZtfTranslator::LineLength(uint16_t line) const {
  DCHECK_GE(line, 1);
  DCHECK_LE(line, NumLines());
  return line_starts_[line] - line_starts_[line - 1];
}

/******** DisassemblerZtf ********/

// Use 2 even though reference "chaining" isn't present in ZTF as it is the
// usual case for other Disassemblers and this is meant to mimic that as closely
// as possible.
DisassemblerZtf::DisassemblerZtf() : Disassembler(2) {}

DisassemblerZtf::~DisassemblerZtf() = default;

// static.
bool DisassemblerZtf::QuickDetect(ConstBufferView image) {
  return ReadZtfHeader(image);
}

ExecutableType DisassemblerZtf::GetExeType() const {
  return kExeTypeZtf;
}

std::string DisassemblerZtf::GetExeTypeString() const {
  return "Zucchini Text Format";
}

std::vector<ReferenceGroup> DisassemblerZtf::MakeReferenceGroups() const {
  return {
      {{5, TypeTag(kAnglesAbs1), PoolTag(kAngles)},
       &DisassemblerZtf::MakeReadAbs<1, kAngles>,
       &DisassemblerZtf::MakeWriteAbs<1, kAngles>},
      {{7, TypeTag(kAnglesAbs2), PoolTag(kAngles)},
       &DisassemblerZtf::MakeReadAbs<2, kAngles>,
       &DisassemblerZtf::MakeWriteAbs<2, kAngles>},
      {{9, TypeTag(kAnglesAbs3), PoolTag(kAngles)},
       &DisassemblerZtf::MakeReadAbs<3, kAngles>,
       &DisassemblerZtf::MakeWriteAbs<3, kAngles>},
      {{7, TypeTag(kAnglesRel1), PoolTag(kAngles)},
       &DisassemblerZtf::MakeReadRel<1, kAngles>,
       &DisassemblerZtf::MakeWriteRel<1, kAngles>},
      {{9, TypeTag(kAnglesRel2), PoolTag(kAngles)},
       &DisassemblerZtf::MakeReadRel<2, kAngles>,
       &DisassemblerZtf::MakeWriteRel<2, kAngles>},
      {{11, TypeTag(kAnglesRel3), PoolTag(kAngles)},
       &DisassemblerZtf::MakeReadRel<3, kAngles>,
       &DisassemblerZtf::MakeWriteRel<3, kAngles>},
      {{5, TypeTag(kBracesAbs1), PoolTag(kBraces)},
       &DisassemblerZtf::MakeReadAbs<1, kBraces>,
       &DisassemblerZtf::MakeWriteAbs<1, kBraces>},
      {{7, TypeTag(kBracesAbs2), PoolTag(kBraces)},
       &DisassemblerZtf::MakeReadAbs<2, kBraces>,
       &DisassemblerZtf::MakeWriteAbs<2, kBraces>},
      {{9, TypeTag(kBracesAbs3), PoolTag(kBraces)},
       &DisassemblerZtf::MakeReadAbs<3, kBraces>,
       &DisassemblerZtf::MakeWriteAbs<3, kBraces>},
      {{7, TypeTag(kBracesRel1), PoolTag(kBraces)},
       &DisassemblerZtf::MakeReadRel<1, kBraces>,
       &DisassemblerZtf::MakeWriteRel<1, kBraces>},
      {{9, TypeTag(kBracesRel2), PoolTag(kBraces)},
       &DisassemblerZtf::MakeReadRel<2, kBraces>,
       &DisassemblerZtf::MakeWriteRel<2, kBraces>},
      {{11, TypeTag(kBracesRel3), PoolTag(kBraces)},
       &DisassemblerZtf::MakeReadRel<3, kBraces>,
       &DisassemblerZtf::MakeWriteRel<3, kBraces>},
      {{5, TypeTag(kBracketsAbs1), PoolTag(kBrackets)},
       &DisassemblerZtf::MakeReadAbs<1, kBrackets>,
       &DisassemblerZtf::MakeWriteAbs<1, kBrackets>},
      {{7, TypeTag(kBracketsAbs2), PoolTag(kBrackets)},
       &DisassemblerZtf::MakeReadAbs<2, kBrackets>,
       &DisassemblerZtf::MakeWriteAbs<2, kBrackets>},
      {{9, TypeTag(kBracketsAbs3), PoolTag(kBrackets)},
       &DisassemblerZtf::MakeReadAbs<3, kBrackets>,
       &DisassemblerZtf::MakeWriteAbs<3, kBrackets>},
      {{7, TypeTag(kBracketsRel1), PoolTag(kBrackets)},
       &DisassemblerZtf::MakeReadRel<1, kBrackets>,
       &DisassemblerZtf::MakeWriteRel<1, kBrackets>},
      {{9, TypeTag(kBracketsRel2), PoolTag(kBrackets)},
       &DisassemblerZtf::MakeReadRel<2, kBrackets>,
       &DisassemblerZtf::MakeWriteRel<2, kBrackets>},
      {{11, TypeTag(kBracketsRel3), PoolTag(kBrackets)},
       &DisassemblerZtf::MakeReadRel<3, kBrackets>,
       &DisassemblerZtf::MakeWriteRel<3, kBrackets>},
      {{5, TypeTag(kParenthesesAbs1), PoolTag(kParentheses)},
       &DisassemblerZtf::MakeReadAbs<1, kParentheses>,
       &DisassemblerZtf::MakeWriteAbs<1, kParentheses>},
      {{7, TypeTag(kParenthesesAbs2), PoolTag(kParentheses)},
       &DisassemblerZtf::MakeReadAbs<2, kParentheses>,
       &DisassemblerZtf::MakeWriteAbs<2, kParentheses>},
      {{9, TypeTag(kParenthesesAbs3), PoolTag(kParentheses)},
       &DisassemblerZtf::MakeReadAbs<3, kParentheses>,
       &DisassemblerZtf::MakeWriteAbs<3, kParentheses>},
      {{7, TypeTag(kParenthesesRel1), PoolTag(kParentheses)},
       &DisassemblerZtf::MakeReadRel<1, kParentheses>,
       &DisassemblerZtf::MakeWriteRel<1, kParentheses>},
      {{9, TypeTag(kParenthesesRel2), PoolTag(kParentheses)},
       &DisassemblerZtf::MakeReadRel<2, kParentheses>,
       &DisassemblerZtf::MakeWriteRel<2, kParentheses>},
      {{11, TypeTag(kParenthesesRel3), PoolTag(kParentheses)},
       &DisassemblerZtf::MakeReadRel<3, kParentheses>,
       &DisassemblerZtf::MakeWriteRel<3, kParentheses>},
  };
}

template <uint8_t digits, DisassemblerZtf::ReferencePool pool>
std::unique_ptr<ReferenceReader> DisassemblerZtf::MakeReadAbs(offset_t lo,
                                                              offset_t hi) {
  static_assert(digits >= 1 && digits <= kMaxDigitCount,
                "|digits| must be in range [1, 3]");
  return std::make_unique<ZtfReferenceReader<ztf::LineCol>>(
      lo, hi, image_, translator_, MakeZtfConfig<pool>(digits));
}

template <uint8_t digits, DisassemblerZtf::ReferencePool pool>
std::unique_ptr<ReferenceReader> DisassemblerZtf::MakeReadRel(offset_t lo,
                                                              offset_t hi) {
  static_assert(digits >= 1 && digits <= kMaxDigitCount,
                "|digits| must be in range [1, 3]");
  return std::make_unique<ZtfReferenceReader<ztf::DeltaLineCol>>(
      lo, hi, image_, translator_, MakeZtfConfig<pool>(digits));
}

template <uint8_t digits, DisassemblerZtf::ReferencePool pool>
std::unique_ptr<ReferenceWriter> DisassemblerZtf::MakeWriteAbs(
    MutableBufferView image) {
  static_assert(digits >= 1 && digits <= kMaxDigitCount,
                "|digits| must be in range [1, 3]");
  return std::make_unique<ZtfReferenceWriter<ztf::LineCol>>(
      image, translator_, MakeZtfConfig<pool>(digits));
}

template <uint8_t digits, DisassemblerZtf::ReferencePool pool>
std::unique_ptr<ReferenceWriter> DisassemblerZtf::MakeWriteRel(
    MutableBufferView image) {
  static_assert(digits >= 1 && digits <= kMaxDigitCount,
                "|digits| must be in range [1, 3]");
  return std::make_unique<ZtfReferenceWriter<ztf::DeltaLineCol>>(
      image, translator_, MakeZtfConfig<pool>(digits));
}

bool DisassemblerZtf::Parse(ConstBufferView image) {
  image_ = image;
  if (!ReadZtfHeader(image_))
    return false;

  CHECK_GE(image_.size(),
           static_cast<size_t>(kTotalMagicSize));  // Needs header and footer.

  // Find the terminating footer "txTZ\n" that indicates the end of the image.
  offset_t offset = 0;
  for (; offset <= image_.size() - kFooterMagicSize; offset++) {
    if (image_.read<uint8_t>(offset) == 't' &&
        image_.read<uint8_t>(offset + 1) == 'x' &&
        image_.read<uint8_t>(offset + 2) == 'T' &&
        image_.read<uint8_t>(offset + 3) == 'Z' &&
        image_.read<uint8_t>(offset + 4) == '\n') {
      break;
    }
  }

  // If no footer is found before the end of the image then the parsing failed.
  if (offset > image_.size() - kFooterMagicSize)
    return false;
  image_.shrink(offset + kFooterMagicSize);

  return translator_.Init(image_);
}

}  // namespace zucchini
