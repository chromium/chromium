// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_HEURISTIC_ENSEMBLE_MATCHER_H_
#define COMPONENTS_ZUCCHINI_HEURISTIC_ENSEMBLE_MATCHER_H_

#include <ostream>

#include "base/memory/raw_ptr.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/ensemble_matcher.h"

namespace zucchini {

// An ensemble matcher that:
// - Detects embedded elements in "old" and "new" archive files.
// - Applies heuristics to create matched pairs.
// It is desired to have matched pairs that:
// - Have "reasonable" size difference (see UnsafeDifference() in the .cc file).
// - Have "minimal distance" among other potential matched pairs.
class HeuristicEnsembleMatcher : public EnsembleMatcher {
 public:
  explicit HeuristicEnsembleMatcher(std::ostream* out);
  HeuristicEnsembleMatcher(const HeuristicEnsembleMatcher&) = delete;
  const HeuristicEnsembleMatcher& operator=(const HeuristicEnsembleMatcher&) =
      delete;
  ~HeuristicEnsembleMatcher() override;

  // EnsembleMatcher:
  bool RunMatch(ConstBufferView old_image, ConstBufferView new_image) override;

 private:
  // Optional stream to print detailed information during matching.
  raw_ptr<std::ostream> out_ = nullptr;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_HEURISTIC_ENSEMBLE_MATCHER_H_
