// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FEATURE_OBSERVER_H_
#define CONTENT_BROWSER_FEATURE_OBSERVER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom.h"

namespace content {

class FeatureObserverClient;

// Observer interface to be notified when frames hold resources.
// client interfaces will be called on the same sequence GetFeatureObserver is
// called from.
class FeatureObserver : public blink::mojom::FeatureObserver {
 public:
  // |client_| must outlive FeatureObserver.
  FeatureObserver(FeatureObserverClient* client, GlobalRenderFrameHostId id);
  ~FeatureObserver() override;

  FeatureObserver(const FeatureObserver&) = delete;
  FeatureObserver& operator=(const FeatureObserver&) = delete;

  void GetFeatureObserver(
      mojo::PendingReceiver<blink::mojom::FeatureObserver> receiver);

  // blink::mojom::FeatureObserver implementation:
  // For a given FeatureObserver receiver passed in through Bind, register the
  // lifetime of a feature of a given type.
  void Register(mojo::PendingReceiver<blink::mojom::ObservedFeature> feature,
                blink::mojom::ObservedFeatureType type,
                uint32_t name_hash) override;

  void OnObservedFeatureDisconnected(blink::mojom::ObservedFeatureType type,
                                     uint32_t name_hash);

 private:
  // FeatureObservers notifying us about features used in this frame.
  mojo::ReceiverSet<blink::mojom::FeatureObserver> observers_;

  // Track features. A std::map is used instead of base::flat_map because
  // mojo::ReceiverSet is not copyable or movable.
  using MapKey = std::pair<blink::mojom::ObservedFeatureType, uint32_t>;
  std::map<MapKey, mojo::ReceiverSet<blink::mojom::ObservedFeature>> features_;

  const raw_ptr<FeatureObserverClient> client_;
  const GlobalRenderFrameHostId id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FEATURE_OBSERVER_H_
