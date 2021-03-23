// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/test/fake_infobar_delegate.h"

#include "base/strings/utf_string_conversions.h"

FakeInfobarDelegate::FakeInfobarDelegate()
    : FakeInfobarDelegate(
          infobars::InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR,
          /*title_text=*/std::u16string(),
          u"FakeInfobarDelegate") {}

FakeInfobarDelegate::FakeInfobarDelegate(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier)
    : FakeInfobarDelegate(identifier,
                          /*title_text=*/std::u16string(),
                          u"FakeInfobarDelegate") {}

FakeInfobarDelegate::FakeInfobarDelegate(std::u16string message_text)
    : FakeInfobarDelegate(
          infobars::InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR,
          /*title_text=*/std::u16string(),
          std::move(message_text)) {}

FakeInfobarDelegate::FakeInfobarDelegate(std::u16string title_text,
                                         std::u16string message_text)
    : FakeInfobarDelegate(
          infobars::InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR,
          std::move(title_text),
          std::move(message_text)) {}

FakeInfobarDelegate::FakeInfobarDelegate(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    std::u16string title_text,
    std::u16string message_text)
    : identifier_(identifier),
      title_text_(std::move(title_text)),
      message_text_(std::move(message_text)) {}

FakeInfobarDelegate::~FakeInfobarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
FakeInfobarDelegate::GetIdentifier() const {
  return identifier_;
}

// Returns the title string to be displayed for the Infobar.
std::u16string FakeInfobarDelegate::GetTitleText() const {
  return title_text_;
}

// Returns the message string to be displayed for the Infobar.
std::u16string FakeInfobarDelegate::GetMessageText() const {
  return message_text_;
}
