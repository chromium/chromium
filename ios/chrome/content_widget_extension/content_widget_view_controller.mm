// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/content_widget_extension/content_widget_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/common/app_group/app_group_command.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/ntp_tile/ntp_tile.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/content_widget_extension/content_widget_view.h"
#import "ios/chrome/content_widget_extension/most_visited_tile_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Using GURL in the extension is not wanted as it includes ICU which makes the
// extension binary much larger; therefore, ios/chrome/common/x_callback_url.h
// cannot be used. This class makes a very basic use of x-callback-url, so no
// full implementation is required.
NSString* const kXCallbackURLHost = @"x-callback-url";
}  // namespace

@interface ContentWidgetViewController ()<ContentWidgetViewDelegate>
@property(nonatomic, strong) NSDictionary<NSURL*, NTPTile*>* sites;
@property(nonatomic, weak) ContentWidgetView* widgetView;
@property(nonatomic, readonly) BOOL isCompact;

// Updates the widget with latest data. Returns whether any visual updates
// occurred.
- (BOOL)updateWidget;
// Expand the widget.
- (void)setExpanded:(CGSize)maxSize;
// Register a display of the widget in the app_group NSUserDefaults.
// Metrics on the widget usage will be sent (if enabled) on the next Chrome
// startup.
- (void)registerWidgetDisplay;
@end

@implementation ContentWidgetViewController

@synthesize sites = _sites;
@synthesize widgetView = _widgetView;

#pragma mark - properties

- (BOOL)isCompact {
  DCHECK(self.extensionContext);
  return [self.extensionContext widgetActiveDisplayMode] ==
         NCWidgetDisplayModeCompact;
  return NO;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  DCHECK(self.extensionContext);
  CGFloat height =
      [self.extensionContext
          widgetMaximumSizeForDisplayMode:NCWidgetDisplayModeCompact]
          .height;

  CGFloat width =
      [self.extensionContext
          widgetMaximumSizeForDisplayMode:NCWidgetDisplayModeCompact]
          .width;

  // A local variable is necessary here as the property is declared weak and the
  // object would be deallocated before being retained by the addSubview call.
  ContentWidgetView* widgetView =
      [[ContentWidgetView alloc] initWithDelegate:self
                                    compactHeight:height
                                            width:width];
  self.widgetView = widgetView;
  [self.view addSubview:self.widgetView];

  self.extensionContext.widgetLargestAvailableDisplayMode =
      NCWidgetDisplayModeCompact;

  self.widgetView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.widgetView, self.view);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self registerWidgetDisplay];

  // |widgetActiveDisplayMode| does not contain a valid value in viewDidLoad. By
  // the time viewWillAppear is called, it is correct, so set the mode here.
  [self.widgetView showMode:self.isCompact];
}

- (void)widgetPerformUpdateWithCompletionHandler:
    (void (^)(NCUpdateResult))completionHandler {
  completionHandler([self updateWidget] ? NCUpdateResultNewData
                                        : NCUpdateResultNoData);
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> _Nonnull context) {
        [self.widgetView showMode:self.isCompact];
        [self.widgetView layoutIfNeeded];
      }
                      completion:nil];
}

#pragma mark - NCWidgetProviding

- (void)widgetActiveDisplayModeDidChange:(NCWidgetDisplayMode)activeDisplayMode
                         withMaximumSize:(CGSize)maxSize
    API_AVAILABLE(ios(10.0)) {
  switch (activeDisplayMode) {
    case NCWidgetDisplayModeCompact:
      self.preferredContentSize = maxSize;
      break;
    case NCWidgetDisplayModeExpanded:
      [self setExpanded:maxSize];
      break;
  }
}

#pragma mark - internal

- (void)setExpanded:(CGSize)maxSize {
  [self updateWidget];
  self.preferredContentSize =
      CGSizeMake(maxSize.width,
                 MIN([self.widgetView widgetExpandedHeight], maxSize.height));
}

- (BOOL)updateWidget {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSData* data = [sharedDefaults objectForKey:app_group::kSuggestedItems];
  NSError* error = nil;
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];

  if (!unarchiver || error) {
    DLOG(WARNING) << "Error creating unarchiver for widget extension: "
                  << base::SysNSStringToUTF8([error description]);
    return NO;
  }

  unarchiver.requiresSecureCoding = NO;

  NSDictionary<NSURL*, NTPTile*>* newSites =
      [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
  if ([newSites isEqualToDictionary:self.sites]) {
    return NO;
  }
  self.sites = newSites;
  [self.widgetView updateSites:self.sites];
  self.extensionContext.widgetLargestAvailableDisplayMode =
      [self.widgetView sitesFitSingleRow] ? NCWidgetDisplayModeCompact
                                          : NCWidgetDisplayModeExpanded;
  return YES;
}

- (void)registerWidgetDisplay {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSInteger numberOfDisplay =
      [sharedDefaults integerForKey:app_group::kContentExtensionDisplayCount];
  [sharedDefaults setInteger:numberOfDisplay + 1
                      forKey:app_group::kContentExtensionDisplayCount];
}

- (void)openURL:(NSURL*)URL {
  AppGroupCommand* command = [[AppGroupCommand alloc]
      initWithSourceApp:app_group::kOpenCommandSourceContentExtension
         URLOpenerBlock:^(NSURL* openURL) {
           [self.extensionContext openURL:openURL completionHandler:nil];
         }];
  [command prepareToOpenURL:URL];
  [command executeInApp];
}

@end
