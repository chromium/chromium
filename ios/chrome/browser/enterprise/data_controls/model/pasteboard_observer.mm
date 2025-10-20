// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/pasteboard_observer.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/functional/callback.h"
#import "components/open_from_clipboard/clipboard_async_wrapper_ios.h"

@implementation PasteboardObserver {
  base::RepeatingCallback<void(UIPasteboard*)> _callback;
  NSInteger _cachedChangeCount;
}

- (instancetype)initWithCallback:
    (base::RepeatingCallback<void(UIPasteboard*)>)callback {
  self = [super init];
  if (self) {
    _callback = std::move(callback);
    _cachedChangeCount = -1;

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(pasteboardChanged:)
               name:UIPasteboardChangedNotification
             object:nil];
    // Monitor when the app goes to and from the background to detect changes in
    // the pasteboard outside of the app.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didBecomeActive:)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(willResignActive:)
               name:UIApplicationWillResignActiveNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)pasteboardChanged:(NSNotification*)notification {
  [self updateFromGeneralPasteboard];
}

- (void)didBecomeActive:(NSNotification*)notification {
  // Check for pasteboard updates outside of the app.
  [self updateFromGeneralPasteboard];
}

- (void)willResignActive:(NSNotification*)notification {
  // Update change count before the app becomes inactive to catch
  // possible pasteboard updates outside of the app.
  __weak __typeof(self) weakSelf = self;
  GetGeneralPasteboard(/* asynchronous= */ true,
                       base::BindOnce(^(UIPasteboard* pasteboard) {
                         [weakSelf updateChangeCountFromPasteboard:pasteboard];
                       }));
}

- (void)updateChangeCountFromPasteboard:(UIPasteboard*)pasteboard {
  _cachedChangeCount = pasteboard.changeCount;
}

- (void)updateFromGeneralPasteboard {
  __weak __typeof(self) weakSelf = self;
  GetGeneralPasteboard(/* asynchronous= */ true,
                       base::BindOnce(^(UIPasteboard* pasteboard) {
                         [weakSelf updateIfNeededWithPasteboard:pasteboard];
                       }));
}

- (void)updateIfNeededWithPasteboard:(UIPasteboard*)pasteboard {
  NSInteger changeCount = pasteboard.changeCount;
  if (changeCount == _cachedChangeCount) {
    return;
  }

  _cachedChangeCount = changeCount;
  _callback.Run(pasteboard);
}

@end
