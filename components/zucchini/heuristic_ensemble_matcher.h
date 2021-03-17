// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_HEURISTIC_ENSEMBLE_MATCHER_H_
#define COMPONENTS_ZUCCHINI_HEURISTIC_ENSEMBLE_MATCHER_H_

#include <ostream>

#include "base/macros.h"
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
  ~HeuristicEnsembleMatcher() override;

  // EnsembleMatcher:
  bool RunMatch(ConstBufferView old_image, ConstBufferView new_image) override;

 private:
  // Optional stream to print detailed information during matching.
  std::ostream* out_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HeuristicEnsembleMatcher);
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_HEURISTIC_ENSEMBLE_MATCHER_H_
