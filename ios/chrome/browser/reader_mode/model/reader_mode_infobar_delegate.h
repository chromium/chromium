// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_INFOBAR_DELEGATE_H_

#import "components/infobars/core/infobar_delegate.h"

// Delegate for the Reader Mode infobar.
class ReaderModeInfobarDelegate : public infobars::InfoBarDelegate {
 public:
  ReaderModeInfobarDelegate();
  ~ReaderModeInfobarDelegate() override;

  // infobars::InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_INFOBAR_DELEGATE_H_
