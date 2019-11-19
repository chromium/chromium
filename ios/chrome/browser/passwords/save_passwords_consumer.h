// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_SAVE_PASSWORDS_CONSUMER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_SAVE_PASSWORDS_CONSUMER_H_

#include <memory>
#include <vector>

#include "components/password_manager/core/browser/password_store_consumer.h"

@protocol SavePasswordsConsumerDelegate

// Callback called when the async request launched from
// |getLoginsFromPasswordStore| finishes.
- (void)onGetPasswordStoreResults:
    (std::vector<std::unique_ptr<autofill::PasswordForm>>)results;

@end

namespace ios {
// A bridge C++ class passing notification about finished password store
// requests to the Obj-C delegate.
class SavePasswordsConsumer : public password_manager::PasswordStoreConsumer {
 public:
  explicit SavePasswordsConsumer(id<SavePasswordsConsumerDelegate> delegate);
  ~SavePasswordsConsumer() override;
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

 private:
  __weak id<SavePasswordsConsumerDelegate> delegate_ = nil;
  DISALLOW_COPY_AND_ASSIGN(SavePasswordsConsumer);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_STORE_FACTORY_H_
