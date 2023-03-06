// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/reading_list_spotlight_manager.h"

#import <CoreSpotlight/CoreSpotlight.h>
#import <memory>

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Called from the BrowserBookmarkModelBridge from C++ -> ObjC.
@interface ReadingListSpotlightManager () <ReadingListModelBridgeObserver> {
  // Bridge to register for reading list changes.
  std::unique_ptr<ReadingListModelBridge> _modelBridge;
}

@end

@implementation ReadingListSpotlightManager

+ (ReadingListSpotlightManager*)readingListSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState {
  return [[ReadingListSpotlightManager alloc]
      initWithLargeIconService:IOSChromeLargeIconServiceFactory::
                                   GetForBrowserState(browserState)
              readingListModel:ReadingListModelFactory::GetInstance()
                                   ->GetForBrowserState(browserState)];
}

- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                        readingListModel:(ReadingListModel*)model {
  self = [super initWithLargeIconService:largeIconService
                                  domain:spotlight::DOMAIN_READING_LIST];
  if (self) {
    _model = model;
    _modelBridge.reset(new ReadingListModelBridge(self, model));
  }
  return self;
}

- (void)detachModel {
  _modelBridge.reset();
  _model = nil;
}

- (void)shutdown {
  [self detachModel];
  [super shutdown];
}

- (void)clearAndReindexReadingListWithCompletionBlock:
    (void (^)(NSError* error))completionHandler {
  if (!self.model || !self.model->loaded()) {
    completionHandler(
        [ReadingListSpotlightManager modelNotReadyOrShutDownError]);
    return;
  }

  __weak ReadingListSpotlightManager* weakSelf = self;
  [self clearAllSpotlightItems:^(NSError* error) {
    if (error) {
      if (completionHandler) {
        completionHandler(error);
      }
      return;
    }
    [weakSelf indexAllReadingListItemsWithCompletionBlock:completionHandler];
  }];
}

- (void)indexAllReadingListItemsWithCompletionBlock:
    (void (^)(NSError* error))completionHandler {
  if (!self.model || !self.model->loaded()) {
    completionHandler(
        [ReadingListSpotlightManager modelNotReadyOrShutDownError]);
    return;
  }

  for (const auto& url : self.model->GetKeys()) {
    const ReadingListEntry* entry = self.model->GetEntryByURL(url).get();
    DCHECK(entry);
    NSString* title = base::SysUTF8ToNSString(entry->Title());
    [self refreshItemsWithURL:entry->URL() title:title];
  }

  if (completionHandler) {
    completionHandler(nil);
  }
}

+ (NSError*)modelNotReadyOrShutDownError {
  return [NSError
      errorWithDomain:@"chrome"
                 code:0
             userInfo:@{
               NSLocalizedDescriptionKey :
                   @"Reading list model isn't initialized or already shut down"
             }];
}

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  [self clearAndReindexReadingListWithCompletionBlock:nil];
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
}

@end
