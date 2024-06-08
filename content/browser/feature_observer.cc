// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/feature_observer.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/feature_observer_client.h"
#include "content/public/common/content_client.h"

namespace content {

FeatureObserver::FeatureObserver(FeatureObserverClient* client,
                                 GlobalRenderFrameHostId id)
    : client_(client), id_(id) {
  DCHECK(client_);

  for (size_t i = 0;
       i <= static_cast<size_t>(blink::mojom::ObservedFeatureType::kMaxValue);
       ++i) {
    features_by_type_[i].set_disconnect_handler(base::BindRepeating(
        [](mojo::ReceiverSet<blink::mojom::ObservedFeature>* set,
           FeatureObserverClient* client, GlobalRenderFrameHostId id,
           blink::mojom::ObservedFeatureType type) {
          if (!set->empty())
            return;

          // Notify if this is the last receiver.
          client->OnStopUsing(id, type);
        },
        base::Unretained(&features_by_type_[i]), client_, id_,
        static_cast<blink::mojom::ObservedFeatureType>(i)));
  }
}

FeatureObserver::~FeatureObserver() = default;

void FeatureObserver::GetFeatureObserver(
    mojo::PendingReceiver<blink::mojom::FeatureObserver> receiver) {
  observers_.Add(this, std::move(receiver));
}

void FeatureObserver::Register(
    mojo::PendingReceiver<blink::mojom::ObservedFeature> feature,
    blink::mojom::ObservedFeatureType type) {
  DCHECK(client_);

  auto& set = features_by_type_[static_cast<int>(type)];

  // Notify if this is the first receiver.
  if (set.empty())
    client_->OnStartUsing(id_, type);

  set.Add(nullptr, std::move(feature));
}

}  // namespace content
