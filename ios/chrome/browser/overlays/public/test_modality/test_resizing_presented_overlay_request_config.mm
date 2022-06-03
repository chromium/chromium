// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/test_modality/test_resizing_presented_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(TestResizingPresentedOverlay);

TestResizingPresentedOverlay::TestResizingPresentedOverlay(const CGRect& frame)
    : frame_(frame) {}

TestResizingPresentedOverlay::~TestResizingPresentedOverlay() = default;
