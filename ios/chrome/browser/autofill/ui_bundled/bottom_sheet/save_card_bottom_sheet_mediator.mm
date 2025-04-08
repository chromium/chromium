// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mediator.h"

#import <memory>
#import <utility>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

// TODO(crbug.com/402511942): Implement SaveCardBottomSheetMediator.
@implementation SaveCardBottomSheetMediator {
  // The model layer component providing resources and callbacks for
  // saving the card or rejecting the card upload.
  // TODO:(crbug.com/402511942): Start observing the model for card upload
  // updates.
  std::unique_ptr<autofill::SaveCardBottomSheetModel> _saveCardBottomSheetModel;
  __weak id<AutofillCommands> _autofillCommandsHandler;
}

- (instancetype)initWithUIModel:
                    (std::unique_ptr<autofill::SaveCardBottomSheetModel>)model
        autofillCommandsHandler:(id<AutofillCommands>)autofillCommandsHandler {
  self = [super init];
  if (self) {
    _saveCardBottomSheetModel = std::move(model);
    _autofillCommandsHandler = autofillCommandsHandler;
  }
  return self;
}

- (void)disconnect {
  // TODO:(crbug.com/402511942): Stop observing the model
}

- (void)setConsumer:(id<SaveCardBottomSheetConsumer>)consumer {
  _consumer = consumer;
  [self.consumer
      setAboveTitleImage:NativeImage(
                             _saveCardBottomSheetModel->logo_icon_id())];
  [self.consumer
      setAboveTitleImageDescription:base::SysUTF16ToNSString(
                                        _saveCardBottomSheetModel
                                            ->logo_icon_description())];
  [self.consumer
      setTitle:base::SysUTF16ToNSString(_saveCardBottomSheetModel->title())];
  [self.consumer setSubtitle:base::SysUTF16ToNSString(
                                 _saveCardBottomSheetModel->subtitle())];
  [self.consumer
      setAcceptActionText:base::SysUTF16ToNSString(
                              _saveCardBottomSheetModel->accept_button_text())];
  [self.consumer
      setCancelActionText:base::SysUTF16ToNSString(
                              _saveCardBottomSheetModel->cancel_button_text())];
}

#pragma mark - SaveCardBottomSheetMutator

- (void)didAccept {
  // TODO(crbug.com/407776335): Show loading state when accept button is pushed.
  _saveCardBottomSheetModel->OnAccepted();
}

- (void)didCancel {
  _saveCardBottomSheetModel->OnCanceled();
  [_autofillCommandsHandler dismissSaveCardBottomSheet];
}

@end
