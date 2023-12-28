// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/progress_indicator_app_interface.h"

#import <UIKit/UIKit.h>

#import <MaterialComponents/MaterialProgressView.h>

#import "base/apple/foundation_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"

@implementation ProgressIndicatorAppInterface

+ (id<GREYMatcher>)progressViewWithProgress:(CGFloat)progress {
  GREYMatchesBlock matches = ^BOOL(UIView* view) {
    MDCProgressView* progressView =
        base::apple::ObjCCast<MDCProgressView>(view);
    return progressView && progressView.progress == progress;
  };

  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"progress view with progress: "];
    [description appendText:@(progress).stringValue];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

@end
