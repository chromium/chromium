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
}

FeatureObserver::~FeatureObserver() = default;

void FeatureObserver::GetFeatureObserver(
    mojo::PendingReceiver<blink::mojom::FeatureObserver> receiver) {
  observers_.Add(this, std::move(receiver));
}

void FeatureObserver::Register(
    mojo::PendingReceiver<blink::mojom::ObservedFeature> feature,
    blink::mojom::ObservedFeatureType type,
    uint32_t name_hash) {
  DCHECK(client_);

  auto& receiver_set = features_[{type, name_hash}];

  // Notify if this is the first receiver.
  if (receiver_set.empty()) {
    receiver_set.set_disconnect_handler(
        base::BindRepeating(&FeatureObserver::OnObservedFeatureDisconnected,
                            base::Unretained(this), type, name_hash));
    client_->OnStartUsing(id_, type, name_hash);
  }

  receiver_set.Add(nullptr, std::move(feature));
}

void FeatureObserver::OnObservedFeatureDisconnected(
    blink::mojom::ObservedFeatureType type,
    uint32_t name_hash) {
  auto it = features_.find({type, name_hash});
  CHECK(it != features_.end());

  // If this was the last receiver, erase the entry and notify the client.
  auto& receiver_set = it->second;
  if (receiver_set.empty()) {
    features_.erase(it);
    client_->OnStopUsing(id_, type, name_hash);
  }
}

}  // namespace content
