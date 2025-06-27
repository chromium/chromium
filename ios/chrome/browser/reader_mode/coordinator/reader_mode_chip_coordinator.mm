// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_chip_coordinator.h"

#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_view_controller.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_visibility_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_chip_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_options_commands.h"

@implementation ReaderModeChipCoordinator

- (void)start {
  _viewController = [[ReaderModeChipViewController alloc] init];
  [self.baseViewController addChildViewController:_viewController];
  [self.viewController didMoveToParentViewController:_viewController];
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(ReaderModeChipCommands)];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [_viewController willMoveToParentViewController:nil];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

#pragma mark - ReaderModeChipCommands

- (void)showReaderModeChip {
  [self.visibilityDelegate readerModeChipCoordinator:self
                          didSetReaderModeChipHidden:NO];
  _viewController.readerModeOptionsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ReaderModeOptionsCommands);
}

- (void)hideReaderModeChip {
  _viewController.readerModeOptionsHandler = nil;
  [self.visibilityDelegate readerModeChipCoordinator:self
                          didSetReaderModeChipHidden:YES];
}

@end
