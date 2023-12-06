// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/infobar_utils.h"

#import <memory>
#import <utility>

#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"

std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  return std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeConfirm,
                                      std::move(delegate));
}

std::unique_ptr<infobars::InfoBar> CreateHighPriorityConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeConfirm, std::move(delegate));
  infobar->set_high_priority(true);
  return infobar;
}
