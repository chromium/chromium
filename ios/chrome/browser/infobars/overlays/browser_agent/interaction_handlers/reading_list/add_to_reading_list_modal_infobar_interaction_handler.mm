// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/reading_list/add_to_reading_list_modal_infobar_interaction_handler.h"

#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/reading_list/add_to_reading_list_infobar_modal_overlay_request_callback_installer.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/reading_list/ios_add_to_reading_list_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using reading_list_infobar_overlay::ModalRequestCallbackInstaller;

ReadingListInfobarModalInteractionHandler::
    ReadingListInfobarModalInteractionHandler(Browser* browser)
    : browser_(browser) {}

ReadingListInfobarModalInteractionHandler::
    ~ReadingListInfobarModalInteractionHandler() = default;

#pragma mark - Public

void ReadingListInfobarModalInteractionHandler::NeverAsk(InfoBarIOS* infobar) {
  IOSAddToReadingListInfobarDelegate* delegate = GetDelegate(infobar);
  delegate->NeverShow();

  // Remove infobar.
  infobar->RemoveSelf();
}

#pragma mark - InfobarModalInteractionHandler

void ReadingListInfobarModalInteractionHandler::PerformMainAction(
    InfoBarIOS* infobar) {
  IOSAddToReadingListInfobarDelegate* delegate = GetDelegate(infobar);
  infobar->set_accepted(delegate->Accept());
  [static_cast<id<BrowserCommands>>(browser_->GetCommandDispatcher())
      showReadingListIPH];
}

#pragma mark - Private

std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>
ReadingListInfobarModalInteractionHandler::CreateModalInstaller() {
  return std::make_unique<ModalRequestCallbackInstaller>(this);
}

IOSAddToReadingListInfobarDelegate*
ReadingListInfobarModalInteractionHandler::GetDelegate(InfoBarIOS* infobar) {
  IOSAddToReadingListInfobarDelegate* delegate =
      IOSAddToReadingListInfobarDelegate::FromInfobarDelegate(
          infobar->delegate());
  DCHECK(delegate);
  return delegate;
}
