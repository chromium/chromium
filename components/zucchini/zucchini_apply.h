// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ZUCCHINI_APPLY_H_
#define COMPONENTS_ZUCCHINI_ZUCCHINI_APPLY_H_

#include "components/zucchini/image_utils.h"
#include "components/zucchini/patch_reader.h"
#include "components/zucchini/zucchini.h"

namespace zucchini {

// Reads equivalences from |patch_reader| to form preliminary |new_image|,
// copying regions from |old_image| and writing extra data from |patch_reader|.
bool ApplyEquivalenceAndExtraData(ConstBufferView old_image,
                                  const PatchElementReader& patch_reader,
                                  MutableBufferView new_image);

// Reads raw delta from |patch_reader| and applies corrections to |new_image|.
bool ApplyRawDelta(const PatchElementReader& patch_reader,
                   MutableBufferView new_image);

// Corrects references in |new_image| by projecting references from |old_image|
// and applying corrections from |patch_reader|. Both |old_image| and
// |new_image| are matching elements associated with |exe_type|.
bool ApplyReferencesCorrection(ExecutableType exe_type,
                               ConstBufferView old_image,
                               const PatchElementReader& patch_reader,
                               MutableBufferView new_image);

// Applies patch element with type |exe_type| from |patch_reader| on |old_image|
// to produce |new_image|.
bool ApplyElement(ExecutableType exe_type,
                  ConstBufferView old_image,
                  const PatchElementReader& patch_reader,
                  MutableBufferView new_image);

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ZUCCHINI_APPLY_H_
