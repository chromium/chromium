// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_chip_coordinator.h"

#import <memory>

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_view_controller.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_visibility_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_chip_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_options_commands.h"

@implementation ReaderModeChipCoordinator {
  // Observer that updates ReaderModeChipViewController for fullscreen events.
  std::unique_ptr<FullscreenUIUpdater> _readerModeChipFullscreenUIUpdater;
  // Handler for Reading Mode options IPH.
  id<HelpCommands> _helpCommandsHandler;
}

- (void)start {
  _viewController = [[ReaderModeChipViewController alloc] init];
  [self.baseViewController addChildViewController:_viewController];
  [self.viewController didMoveToParentViewController:_viewController];
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(ReaderModeChipCommands)];
  _readerModeChipFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      FullscreenController::FromBrowser(self.browser), self.viewController);
  _helpCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), HelpCommands);
}

- (void)stop {
  _readerModeChipFullscreenUIUpdater = nullptr;
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [_viewController willMoveToParentViewController:nil];
  [_viewController removeFromParentViewController];
  _viewController = nil;
  _helpCommandsHandler = nil;
}

#pragma mark - ReaderModeChipCommands

- (void)showReaderModeChip {
  [self.visibilityDelegate readerModeChipCoordinator:self
                          didSetReaderModeChipHidden:NO];
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  if ([dispatcher
          dispatchingForProtocol:@protocol(ReaderModeOptionsCommands)]) {
    _viewController.readerModeOptionsHandler =
        HandlerForProtocol(dispatcher, ReaderModeOptionsCommands);
  }

  [_helpCommandsHandler
      presentInProductHelpWithType:InProductHelpType::kReaderModeOptions];
}

- (void)hideReaderModeChip {
  _viewController.readerModeOptionsHandler = nil;
  [self.visibilityDelegate readerModeChipCoordinator:self
                          didSetReaderModeChipHidden:YES];
}

@end
