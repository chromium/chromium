// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_camera_handler.h"

#import <UIKit/UIKit.h>

#import "components/prefs/pref_service.h"

@implementation GeminiCameraHandler {
  // The pref service used by this handler.
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
  }
  return self;
}

#pragma mark - GeminiCameraDelegate

- (void)openCameraFromViewController:(UIViewController*)presentingViewController
                      withCompletion:
                          (void (^)(NSArray<UIImage*>*, NSError*))completion {
  // TODO(crbug.com/455905539): Implement camera opening logic.
}

@end
