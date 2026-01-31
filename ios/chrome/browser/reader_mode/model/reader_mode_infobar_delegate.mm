// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_infobar_delegate.h"

#import "ios/chrome/browser/infobars/model/infobar_type.h"

ReaderModeInfobarDelegate::ReaderModeInfobarDelegate() = default;

ReaderModeInfobarDelegate::~ReaderModeInfobarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
ReaderModeInfobarDelegate::GetIdentifier() const {
  return READER_MODE_INFOBAR_DELEGATE_IOS;
}
