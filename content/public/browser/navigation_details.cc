// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_details.h"

namespace content {

LoadCommittedDetails::LoadCommittedDetails() = default;

LoadCommittedDetails::LoadCommittedDetails(const LoadCommittedDetails&) =
    default;

LoadCommittedDetails& LoadCommittedDetails::operator=(
    const LoadCommittedDetails&) = default;

}  // namespace content
