// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/common/confirmation/confirmation_overlay_response_util.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/overlays/model/public/common/confirmation/confirmation_overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"

using alert_overlays::AlertResponse;
using alert_overlays::ResponseConverter;

namespace {
// Parses the AlertResponse from `response` and produces a
// ConfirmationOverlayResponse if the alert response's tapped button is
// `confirm_button_row_index`.
std::unique_ptr<OverlayResponse> CreateConfirmResponse(
    size_t confirm_button_row_index,
    std::unique_ptr<OverlayResponse> response) {
  AlertResponse* alert_response = response->GetInfo<AlertResponse>();
  if (!alert_response)
    return nullptr;
  return OverlayResponse::CreateWithInfo<ConfirmationOverlayResponse>(
      alert_response->tapped_button_row_index() == confirm_button_row_index);
}
}

alert_overlays::ResponseConverter GetConfirmationResponseConverter(
    size_t confirm_button_row_index) {
  return base::BindRepeating(&CreateConfirmResponse, confirm_button_row_index);
}
