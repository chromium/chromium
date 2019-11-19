// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace content {
namespace background_fetch {

// Exponential bucket spacing for UKM event data.
const double kUkmEventDataBucketSpacing = 2.0;

void RecordRegistrationsOnStartup(int num_registrations) {
  UMA_HISTOGRAM_COUNTS_100("BackgroundFetch.IncompleteFetchesOnStartup",
                           num_registrations);
}

void RecordBackgroundFetchUkmEvent(
    const url::Origin& origin,
    int requests_size,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    int frame_tree_node_id,
    BackgroundFetchPermission permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Only record UKM data if there's a frame associated.
  auto* web_contents = WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return;

  // Only record UKM data if the origin of the page currently being displayed
  // is the same as the one the background fetch was started with.
  auto displayed_origin = web_contents->GetLastCommittedURL().GetOrigin();
  if (!origin.IsSameOriginWith(url::Origin::Create(displayed_origin)))
    return;
  ukm::SourceId source_id = static_cast<WebContentsImpl*>(web_contents)
                                ->GetUkmSourceIdForLastCommittedSource();

  ukm::builders::BackgroundFetch(source_id)
      .SetHasTitle(!options->title.empty())
      .SetNumIcons(options->icons.size())
      .SetRatioOfIdealToChosenIconSize(ukm_data->ideal_to_chosen_icon_size)
      .SetDownloadTotal(ukm::GetExponentialBucketMin(
          options->download_total, kUkmEventDataBucketSpacing))
      .SetNumRequestsInFetch(ukm::GetExponentialBucketMin(
          requests_size, kUkmEventDataBucketSpacing))
      .SetDeniedDueToPermissions(permission ==
                                 BackgroundFetchPermission::BLOCKED)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace background_fetch
}  // namespace content
