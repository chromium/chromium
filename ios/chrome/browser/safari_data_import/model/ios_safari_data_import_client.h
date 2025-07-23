// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_MODEL_IOS_SAFARI_DATA_IMPORT_CLIENT_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_MODEL_IOS_SAFARI_DATA_IMPORT_CLIENT_H_

#import "base/memory/weak_ptr.h"
#import "components/user_data_importer/utility/safari_data_import_client.h"

@protocol SafariDataItemConsumer;

// A C++ class that provides a platform-specific implementation for
// `SafariDataImportClient` on iOS.
class IOSSafariDataImportClient : public SafariDataImportClient {
 public:
  IOSSafariDataImportClient();
  ~IOSSafariDataImportClient() override;

  // Consumer object handling UI updates to display the progress of data
  // importing. This needs to be set before preparing the .zip file containing
  // Safari data.
  void SetSafariDataItemConsumer(id<SafariDataItemConsumer> consumer);

  // SafariDataImportClient:
  void OnTotalFailure() override;
  void OnBookmarksReady(size_t count) override;
  void OnHistoryReady(size_t estimated_count,
                      std::vector<std::u16string> profiles) override;
  void OnPasswordsReady(
      const password_manager::ImportResults& results) override;
  void OnPaymentCardsReady(size_t count) override;
  void OnBookmarksImported(size_t count) override;
  void OnHistoryImported(size_t count) override;
  void OnPasswordsImported(
      const password_manager::ImportResults& results) override;
  void OnPaymentCardsImported(size_t count) override;
  base::WeakPtr<SafariDataImportClient> AsWeakPtr() override;

 private:
  __weak id<SafariDataItemConsumer> consumer_;
  base::WeakPtrFactory<IOSSafariDataImportClient> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_MODEL_IOS_SAFARI_DATA_IMPORT_CLIENT_H_
