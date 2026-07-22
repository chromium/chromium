// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_ai_prefetch_failure_infobar_delegate_ios.h"

#import "components/infobars/core/infobar_delegate.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image.h"

namespace {

// Point size of the Autofill AI infobar icon.
constexpr CGFloat kAutofillAiInfobarSymbolPointSize = 24.0;

// Returns the image model for the prefetch failure infobar icon.
ui::ImageModel GetPrefetchFailureIcon() {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  UIImage* image = MakeSymbolMulticolor(CustomSymbolWithPointSize(
      kMulticolorChromeballSymbol, kAutofillAiInfobarSymbolPointSize));
#else
  UIImage* image = CustomSymbolWithPointSize(kChromeProductSymbol,
                                             kAutofillAiInfobarSymbolPointSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return image ? ui::ImageModel::FromImage(gfx::Image(image))
               : ui::ImageModel();
}
}  // namespace

namespace autofill {

AutofillAiPrefetchFailureInfoBarDelegateIOS::
    AutofillAiPrefetchFailureInfoBarDelegateIOS()
    : icon_(GetPrefetchFailureIcon()) {}

AutofillAiPrefetchFailureInfoBarDelegateIOS::
    ~AutofillAiPrefetchFailureInfoBarDelegateIOS() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
AutofillAiPrefetchFailureInfoBarDelegateIOS::GetIdentifier() const {
  return infobars::InfoBarDelegate::
      AUTOFILL_AI_PRE_FETCH_FAILURE_INFOBAR_DELEGATE_IOS;
}

ui::ImageModel AutofillAiPrefetchFailureInfoBarDelegateIOS::GetIcon() const {
  return icon_;
}

std::u16string AutofillAiPrefetchFailureInfoBarDelegateIOS::GetMessageText()
    const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_PRE_FETCH_ERROR_MESSAGE);
}

int AutofillAiPrefetchFailureInfoBarDelegateIOS::GetButtons() const {
  return BUTTON_OK;
}

std::u16string AutofillAiPrefetchFailureInfoBarDelegateIOS::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(button, BUTTON_OK);
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AI_PRE_FETCH_ERROR_MESSAGE_BUTTON_TEXT);
}

bool AutofillAiPrefetchFailureInfoBarDelegateIOS::IgnoreIconColorWithTint()
    const {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return false;
#else
  return true;
#endif
}

}  // namespace autofill
