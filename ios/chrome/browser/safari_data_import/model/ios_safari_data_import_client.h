// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_MODEL_IOS_SAFARI_DATA_IMPORT_CLIENT_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_MODEL_IOS_SAFARI_DATA_IMPORT_CLIENT_H_

#import <Foundation/Foundation.h>

#import "base/callback_list.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/types/expected.h"
#import "components/user_data_importer/utility/safari_data_import_client.h"

@protocol ImportDataItemConsumer;
@class PasswordImportItem;

// A C++ class that provides a platform-specific implementation for
// `SafariDataImportClient` on iOS.
class IOSSafariDataImportClient
    : public user_data_importer::SafariDataImportClient {
 public:
  // Container for the callbacks registered with
  // `RegisterCallbackOnImportFailure`.
  using ImportFailureCallbackList = base::OnceCallbackList<void()>;
  using ImportFailureCallback = ImportFailureCallbackList::CallbackType;

  IOSSafariDataImportClient();
  ~IOSSafariDataImportClient() override;

  // Consumer object handling UI updates to display the progress of data
  // importing. This needs to be set before preparing the .zip file containing
  // Safari data.
  void SetImportDataItemConsumer(id<ImportDataItemConsumer> consumer);

  // Register callback function invoked when no Safari data item could be
  // loaded.
  void RegisterCallbackOnImportFailure(ImportFailureCallback callback);

  // List of password conflicts with the information retrieved from the source
  // of import. Only available `OnPasswordsReady` is invoked.
  NSArray<PasswordImportItem*>* GetConflictingPasswords();

  // List of passwords failed to be imported. Only available
  // `OnPasswordsImported` is invoked.
  NSArray<PasswordImportItem*>* GetInvalidPasswords();

  // SafariDataImportClient:
  void OnTotalFailure() override;
  void OnBookmarksReady(user_data_importer::CountOrError result) override;
  void OnHistoryReady(
      user_data_importer::CountOrError estimated_count) override;
  void OnPasswordsReady(
      base::expected<password_manager::ImportResults,
                     user_data_importer::ImportPreparationError> results)
      override;
  void OnPaymentCardsReady(user_data_importer::CountOrError result) override;
  void OnBookmarksImported(size_t count) override;
  void OnHistoryImported(size_t count) override;
  void OnPasswordsImported(
      const password_manager::ImportResults& results) override;
  void OnPaymentCardsImported(size_t count) override;
  base::WeakPtr<SafariDataImportClient> AsWeakPtr() override;

 private:
  // Ensures all UI updates happen on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  // Object handling ready or imported Safari Data items.
  __weak id<ImportDataItemConsumer> consumer_;

  // List of registered callbacks, and the object managing its lifetime.
  ImportFailureCallbackList failure_callbacks_;
  base::CallbackListSubscription failure_callbacks_subscription_;

  // Lists of conflicting and invalid passwords.
  NSArray<PasswordImportItem*>* conflicting_passwords_;
  NSArray<PasswordImportItem*>* invalid_passwords_;

  // Weak pointer factory.
  base::WeakPtrFactory<IOSSafariDataImportClient> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_MODEL_IOS_SAFARI_DATA_IMPORT_CLIENT_H_
