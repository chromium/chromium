// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_IMPOSED_ENSEMBLE_MATCHER_H_
#define COMPONENTS_ZUCCHINI_IMPOSED_ENSEMBLE_MATCHER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/element_detection.h"
#include "components/zucchini/ensemble_matcher.h"

namespace zucchini {

// A class to parse imposed match format, which is either an empty string (no
// imposed patch), or a string formatted as:
//   "#+#=#+#,#+#=#+#,..."  (e.g., "1+2=3+4", "1+2=3+4,5+6=7+8"),
// where "#+#=#+#" encodes a match as 4 unsigned integers:
//   [offset in "old", size in "old", offset in "new", size in "new"].
class ImposedMatchParser {
 public:
  enum Status {
    kSuccess,
    kInvalidDelimiter,
    kParseError,
    kOutOfBound,
    kOverlapInNew,
    kTypeMismatch,
  };

  ImposedMatchParser();
  ImposedMatchParser(const ImposedMatchParser&) = delete;
  const ImposedMatchParser& operator=(const ImposedMatchParser&) = delete;
  ~ImposedMatchParser();

  // Parses |imposed_matches| and writes the results to member variables.
  // |old_image| and |new_image| are used for validation. Returns a Status value
  // to signal success or various error modes. |detector| is used to validate
  // Element types for matched pairs. This should only be called once for each
  // instance.
  Status Parse(std::string imposed_matches,
               ConstBufferView old_image,
               ConstBufferView new_image,
               ElementDetector&& detector);

  size_t num_identical() const { return num_identical_; }
  std::vector<ElementMatch>* mutable_matches() { return &matches_; }
  std::vector<ElementMatch>* mutable_bad_matches() { return &bad_matches_; }

 private:
  size_t num_identical_ = 0;
  std::vector<ElementMatch> matches_;
  // Stores "forgiven" bad matches, so the caller can impose matches for
  // unsupported image types (which will simply be ignored). Note that imposing
  // matches for known but incompatible image types would result in error.
  std::vector<ElementMatch> bad_matches_;
};

// An ensemble matcher that parses a format string that describes matches.
class ImposedEnsembleMatcher : public EnsembleMatcher {
 public:
  // |imposed_matches| specifies imposed maches, using a format described below.
  // Validation is performed in RunMatch().
  explicit ImposedEnsembleMatcher(const std::string& imposed_matches);
  ImposedEnsembleMatcher(const ImposedEnsembleMatcher&) = delete;
  const ImposedEnsembleMatcher& operator=(const ImposedEnsembleMatcher&) =
      delete;
  ~ImposedEnsembleMatcher() override;

  // EnsembleMatcher:
  bool RunMatch(ConstBufferView old_image, ConstBufferView new_image) override;

 private:
  const std::string imposed_matches_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_IMPOSED_ENSEMBLE_MATCHER_H_
