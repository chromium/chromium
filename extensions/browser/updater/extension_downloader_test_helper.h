// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TEST_HELPER_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TEST_HELPER_H_

#include <memory>
#include <set>
#include <string>

#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

class MockExtensionDownloaderDelegate : public ExtensionDownloaderDelegate {
 public:
  MockExtensionDownloaderDelegate();

  ~MockExtensionDownloaderDelegate();

  MOCK_METHOD4(
      OnExtensionDownloadFailed,
      void(const ExtensionId&, Error, const PingResult&, const std::set<int>&));
  MOCK_METHOD2(OnExtensionDownloadStageChanged,
               void(const ExtensionId&, Stage));
  MOCK_METHOD2(OnExtensionDownloadCacheStatusRetrieved,
               void(const ExtensionId&, CacheStatus));
  MOCK_METHOD7(OnExtensionDownloadFinished,
               void(const extensions::CRXFileInfo&,
                    bool,
                    const GURL&,
                    const std::string&,
                    const PingResult&,
                    const std::set<int>&,
                    const InstallCallback&));
  MOCK_METHOD0(OnExtensionDownloadRetryForTests, void());
  MOCK_METHOD2(GetPingDataForExtension,
               bool(const ExtensionId&, ManifestFetchData::PingData*));
  MOCK_METHOD1(GetUpdateUrlData, std::string(const ExtensionId&));
  MOCK_METHOD1(IsExtensionPending, bool(const ExtensionId&));
  MOCK_METHOD2(GetExtensionExistingVersion,
               bool(const ExtensionId&, std::string*));

  void Wait();

  void Quit();

  void DelegateTo(ExtensionDownloaderDelegate* delegate);

 private:
  base::RepeatingClosure quit_closure_;
};

// Creates ExtensionDownloader for tests, with mocked delegate and
// TestURLLoaderFactory as a URL factory.
class ExtensionDownloaderTestHelper {
 public:
  ExtensionDownloaderTestHelper();

  ~ExtensionDownloaderTestHelper();

  MockExtensionDownloaderDelegate& delegate() { return delegate_; }
  ExtensionDownloader& downloader() { return downloader_; }
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return test_shared_url_loader_factory_;
  }
  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

  // Returns a request that URL loader factory has received (or nullptr if it
  // didn't receive enough requests).
  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest(
      size_t index = 0);

  // Clears previously added responses from URL loader factory.
  void ClearURLLoaderFactoryResponses();

  // Create a downloader in a separate pointer. Could be used by, for example,
  // ExtensionUpdater.
  std::unique_ptr<ExtensionDownloader> CreateDownloader();

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  MockExtensionDownloaderDelegate delegate_;
  ExtensionDownloader downloader_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDownloaderTestHelper);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TEST_HELPER_H_
