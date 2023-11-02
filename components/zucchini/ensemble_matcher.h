// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ENSEMBLE_MATCHER_H_
#define COMPONENTS_ZUCCHINI_ENSEMBLE_MATCHER_H_

#include <stddef.h>

#include <vector>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/element_detection.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

// A base class for ensemble matching strategies, which identify Elements in a
// "new" and "old" archives, and match each "new" Element to an "old" Element.
// Matched pairs can then be passed to Disassembler for architecture-specific
// patching. Notes:
// - A matched Element pair must have the same ExecutableType.
// - Special case: Exact matches are ignored, since they can be patched directly
//   without architecture-specific patching.
// - Multiple "new" Elements may match a common "old" Element.
// - A "new" Element may have no match. This can happen when no viable match
//   exists, or when an exact match is skipped.
class EnsembleMatcher {
 public:
  EnsembleMatcher();
  EnsembleMatcher(const EnsembleMatcher&) = delete;
  const EnsembleMatcher& operator=(const EnsembleMatcher&) = delete;
  virtual ~EnsembleMatcher();

  // Interface to main matching feature. Returns whether match was successful.
  // This should be called at most once per instace.
  virtual bool RunMatch(ConstBufferView old_image,
                        ConstBufferView new_image) = 0;

  // Accessors to RunMatch() results.
  const std::vector<ElementMatch>& matches() const { return matches_; }

  size_t num_identical() const { return num_identical_; }

 protected:
  // Post-processes |matches_| to remove potentially unfavorable entries.
  void Trim();

  // Storage of matched elements: A list of matched pairs, where the list of
  // "new" elements have increasing offsets and don't overlap. May be empty.
  std::vector<ElementMatch> matches_;

  // Number of identical matches found in match candidates. These should be
  // excluded from |matches_|.
  size_t num_identical_ = 0;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ENSEMBLE_MATCHER_H_
