// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/common/confirmation/confirmation_overlay_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(ConfirmationOverlayResponse);

ConfirmationOverlayResponse::ConfirmationOverlayResponse(bool confirmed)
    : confirmed_(confirmed) {}

ConfirmationOverlayResponse::~ConfirmationOverlayResponse() = default;
