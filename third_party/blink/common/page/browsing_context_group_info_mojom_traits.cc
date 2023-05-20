// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/browsing_context_group_info_mojom_traits.h"

#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::BrowsingContextGroupInfoDataView,
                  blink::BrowsingContextGroupInfo>::
    Read(blink::mojom::BrowsingContextGroupInfoDataView data,
         blink::BrowsingContextGroupInfo* out_browsing_context_group_info) {
  if (!data.ReadBrowsingContextGroupToken(
          &(out_browsing_context_group_info->browsing_context_group_token))) {
    return false;
  }
  if (!data.ReadCoopRelatedGroupToken(
          &(out_browsing_context_group_info->coop_related_group_token))) {
    return false;
  }

  return true;
}

}  // namespace mojo
