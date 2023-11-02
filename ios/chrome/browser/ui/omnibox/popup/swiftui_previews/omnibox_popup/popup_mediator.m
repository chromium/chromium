// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "popup_mediator.h"

#import "omnibox_popup-Swift.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PopupMediator

@synthesize model = _model;

- (PopupModel*)model {
  if (!_model) {
    _model = [self createModel];
  }
  return _model;
}

- (PopupModel*)createModel {
  PopupModel* model =
      [[PopupModel alloc] initWithMatches:@[ PopupMatch.previews ]
                                  headers:@[ @"" ]
                               dataSource:nil
                                 delegate:nil];
  return model;
}

@end
