// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/context_menu/reading_list_context_menu_coordinator.h"

#import "base/ios/ios_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/reading_list/context_menu/reading_list_context_menu_delegate.h"
#import "ios/chrome/browser/ui/reading_list/context_menu/reading_list_context_menu_params.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Action chosen by the user in the context menu, for UMA report.
// These match tools/metrics/histograms/histograms.xml.
enum UMAContextMenuAction {
  // The user opened the entry in a new tab.
  NEW_TAB = 0,
  // The user opened the entry in a new incognito tab.
  NEW_INCOGNITO_TAB = 1,
  // The user copied the url of the entry.
  COPY_LINK = 2,
  // The user chose to view the offline version of the entry.
  VIEW_OFFLINE = 3,
  // The user cancelled the context menu.
  CANCEL = 4,
  // Add new enum above ENUM_MAX.
  ENUM_MAX
};
}  // namespace

@interface ReadingListContextMenuCoordinator ()

// Whether the coordinator has been started.
@property(nonatomic, assign, getter=isStarted) BOOL started;

@end

@implementation ReadingListContextMenuCoordinator
@synthesize delegate = _delegate;
@synthesize params = _params;
@synthesize started = _started;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:
                                        (ReadingListContextMenuParams*)params {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                                     title:params.title
                                   message:params.message
                                      rect:params.rect
                                      view:params.view];
  if (self) {
    DCHECK(params);
    _params = params;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;

  __weak id<ReadingListContextMenuDelegate> weakDelegate = self.delegate;
  __weak ReadingListContextMenuParams* weakParams = self.params;

  // Add "Open In New Tab" option.
  NSString* openInNewTabTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  [self addItemWithTitle:openInNewTabTitle
                  action:^{
                    [weakDelegate
                        openURLInNewTabForContextMenuWithParams:weakParams];
                    UMA_HISTOGRAM_ENUMERATION("ReadingList.ContextMenu",
                                              NEW_TAB, ENUM_MAX);
                  }
                   style:UIAlertActionStyleDefault];

  if (base::ios::IsMultipleScenesSupported()) {
    // Add "Open In New Window" option.
    NSString* openInNewWindowTitle =
        l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW);
    [self
        addItemWithTitle:openInNewWindowTitle
                  action:^{
                    [weakDelegate
                        openURLInNewWindowForContextMenuWithParams:weakParams];
                  }
                   style:UIAlertActionStyleDefault];
  }

  // Add "Open In New Incognito Tab" option;
  NSString* openInNewTabIncognitoTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
  [self addItemWithTitle:openInNewTabIncognitoTitle
                  action:^{
                    [weakDelegate
                        openURLInNewIncognitoTabForContextMenuWithParams:
                            weakParams];
                    UMA_HISTOGRAM_ENUMERATION("ReadingList.ContextMenu",
                                              NEW_INCOGNITO_TAB, ENUM_MAX);
                  }
                   style:UIAlertActionStyleDefault];

  // Add "Copy Link URL" option.
  NSString* copyLinkTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_COPY);
  [self addItemWithTitle:copyLinkTitle
                  action:^{
                    [weakDelegate copyURLForContextMenuWithParams:weakParams];
                    UMA_HISTOGRAM_ENUMERATION("ReadingList.ContextMenu",
                                              COPY_LINK, ENUM_MAX);
                  }
                   style:UIAlertActionStyleDefault];

  // Add "View Offline Version In New Tab" option if there is an offline URL.
  if (self.params.offlineURL.is_valid()) {
    NSString* viewOfflineVersionTitle =
        l10n_util::GetNSString(IDS_IOS_READING_LIST_CONTENT_CONTEXT_OFFLINE);
    [self addItemWithTitle:viewOfflineVersionTitle
                    action:^{
                      [weakDelegate
                          openOfflineURLInNewTabForContextMenuWithParams:
                              weakParams];
                      UMA_HISTOGRAM_ENUMERATION("ReadingList.ContextMenu",
                                                VIEW_OFFLINE, ENUM_MAX);
                    }
                     style:UIAlertActionStyleDefault];
  }

  // Add "Cancel" option.
  [self addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                  action:^{
                    UMA_HISTOGRAM_ENUMERATION("ReadingList.ContextMenu", CANCEL,
                                              ENUM_MAX);
                  }
                   style:UIAlertActionStyleCancel];

  [super start];
  self.started = YES;
}

@end
