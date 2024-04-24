// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_document_data.h"
#include "content/public/browser/document_user_data.h"

namespace content {

DOCUMENT_USER_DATA_KEY_IMPL(FencedDocumentData);

FencedDocumentData::~FencedDocumentData() = default;

FencedDocumentData::FencedDocumentData(RenderFrameHost* rfh)
    : DocumentUserData(rfh) {}

const std::optional<AutomaticBeaconInfo>
FencedDocumentData::GetAutomaticBeaconInfo(
    blink::mojom::AutomaticBeaconType event_type) const {
  auto it = automatic_beacon_info_.find(event_type);
  if (it == automatic_beacon_info_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void FencedDocumentData::UpdateAutomaticBeaconData(
    blink::mojom::AutomaticBeaconType event_type,
    const std::string& event_data,
    const std::vector<blink::FencedFrame::ReportingDestination>& destinations,
    bool once,
    bool cross_origin_exposed) {
  automatic_beacon_info_[event_type] =
      AutomaticBeaconInfo(event_data, destinations, once, cross_origin_exposed);
}

void FencedDocumentData::MaybeResetAutomaticBeaconData(
    blink::mojom::AutomaticBeaconType event_type) {
  auto it = automatic_beacon_info_.find(event_type);
  if (it != automatic_beacon_info_.end() && it->second.once == true) {
    automatic_beacon_info_.erase(it);
  }
}

void FencedDocumentData::RunDisabledUntrustedNetworkCallbacks() {
  for (auto& callback : on_disabled_untrusted_network_callbacks_) {
    if (!callback.is_null()) {
      std::move(callback).Run();
    }
  }
  on_disabled_untrusted_network_callbacks_.clear();
}

}  // namespace content
