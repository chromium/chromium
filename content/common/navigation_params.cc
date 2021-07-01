// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigation_params.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"

namespace content {

blink::mojom::CommonNavigationParamsPtr CreateCommonNavigationParams() {
  auto common_params = blink::mojom::CommonNavigationParams::New();
  common_params->referrer = blink::mojom::Referrer::New();
  common_params->navigation_start = base::TimeTicks::Now();
  common_params->source_location = network::mojom::SourceLocation::New();

  return common_params;
}

blink::mojom::CommitNavigationParamsPtr CreateCommitNavigationParams() {
  auto commit_params = blink::mojom::CommitNavigationParams::New();
  commit_params->navigation_token = base::UnguessableToken::Create();
  commit_params->navigation_timing = blink::mojom::NavigationTiming::New();

  return commit_params;
}

}  // namespace content
