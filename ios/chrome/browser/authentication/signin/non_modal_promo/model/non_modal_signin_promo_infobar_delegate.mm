// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/signin/non_modal_promo/model/non_modal_signin_promo_infobar_delegate.h"

#import <UIKit/UIKit.h>

#import <utility>

#import "base/check_op.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"
#import "ui/gfx/image/image.h"

namespace {

// The point size of the promo icon.
constexpr CGFloat kIconPointSize = 22;

}  // namespace

NonModalSignInPromoInfoBarDelegate::NonModalSignInPromoInfoBarDelegate(
    std::u16string message_text,
    base::OnceClosure accept_callback,
    base::OnceClosure destroyed_callback)
    : message_text_(std::move(message_text)),
      accept_callback_(std::move(accept_callback)),
      destroyed_callback_(std::move(destroyed_callback)) {}

NonModalSignInPromoInfoBarDelegate::~NonModalSignInPromoInfoBarDelegate() {
  if (destroyed_callback_) {
    std::move(destroyed_callback_).Run();
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
NonModalSignInPromoInfoBarDelegate::GetIdentifier() const {
  return NON_MODAL_SIGNIN_PROMO_INFOBAR_DELEGATE_IOS;
}

std::u16string NonModalSignInPromoInfoBarDelegate::GetTitleText() const {
  return l10n_util::GetStringUTF16(IDS_IOS_NON_MODAL_SIGNIN_PROMO_PROMPT);
}

std::u16string NonModalSignInPromoInfoBarDelegate::GetMessageText() const {
  return message_text_;
}

int NonModalSignInPromoInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string NonModalSignInPromoInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  CHECK_EQ(button, BUTTON_OK);
  return l10n_util::GetStringUTF16(
      IDS_IOS_NON_MODAL_SIGNIN_PROMO_SIGNIN_BUTTON);
}

ui::ImageModel NonModalSignInPromoInfoBarDelegate::GetIcon() const {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  UIImage* icon = MakeSymbolMulticolor(
      SymbolWithPointSize(SymbolMulticolorChromeball, kIconPointSize));
#else
  UIImage* icon = SymbolWithPointSize(SymbolChromeProduct, kIconPointSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return ui::ImageModel::FromImage(gfx::Image(icon));
}

bool NonModalSignInPromoInfoBarDelegate::UseIconBackgroundTint() const {
  // The Chrome logo is shown as is.
  return false;
}

bool NonModalSignInPromoInfoBarDelegate::Accept() {
  if (accept_callback_) {
    std::move(accept_callback_).Run();
  }
  return true;
}

bool NonModalSignInPromoInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // The promo isn't related to the page content, so it stays until it is
  // dismissed.
  return false;
}
