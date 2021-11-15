// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_PAGE_LIFECYCLE_STATE_UPDATER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_PAGE_LIFECYCLE_STATE_UPDATER_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/page/page.mojom-shared.h"

namespace blink {

// Returns true if this state update is for the page being restored from
// back-forward cache, causing the pageshow event to fire with persisted=true.
// Templated to be usable in both
// the browser with `blink::mojom::PageLifecycleStatePtr` and renderer with
// `mojom::blink::PageLifecycleStatePtr`.
// TODO(https://crbug.com/1234634): Remove this when this bug is fixed.
template <class PageLifecycleStatePtrT>
bool IsRestoredFromBackForwardCache(const PageLifecycleStatePtrT& old_state,
                                    const PageLifecycleStatePtrT& new_state) {
  if (!old_state)
    return false;
  bool old_state_hidden = old_state->pagehide_dispatch !=
                          blink::mojom::PagehideDispatch::kNotDispatched;
  bool new_state_shown = new_state->pagehide_dispatch ==
                         blink::mojom::PagehideDispatch::kNotDispatched;
  // It's a pageshow but it can't be the initial pageshow since it was already
  // hidden. So it must be a back-forward cache restore.
  return old_state_hidden && new_state_shown;
}
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_PAGE_LIFECYCLE_STATE_UPDATER_H_
