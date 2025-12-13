// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/process_selection_user_data.h"

namespace content {

ProcessSelectionUserData::ProcessSelectionUserData() = default;
ProcessSelectionUserData::~ProcessSelectionUserData() = default;

base::SafeRef<ProcessSelectionUserData> ProcessSelectionUserData::GetSafeRef() {
  return weak_ptr_factory_.GetSafeRef();
}

}  // namespace content
