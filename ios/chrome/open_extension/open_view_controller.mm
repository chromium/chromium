// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/open_extension/open_view_controller.h"

#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@implementation OpenViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{{
        [self.extensionContext
         cancelRequestWithError:[NSError errorWithDomain:@"Open in Chrome"
                                                    code:0
                                                userInfo:@{
            NSLocalizedDescriptionKey :
                @"Extension is not supported"
         }]];
}
});
}
@end
