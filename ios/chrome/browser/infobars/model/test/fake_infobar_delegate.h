// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_TEST_FAKE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_TEST_FAKE_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/image_model.h"

// Fake version of InfoBarDelegate.
class FakeInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  FakeInfobarDelegate();
  FakeInfobarDelegate(std::u16string message_text);
  FakeInfobarDelegate(std::u16string title_text, std::u16string message_text);
  FakeInfobarDelegate(std::u16string title_text,
                      std::u16string message_text,
                      std::u16string button_label_text,
                      bool use_icon_background_tint,
                      ui::ImageModel icon);
  FakeInfobarDelegate(infobars::InfoBarDelegate::InfoBarIdentifier identifier);
  ~FakeInfobarDelegate() override;

  // Returns `identifier_`, set during construction.
  InfoBarIdentifier GetIdentifier() const override;

  // Returns the message string to be displayed for the Infobar.
  std::u16string GetTitleText() const override;

  // Returns the message string to be displayed for the Infobar.
  std::u16string GetMessageText() const override;

  // Returns the button label string to be displayed for the Infobar.
  std::u16string GetButtonLabel(InfoBarButton button) const override;

  // Returns true if to use icon background tint for the Infobar.
  bool UseIconBackgroundTint() const override;

  // Returns the icon for the Infobar.
  ui::ImageModel GetIcon() const override;

 private:
  FakeInfobarDelegate(infobars::InfoBarDelegate::InfoBarIdentifier identifier,
                      std::u16string title_text,
                      std::u16string message_text);
  infobars::InfoBarDelegate::InfoBarIdentifier identifier_;
  std::u16string title_text_;
  std::u16string message_text_;
  std::u16string button_label_text_;
  bool use_icon_background_tint_ = true;
  ui::ImageModel icon_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_TEST_FAKE_INFOBAR_DELEGATE_H_
