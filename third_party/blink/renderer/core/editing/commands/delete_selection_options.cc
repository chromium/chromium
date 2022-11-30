// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/delete_selection_options.h"

namespace blink {

DeleteSelectionOptions::DeleteSelectionOptions(const DeleteSelectionOptions&) =
    default;
DeleteSelectionOptions::DeleteSelectionOptions() = default;

bool DeleteSelectionOptions::IsExpandForSpecialElements() const {
  return is_expand_for_special_elements_;
}
bool DeleteSelectionOptions::IsMergeBlocksAfterDelete() const {
  return is_merge_blocks_after_delete_;
}
bool DeleteSelectionOptions::IsSanitizeMarkup() const {
  return is_sanitize_markup_;
}
bool DeleteSelectionOptions::IsSmartDelete() const {
  return is_smart_delete_;
}

// static
DeleteSelectionOptions DeleteSelectionOptions::NormalDelete() {
  return Builder()
      .SetMergeBlocksAfterDelete(true)
      .SetExpandForSpecialElements(true)
      .SetSanitizeMarkup(true)
      .Build();
}

DeleteSelectionOptions DeleteSelectionOptions::SmartDelete() {
  return Builder()
      .SetSmartDelete(true)
      .SetMergeBlocksAfterDelete(true)
      .SetExpandForSpecialElements(true)
      .SetSanitizeMarkup(true)
      .Build();
}

// ----
DeleteSelectionOptions::Builder::Builder() = default;

DeleteSelectionOptions DeleteSelectionOptions::Builder::Build() const {
  return options_;
}

DeleteSelectionOptions::Builder&
DeleteSelectionOptions::Builder::SetExpandForSpecialElements(bool value) {
  options_.is_expand_for_special_elements_ = value;
  return *this;
}

DeleteSelectionOptions::Builder&
DeleteSelectionOptions::Builder::SetMergeBlocksAfterDelete(bool value) {
  options_.is_merge_blocks_after_delete_ = value;
  return *this;
}

DeleteSelectionOptions::Builder&
DeleteSelectionOptions::Builder::SetSanitizeMarkup(bool value) {
  options_.is_sanitize_markup_ = value;
  return *this;
}

DeleteSelectionOptions::Builder&
DeleteSelectionOptions::Builder::SetSmartDelete(bool value) {
  options_.is_smart_delete_ = value;
  return *this;
}

}  //  namespace blink
