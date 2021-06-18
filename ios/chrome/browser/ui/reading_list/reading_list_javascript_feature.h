// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_JAVASCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_JAVASCRIPT_FEATURE_H_

#include "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

// A feature which receives DOM attributes and uses them to determine Time to
// Read and Distillibility.
class ReadingListJavaScriptFeature : public web::JavaScriptFeature {
 private:
  friend class base::NoDestructor<ReadingListJavaScriptFeature>;

  ReadingListJavaScriptFeature();
  ~ReadingListJavaScriptFeature() override;

  ReadingListJavaScriptFeature(const ReadingListJavaScriptFeature&) = delete;
  ReadingListJavaScriptFeature& operator=(const ReadingListJavaScriptFeature&) =
      delete;

  // JavaScriptFeature:
  absl::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  // Returns true if there has not been a presented Add to Reading List Messages
  // prompt in this browsing session.
  bool CanShowReadingListMessages();
  // Saves that an Add to Reading List Messages prompt has been presented.
  void SaveReadingListMessagesShownTime();
};

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_JAVASCRIPT_FEATURE_H_
