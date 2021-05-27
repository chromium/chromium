// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_IOS_ADD_TO_READING_LIST_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_IOS_ADD_TO_READING_LIST_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"

class ReadingListModel;

// Shows an add to Reading List prompt in iOS
class IOSAddToReadingListInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  IOSAddToReadingListInfobarDelegate(const GURL& URL,
                                     const std::u16string& title,
                                     int time_to_read,
                                     ReadingListModel* model);
  ~IOSAddToReadingListInfobarDelegate() override;

  // Returns |delegate| as an IOSAddToReadingListInfobarDelegate, or nullptr
  // if it is of another type.
  static IOSAddToReadingListInfobarDelegate* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  // Not copyable or moveable.
  IOSAddToReadingListInfobarDelegate(
      const IOSAddToReadingListInfobarDelegate&) = delete;
  IOSAddToReadingListInfobarDelegate& operator=(
      const IOSAddToReadingListInfobarDelegate&) = delete;

  const GURL& URL() const { return url_; }

  int time_to_read() { return time_to_read_; }

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;

  // ConfirmInfoBarDelegate implementation.
  bool Accept() override;

 private:
  // The URL of the page to be saved to Reading List.
  GURL url_;
  // The title of the page to be saved to Reading List.
  const std::u16string& title_;
  // The estimated time to read of the page.
  int time_to_read_;
  // Reference to save |url_| to Reading List.
  ReadingListModel* model_;
};

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_IOS_ADD_TO_READING_LIST_INFOBAR_DELEGATE_H_
