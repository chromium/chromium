// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/frame_ad_evidence_mojom_traits.h"

namespace mojo {

bool StructTraits<
    blink::mojom::FrameAdEvidenceDataView,
    blink::FrameAdEvidence>::Read(blink::mojom::FrameAdEvidenceDataView data,
                                  blink::FrameAdEvidence* out) {
  *out = blink::FrameAdEvidence(data.parent_is_ad());

  // First, read the most restrictive filter list result. Updating the filter
  // list result here sets both the most restrictive and the latest filter list
  // results to the deserialized values.
  blink::mojom::FilterListResult most_restrictive_filter_list_result;
  if (!data.ReadMostRestrictiveFilterListResult(
          &most_restrictive_filter_list_result))
    return false;
  out->UpdateFilterListResult(most_restrictive_filter_list_result);

  // Then, read the latest filter list result. This should never be more
  // restrictive than the (previously read) most restrictive filter list result.
  blink::mojom::FilterListResult latest_filter_list_result;
  if (!data.ReadLatestFilterListResult(&latest_filter_list_result))
    return false;
  if (most_restrictive_filter_list_result !=
      blink::MoreRestrictiveFilterListEvidence(
          latest_filter_list_result, most_restrictive_filter_list_result)) {
    return false;
  }

  // Now, run another update to fix the latest filter list result, without
  // affecting the most restrictive.
  out->UpdateFilterListResult(latest_filter_list_result);

  blink::mojom::FrameCreationStackEvidence created_by_ad_script;
  if (!data.ReadCreatedByAdScript(&created_by_ad_script))
    return false;
  out->set_created_by_ad_script(created_by_ad_script);

  if (data.is_complete())
    out->set_is_complete();

  return true;
}

}  // namespace mojo
