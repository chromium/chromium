// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/infobar_banner/confirm_infobar_banner_overlay_request_config_storage.h"

#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"
#import "ui/base/models/image_model.h"

namespace confirm_infobar_overlays {

ConfirmBannerRequestConfigStorage::ConfirmBannerRequestConfigStorage(
    infobars::InfoBar* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  ConfirmInfoBarDelegate* delegate =
      static_cast<ConfirmInfoBarDelegate*>(infobar_->delegate());
  title_text_ = delegate->GetTitleText();
  message_text_ = delegate->GetMessageText();
  button_label_text_ =
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK);
  if (!delegate->GetIcon().IsEmpty()) {
    icon_image_ = delegate->GetIcon().GetImage();
  }
  is_high_priority_ = static_cast<InfoBarIOS*>(infobar)->high_priority();
  use_icon_background_tint_ = delegate->UseIconBackgroundTint();
}

ConfirmBannerRequestConfigStorage::~ConfirmBannerRequestConfigStorage() =
    default;

}  // namespace confirm_infobar_overlays
