// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/common/confirmation/confirmation_overlay_response_util.h"

#include "base/bind.h"
#import "ios/chrome/browser/overlays/public/common/confirmation/confirmation_overlay_response.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using alert_overlays::AlertResponse;
using alert_overlays::ResponseConverter;

namespace {
// Parses the AlertResponse from |response| and produces a
// ConfirmationOverlayResponse if the alert response's tapped button is
// |confirm_button_index|.
std::unique_ptr<OverlayResponse> CreateConfirmResponse(
    size_t confirm_button_index,
    std::unique_ptr<OverlayResponse> response) {
  AlertResponse* alert_response = response->GetInfo<AlertResponse>();
  if (!alert_response)
    return nullptr;
  return OverlayResponse::CreateWithInfo<ConfirmationOverlayResponse>(
      alert_response->tapped_button_index() == confirm_button_index);
}
}

alert_overlays::ResponseConverter GetConfirmationResponseConverter(
    size_t confirm_button_index) {
  return base::BindRepeating(&CreateConfirmResponse, confirm_button_index);
}
