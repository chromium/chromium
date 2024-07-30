// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/test/test_overlay_presentation_context.h"

#import "ios/chrome/browser/overlays/ui_bundled/test/test_overlay_request_coordinator_factory.h"

TestOverlayPresentationContext::TestOverlayPresentationContext(Browser* browser)
    : OverlayPresentationContextImpl(
          browser,
          OverlayModality::kTesting,
          [[TestOverlayRequestCoordinatorFactory alloc]
              initWithBrowser:browser
                     modality:OverlayModality::kTesting]) {}

TestOverlayPresentationContext::~TestOverlayPresentationContext() = default;
