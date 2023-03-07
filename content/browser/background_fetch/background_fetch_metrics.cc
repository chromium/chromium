// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
namespace background_fetch {

// Exponential bucket spacing for UKM event data.
const double kUkmEventDataBucketSpacing = 2.0;

void RecordBackgroundFetchUkmEvent(
    const blink::StorageKey& storage_key,
    int requests_size,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    RenderFrameHostImpl* rfh,
    BackgroundFetchPermission permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Only record UKM data if there's an active RenderFrameHost associated.
  if (!rfh || !rfh->IsActive())
    return;

  // Only record UKM data if the origin of the last committed page is the same
  // as the one the background fetch was started with.
  // For a fenced frame, it should be treated as a sub frame for a UKM record.
  auto last_committed_origin =
      rfh->GetOutermostMainFrame()->GetLastCommittedOrigin();
  if (!storage_key.origin().IsSameOriginWith(last_committed_origin))
    return;
  ukm::SourceId source_id = rfh->GetPageUkmSourceId();

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
