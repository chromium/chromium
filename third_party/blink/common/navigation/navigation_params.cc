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
  commit_params->content_settings = CreateDefaultRendererContentSettings();

  return commit_params;
}

mojom::RendererContentSettingsPtr CreateDefaultRendererContentSettings() {
  // These defaults are used in exactly 3 places:
  //   (1) A new empty window does not go through "navigation" and thus needs
  //   default values. As this is an empty window, the values do not matter.
  //   (2) On navigation error, the renderer sets the URL to
  //   kUnreachableWebDataURL. This page does have script and images, which we
  //   always want to allow regardless of the user's content settings.
  //   (3) When content settings are not supported on a given platform (e.g.
  //   allow_image is not supported on Android), then these defaults are used.
  return mojom::RendererContentSettings::New(
      /*allow_script=*/true, /*allow_image=*/true, /*allow_popup=*/false,
      /*allow_mixed_content=*/false);
}

}  // namespace blink
