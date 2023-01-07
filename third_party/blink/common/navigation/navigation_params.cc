// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/navigation/navigation_params.h"

#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"

namespace blink {

mojom::CommonNavigationParamsPtr CreateCommonNavigationParams() {
  auto common_params = mojom::CommonNavigationParams::New();
  common_params->referrer = mojom::Referrer::New();
  common_params->navigation_start = base::TimeTicks::Now();
  common_params->source_location = network::mojom::SourceLocation::New();

  return common_params;
}

mojom::CommitNavigationParamsPtr CreateCommitNavigationParams() {
  auto commit_params = mojom::CommitNavigationParams::New();
  commit_params->navigation_token = base::UnguessableToken::Create();
  commit_params->navigation_timing = mojom::NavigationTiming::New();
  commit_params->navigation_api_history_entry_arrays =
      mojom::NavigationApiHistoryEntryArrays::New();

  return commit_params;
}

}  // namespace blink
