// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_IOS_ADD_TO_READING_LIST_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_IOS_ADD_TO_READING_LIST_INFOBAR_DELEGATE_H_

#import "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace web {
class WebState;
}

class ReadingListModel;

// Shows an add to Reading List prompt in iOS
class IOSAddToReadingListInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  IOSAddToReadingListInfobarDelegate(const GURL& URL,
                                     const std::u16string& title,
                                     int estimated_read_time_,
                                     double score,
                                     double long_score,
                                     ReadingListModel* model,
                                     web::WebState* web_state);
  ~IOSAddToReadingListInfobarDelegate() override;

  // Returns `delegate` as an IOSAddToReadingListInfobarDelegate, or nullptr
  // if it is of another type.
  static IOSAddToReadingListInfobarDelegate* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  // Not copyable or moveable.
  IOSAddToReadingListInfobarDelegate(
      const IOSAddToReadingListInfobarDelegate&) = delete;
  IOSAddToReadingListInfobarDelegate& operator=(
      const IOSAddToReadingListInfobarDelegate&) = delete;

  const GURL& URL() const { return url_; }

  int estimated_read_time() { return estimated_read_time_; }

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  void InfoBarDismissed() override;

  // ConfirmInfoBarDelegate implementation.
  bool Accept() override;

  // If called, sets the pref to never show the Reading List Message.
  virtual void NeverShow();

 private:
  // The URL of the page to be saved to Reading List.
  GURL url_;
  // The title of the page to be saved to Reading List.
  const raw_ref<const std::u16string> title_;
  // The estimated time to read of the page.
  int estimated_read_time_;
  // The score of the page measuring distilibility, a proxy for whether the
  // page is likely an article.
  double distilibility_score_;
  // The score of the page measuring length of the page.
  double length_score_;
  // Reference to save `url_` to Reading List.
  raw_ptr<ReadingListModel> model_ = nullptr;
  // WebState pointer that is showing `url_`.
  raw_ptr<web::WebState> web_state_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_IOS_ADD_TO_READING_LIST_INFOBAR_DELEGATE_H_
