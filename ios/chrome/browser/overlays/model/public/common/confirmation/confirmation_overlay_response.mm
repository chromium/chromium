// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/common/confirmation/confirmation_overlay_response.h"

ConfirmationOverlayResponse::ConfirmationOverlayResponse(bool confirmed)
    : confirmed_(confirmed) {}

ConfirmationOverlayResponse::~ConfirmationOverlayResponse() = default;
