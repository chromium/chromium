// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"

#import "ios/chrome/browser/shared/public/features/features.h"

namespace {
const CGFloat kAppBarHeightDefault = 77;
const CGFloat kAppBarHeightLandscapeDefault = 69;
}  // namespace

const CGFloat kAppBarHeightFullscreen = 62;

CGFloat AppBarHeightPortrait() {
  if (IsAppBarLabelsHidden()) {
    return kAppBarHeightFullscreen;
  }
  return kAppBarHeightDefault;
}

CGFloat AppBarHeightLandscape() {
  return kAppBarHeightLandscapeDefault;
}

NSString* const kAppBarAssistantButtonId = @"kAppBarAssistantButtonId";
NSString* const kAppBarTabGridButtonIdentifier =
    @"kAppBarTabGridButtonIdentifier";
NSString* const kAppBarNewTabButtonIdentifier =
    @"kAppBarNewTabButtonIdentifier";

const char kAppBarAssistantButtonTappedHistogram[] =
    "IOS.AppBar.AssistantButtonTapped";

const char kAppBarAssistantButtonStateOnLoadHistogram[] =
    "IOS.AppBar.AssistantButtonStateOnLoad";
