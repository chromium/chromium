// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_details.h"

namespace content {

LoadCommittedDetails::LoadCommittedDetails()
    : entry(nullptr),
      type(content::NAVIGATION_TYPE_UNKNOWN),
      previous_entry_index(-1),
      did_replace_entry(false),
      is_same_document(false),
      is_main_frame(true),
      http_status_code(0) {}

LoadCommittedDetails::LoadCommittedDetails(const LoadCommittedDetails&) =
    default;

LoadCommittedDetails& LoadCommittedDetails::operator=(
    const LoadCommittedDetails&) = default;

}  // namespace content
