// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/test_text_zoom_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestTextZoomProvider::TestTextZoomProvider() = default;

TestTextZoomProvider::~TestTextZoomProvider() = default;

bool TestTextZoomProvider::IsTextZoomEnabled() {
  return true;
}
