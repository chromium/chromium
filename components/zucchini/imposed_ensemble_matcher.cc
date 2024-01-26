// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/imposed_ensemble_matcher.h"

#include <sstream>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "components/zucchini/io_utils.h"

namespace zucchini {

/******** ImposedMatchParser ********/

ImposedMatchParser::ImposedMatchParser() = default;

ImposedMatchParser::~ImposedMatchParser() = default;

ImposedMatchParser::Status ImposedMatchParser::Parse(
    std::string imposed_matches,
    ConstBufferView old_image,
    ConstBufferView new_image,
    ElementDetector&& detector) {
  CHECK(matches_.empty());
  CHECK(bad_matches_.empty());

  // Parse |imposed_matches| and check bounds.
  std::istringstream iss(std::move(imposed_matches));
  bool first = true;
  iss.peek();  // Makes empty |iss| realize EOF is reached.
  while (iss && !iss.eof()) {
    // Eat delimiter.
    if (first) {
      first = false;
    } else if (!(iss >> EatChar(','))) {
      return kInvalidDelimiter;
    }
    // Extract parameters for one imposed match.
    offset_t old_offset = 0U;
    size_t old_size = 0U;
    offset_t new_offset = 0U;
    size_t new_size = 0U;
    if (!(iss >> StrictUInt<offset_t>(old_offset) >> EatChar('+') >>
          StrictUInt<size_t>(old_size) >> EatChar('=') >>
          StrictUInt<offset_t>(new_offset) >> EatChar('+') >>
          StrictUInt<size_t>(new_size))) {
      return kParseError;
    }
    // Check bounds.
    if (old_size == 0 || new_size == 0 ||
        !old_image.covers({old_offset, old_size}) ||
        !new_image.covers({new_offset, new_size})) {
      return kOutOfBound;
    }
    matches_.push_back(
        {{{old_offset, old_size}, kExeTypeUnknown},    // Assign type later.
         {{new_offset, new_size}, kExeTypeUnknown}});  // Assign type later.
  }
  // Sort matches by "new" file offsets. This helps with overlap checks.
  std::sort(matches_.begin(), matches_.end(),
            [](const ElementMatch& match_a, const ElementMatch& match_b) {
              return match_a.new_element.offset < match_b.new_element.offset;
            });

  // Check for overlaps in "new" file.
  if (base::ranges::adjacent_find(
          matches_, [](const ElementMatch& match1, const ElementMatch& match2) {
            return match1.new_element.hi() > match2.new_element.lo();
          }) != matches_.end()) {
    return kOverlapInNew;
  }

  // Compute types and verify consistency. Remove identical matches and matches
  // where any sub-image has an unknown type.
  size_t write_idx = 0;
  for (size_t read_idx = 0; read_idx < matches_.size(); ++read_idx) {
    ConstBufferView old_sub_image(
        old_image[matches_[read_idx].old_element.region()]);
    ConstBufferView new_sub_image(
        new_image[matches_[read_idx].new_element.region()]);
    // Remove identical match.
    if (old_sub_image.equals(new_sub_image)) {
      ++num_identical_;
      continue;
    }
    // Check executable types of sub-images.
    std::optional<Element> old_element = detector.Run(old_sub_image);
    std::optional<Element> new_element = detector.Run(new_sub_image);
    if (!old_element || !new_element) {
      // Skip unknown types, including those mixed with known types.
      bad_matches_.push_back(matches_[read_idx]);
      continue;
    } else if (old_element->exe_type != new_element->exe_type) {
      // Error if types are known, but inconsistent.
      return kTypeMismatch;
    }

    // Keep match and remove gaps.
    matches_[read_idx].old_element.exe_type = old_element->exe_type;
    matches_[read_idx].new_element.exe_type = new_element->exe_type;
    if (write_idx < read_idx)
      matches_[write_idx] = matches_[read_idx];
    ++write_idx;
  }
  matches_.resize(write_idx);
  return kSuccess;
}

/******** ImposedEnsembleMatcher ********/

ImposedEnsembleMatcher::ImposedEnsembleMatcher(
    const std::string& imposed_matches)
    : imposed_matches_(imposed_matches) {}

ImposedEnsembleMatcher::~ImposedEnsembleMatcher() = default;

bool ImposedEnsembleMatcher::RunMatch(ConstBufferView old_image,
                                      ConstBufferView new_image) {
  DCHECK(matches_.empty());
  LOG(INFO) << "Start matching.";
  ImposedMatchParser parser;
  ImposedMatchParser::Status status =
      parser.Parse(std::move(imposed_matches_), old_image, new_image,
                   base::BindRepeating(DetectElementFromDisassembler));
  // Print all warnings first.
  for (const ElementMatch& bad_match : *parser.mutable_bad_matches())
    LOG(WARNING) << "Skipped match with unknown type: " << bad_match.ToString();
  if (status != ImposedMatchParser::kSuccess) {
    LOG(ERROR) << "Imposed match failed with error code " << status << ".";
    return false;
  }
  num_identical_ = parser.num_identical();
  matches_ = std::move(*parser.mutable_matches());
  Trim();
  return true;
}

}  // namespace zucchini
