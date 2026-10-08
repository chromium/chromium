// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_ui_test_util.h"

BOOL HasLargeContentViewerInteraction(UIView* view) {
  for (id<UIInteraction> interaction in view.interactions) {
    if ([interaction isKindOfClass:[UILargeContentViewerInteraction class]]) {
      return YES;
    }
  }
  return NO;
}
