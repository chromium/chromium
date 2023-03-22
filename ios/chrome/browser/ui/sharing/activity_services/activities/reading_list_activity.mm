// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/reading_list_activity.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

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
    _title = [title copy];
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
  return DefaultSymbolWithPointSize(kReadLaterActionSymbol,
                                    kSymbolActionPointSize);
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return YES;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self activityDidFinish:YES];
  // Reading list does not support not having title, so add host instead.
  NSString* title =
      _title ? _title : base::SysUTF8ToNSString(_activityURL.host());
  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURL:_activityURL title:title];
  [_dispatcher addToReadingList:command];
}

@end
