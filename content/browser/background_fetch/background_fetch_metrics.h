// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_METRICS_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_METRICS_H_

#include "content/public/browser/background_fetch_delegate.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {
namespace background_fetch {

// Records the number of registrations that have unfinished fetches found on
// start-up.
void RecordRegistrationsOnStartup(int num_registrations);

// Records the BackgroundFetch UKM event. Must be called before a Background
// Fetch registration has been created. Will be a no-op if |frame_tree_node_id|
// does not identify a valid, live frame.
void RecordBackgroundFetchUkmEvent(
    const url::Origin& origin,
    int requests_size,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    int frame_tree_node_id,
    BackgroundFetchPermission permission);

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_METRICS_H_
