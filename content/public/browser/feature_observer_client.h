// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEATURE_OBSERVER_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_FEATURE_OBSERVER_CLIENT_H_

#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom.h"

namespace content {

class CONTENT_EXPORT FeatureObserverClient {
 public:
  virtual ~FeatureObserverClient() {}

  // These functions are invoked after the number of features of |feature_type|
  // used by a frame switches between zero to non-zero. There is no guarantee
  // that the frame identified by |render_process_id| and |render_frame_id|
  // still exists when this is called.
  virtual void OnStartUsing(GlobalRenderFrameHostId id,
                            blink::mojom::ObservedFeatureType feature_type) = 0;
  virtual void OnStopUsing(GlobalRenderFrameHostId id,
                           blink::mojom::ObservedFeatureType feature_type) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEATURE_OBSERVER_CLIENT_H_
