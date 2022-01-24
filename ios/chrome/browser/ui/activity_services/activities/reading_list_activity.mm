// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/reading_list_activity.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kReadingListActivityType =
    @"com.google.chrome.readingListActivity";

}  // namespace

@interface ReadingListActivity () {
  GURL _activityURL;
  NSString* _title;
}

@property(nonatomic, weak, readonly) id<BrowserCommands> dispatcher;

@end

@implementation ReadingListActivity

@synthesize dispatcher = _dispatcher;

- (instancetype)initWithURL:(const GURL&)activityURL
                      title:(NSString*)title
                 dispatcher:(id<BrowserCommands>)dispatcher {
  if (self = [super init]) {
    _dispatcher = dispatcher;
    _activityURL = activityURL;
    _title = [NSString stringWithString:title];
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kReadingListActivityType;
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_READING_LIST_ACTION);
}

- (UIImage*)activityImage {
  return [UIImage imageNamed:@"activity_services_read_later"];
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return YES;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self activityDidFinish:YES];
  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURL:_activityURL title:_title];
  [_dispatcher addToReadingList:command];
}

@end
