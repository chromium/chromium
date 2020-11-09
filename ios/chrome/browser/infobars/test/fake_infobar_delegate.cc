// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/test/fake_infobar_delegate.h"

#include "base/strings/utf_string_conversions.h"

FakeInfobarDelegate::FakeInfobarDelegate()
    : FakeInfobarDelegate(
          infobars::InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR,
          /*title_text=*/base::string16(),
          base::ASCIIToUTF16("FakeInfobarDelegate")) {}

FakeInfobarDelegate::FakeInfobarDelegate(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier)
    : FakeInfobarDelegate(identifier,
                          /*title_text=*/base::string16(),
                          base::ASCIIToUTF16("FakeInfobarDelegate")) {}

FakeInfobarDelegate::FakeInfobarDelegate(base::string16 message_text)
    : FakeInfobarDelegate(
          infobars::InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR,
          /*title_text=*/base::string16(),
          std::move(message_text)) {}

FakeInfobarDelegate::FakeInfobarDelegate(base::string16 title_text,
                                         base::string16 message_text)
    : FakeInfobarDelegate(
          infobars::InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR,
          std::move(title_text),
          std::move(message_text)) {}

FakeInfobarDelegate::FakeInfobarDelegate(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    base::string16 title_text,
    base::string16 message_text)
    : identifier_(identifier),
      title_text_(std::move(title_text)),
      message_text_(std::move(message_text)) {}

FakeInfobarDelegate::~FakeInfobarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
FakeInfobarDelegate::GetIdentifier() const {
  return identifier_;
}

// Returns the title string to be displayed for the Infobar.
base::string16 FakeInfobarDelegate::GetTitleText() const {
  return title_text_;
}

// Returns the message string to be displayed for the Infobar.
base::string16 FakeInfobarDelegate::GetMessageText() const {
  return message_text_;
}
