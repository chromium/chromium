// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ZUCCHINI_TOOLS_H_
#define COMPONENTS_ZUCCHINI_ZUCCHINI_TOOLS_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/zucchini.h"

namespace zucchini {

// The functions below are called to print diagnosis information, so outputs are
// printed using std::ostream instead of LOG().

// Prints stats on references found in |image|. If |do_dump| is true, then
// prints all references (locations and targets).
status::Code ReadReferences(ConstBufferView image,
                            bool do_dump,
                            std::ostream& out);

// Prints regions and types of all detected executables in |image|. Appends
// detected subregions to |sub_image_list|.
status::Code DetectAll(ConstBufferView image,
                       std::ostream& out,
                       std::vector<ConstBufferView>* sub_image_list);

// Prints all matched regions from |old_image| to |new_image|.
// |imposed_matches|, if non-empty, encodes custom element matching to override
// the default element detection and matching heuristics, and is formatted as:
//   "#+#=#+#,#+#=#+#,..."  (e.g., "1+2=3+4", "1+2=3+4,5+6=7+8"),
// where "#+#=#+#" encodes a match as 4 unsigned integers:
//   [offset in "old", size in "old", offset in "new", size in "new"].
status::Code MatchAll(ConstBufferView old_image,
                      ConstBufferView new_image,
                      std::string imposed_matches,
                      std::ostream& out);

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ZUCCHINI_TOOLS_H_
