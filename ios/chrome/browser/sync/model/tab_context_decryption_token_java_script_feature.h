// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_TAB_CONTEXT_DECRYPTION_TOKEN_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_TAB_CONTEXT_DECRYPTION_TOKEN_JAVA_SCRIPT_FEATURE_H_

#import <optional>
#import <string>

#import "ios/web/public/js_messaging/java_script_feature.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

// Feature that injects the `chrome.getContainerDecryptionToken` JavaScript API
// for the allowed tab context origin (`https://chromestorage.goog`).
class TabContextDecryptionTokenJavaScriptFeature
    : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static TabContextDecryptionTokenJavaScriptFeature* GetInstance();

  TabContextDecryptionTokenJavaScriptFeature(
      const TabContextDecryptionTokenJavaScriptFeature&) = delete;
  TabContextDecryptionTokenJavaScriptFeature& operator=(
      const TabContextDecryptionTokenJavaScriptFeature&) = delete;

 private:
  friend class base::NoDestructor<TabContextDecryptionTokenJavaScriptFeature>;
  friend class TabContextDecryptionTokenJavaScriptFeatureTest;

  TabContextDecryptionTokenJavaScriptFeature();
  ~TabContextDecryptionTokenJavaScriptFeature() override;

  // web::JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  bool GetFeatureRepliesToMessages() const override;
  void ScriptMessageReceivedWithReply(
      web::WebState* web_state,
      const web::ScriptMessage& message,
      ScriptMessageReplyCallback callback) override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_TAB_CONTEXT_DECRYPTION_TOKEN_JAVA_SCRIPT_FEATURE_H_
