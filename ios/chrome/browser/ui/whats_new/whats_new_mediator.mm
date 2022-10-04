// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_mediator.h"

#import "base/notreached.h"
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WhatsNewMediator

#pragma mark - Public

- (NSArray<WhatsNewItem*>*)whatsNewChromeTipEntries {
  return WhatsNewChromeTipEntries(WhatsNewFilePath());
}

- (NSArray<WhatsNewItem*>*)whatsNewFeatureEntries {
  return WhatsNewFeatureEntries(WhatsNewFilePath());
}

#pragma mark - WhatsNewPrimaryActionHandler

- (void)didTapActionButton:(WhatsNewItem*)item {
  if (!item.hasPrimaryAction) {
    return;
  }

  switch (item.type) {
    case WhatsNewType::kAddPasswordManually:
    case WhatsNewType::kUseChromeByDefault:
    case WhatsNewType::kPasswordsInOtherApps:
      [self openSettingsURLString];
      break;
    default:
      NOTREACHED();
      break;
  };
}

#pragma mark Private

- (void)openSettingsURLString {
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
}

@end
