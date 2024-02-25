// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/model/find_in_page_app_interface.h"

#import "ios/chrome/browser/find_in_page/model/find_in_page_controller.h"

@implementation FindInPageAppInterface

+ (void)clearSearchTerm {
  [FindInPageController clearSearchTerm];
}

@end
