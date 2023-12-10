// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_SAVE_PASSWORDS_CONSUMER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_SAVE_PASSWORDS_CONSUMER_H_

#include <CoreFoundation/CoreFoundation.h>

#include <memory>
#include <vector>

#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

@protocol SavePasswordsConsumerDelegate

// Callback called when the async request launched from
// `getLoginsFromPasswordStore` finishes.
- (void)
    onGetPasswordStoreResults:
        (std::vector<std::unique_ptr<password_manager::PasswordForm>>)results
                    fromStore:(password_manager::PasswordStoreInterface*)store;

@end

namespace ios {
// A bridge C++ class passing notification about finished password store
// requests to the Obj-C delegate.
class SavePasswordsConsumer : public password_manager::PasswordStoreConsumer {
 public:
  explicit SavePasswordsConsumer(id<SavePasswordsConsumerDelegate> delegate);

  SavePasswordsConsumer(const SavePasswordsConsumer&) = delete;
  SavePasswordsConsumer& operator=(const SavePasswordsConsumer&) = delete;

  ~SavePasswordsConsumer() override;

  // password_manager::PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;
  void OnGetPasswordStoreResultsFrom(
      password_manager::PasswordStoreInterface* store,
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  base::WeakPtr<password_manager::PasswordStoreConsumer> GetWeakPtr();

 private:
  __weak id<SavePasswordsConsumerDelegate> delegate_ = nil;
  base::WeakPtrFactory<SavePasswordsConsumer> weak_ptr_factory_{this};
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_SAVE_PASSWORDS_CONSUMER_H_
