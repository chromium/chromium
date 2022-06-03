// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/test/test_overlay_presentation_context.h"

#import "ios/chrome/browser/ui/overlays/test/test_overlay_request_coordinator_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestOverlayPresentationContext::TestOverlayPresentationContext(Browser* browser)
    : OverlayPresentationContextImpl(
          browser,
          OverlayModality::kTesting,
          [[TestOverlayRequestCoordinatorFactory alloc]
              initWithBrowser:browser]) {}

TestOverlayPresentationContext::~TestOverlayPresentationContext() = default;
