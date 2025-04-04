// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mediator.h"

#import <memory>
#import <utility>

// TODO(crbug.com/402511942): Implement SaveCardBottomSheetMediator.
@implementation SaveCardBottomSheetMediator {
  // The model layer component providing resources and callbacks for
  // saving the card or rejecting the card upload.
  // TODO:(crbug.com/402511942): Start observing the model for card upload
  // updates.
  std::unique_ptr<autofill::SaveCardBottomSheetModel> _saveCardBottomSheetModel;
}

- (instancetype)initWithUIModel:
    (std::unique_ptr<autofill::SaveCardBottomSheetModel>)model {
  self = [super init];
  if (self) {
    _saveCardBottomSheetModel = std::move(model);
  }
  return self;
}

- (void)disconnect {
  // TODO:(crbug.com/402511942): Stop observing the model
}

@end
