// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FEATURE_OBSERVER_H_
#define CONTENT_BROWSER_FEATURE_OBSERVER_H_

#include "base/containers/stack_container.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom.h"

namespace content {

class FeatureObserverClient;

// Observer interface to be notified when frames hold resources.
// client interfaces will be called on the same sequence GetFeatureObserver is
// called from.
class CONTENT_EXPORT FeatureObserver : public blink::mojom::FeatureObserver {
 public:
  // |client_| must outlive FeatureObserver.
  FeatureObserver(FeatureObserverClient* client, GlobalFrameRoutingId id);
  ~FeatureObserver() override;

  FeatureObserver(const FeatureObserver&) = delete;
  FeatureObserver& operator=(const FeatureObserver&) = delete;

  void GetFeatureObserver(
      mojo::PendingReceiver<blink::mojom::FeatureObserver> receiver);

  // blink::mojom::FeatureObserver implementation:
  // For a given FeatureObserver receiver passed in through Bind, register the
  // lifetime of a feature of a given type.
  void Register(mojo::PendingReceiver<blink::mojom::ObservedFeature> feature,
                blink::mojom::ObservedFeatureType type) override;

 private:
  // FeatureObservers notifying us about features used in this frame.
  mojo::ReceiverSet<blink::mojom::FeatureObserver> observers_;

  // Registered features.
  mojo::ReceiverSet<blink::mojom::ObservedFeature> features_by_type_
      [static_cast<int>(blink::mojom::ObservedFeatureType::kMaxValue) + 1];

  FeatureObserverClient* const client_;
  const GlobalFrameRoutingId id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FEATURE_OBSERVER_H_
