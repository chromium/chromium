// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TEST_HELPER_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TEST_HELPER_H_

#include <memory>
#include <set>
#include <string>

#include "extensions/browser/updater/extension_cache.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/browser/updater/extension_downloader_types.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

// Backoff policy to enforce zero initial delay.
const extern net::BackoffEntry::Policy kZeroBackoffPolicy;

class MockExtensionDownloaderDelegate
    : public ::testing::NiceMock<ExtensionDownloaderDelegate> {
 public:
  MockExtensionDownloaderDelegate();

  ~MockExtensionDownloaderDelegate();

  MOCK_METHOD5(OnExtensionDownloadFailed,
               void(const ExtensionId&,
                    Error,
                    const PingResult&,
                    const std::set<int>&,
                    const FailureData&));
  MOCK_METHOD2(OnExtensionDownloadStageChanged,
               void(const ExtensionId&, Stage));
  MOCK_METHOD(void,
              OnExtensionUpdateFound,
              (const ExtensionId&, const std::set<int>&, const base::Version&),
              (override));
  MOCK_METHOD2(OnExtensionDownloadCacheStatusRetrieved,
               void(const ExtensionId&, CacheStatus));
  // Gmock doesn't have good support for move-only types like
  // base::OnceCallback, so we have to do this hack.
  void OnExtensionDownloadFinished(const CRXFileInfo& file,
                                   bool file_ownership_passed,
                                   const GURL& download_url,
                                   const PingResult& ping_result,
                                   const std::set<int>& request_ids,
                                   InstallCallback callback) override {
    OnExtensionDownloadFinished_(file, file_ownership_passed, download_url,
                                 ping_result, request_ids, callback);
  }
  MOCK_METHOD6(OnExtensionDownloadFinished_,
               void(const extensions::CRXFileInfo&,
                    bool,
                    const GURL&,
                    const PingResult&,
                    const std::set<int>&,
                    InstallCallback&));
  MOCK_METHOD0(OnExtensionDownloadRetryForTests, void());
  MOCK_METHOD2(GetPingDataForExtension,
               bool(const ExtensionId&, DownloadPingData*));
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

class MockExtensionCache : public ExtensionCache {
 public:
  MockExtensionCache();
  ~MockExtensionCache() override;

  void Start(base::OnceClosure callback) override;
  void Shutdown(base::OnceClosure callback) override;
  MOCK_METHOD1(AllowCaching, void(const ExtensionId&));
  MOCK_METHOD4(GetExtension,
               bool(const ExtensionId&,
                    const std::string& expected_hash,
                    base::FilePath* path,
                    std::string* version));
  MOCK_METHOD5(PutExtension,
               void(const ExtensionId&,
                    const std::string& hash,
                    const base::FilePath& path,
                    const std::string& version,
                    PutExtensionCallback callback));

  MOCK_METHOD3(OnInstallFailed,
               bool(const std::string& id,
                    const std::string& hash,
                    const CrxInstallError& error));
};

// Creates ExtensionDownloader for tests, with mocked delegate and
// TestURLLoaderFactory as a URL factory.
class ExtensionDownloaderTestHelper {
 public:
  static constexpr DownloadPingData kNeverPingedData =
      DownloadPingData(ManifestFetchData::kNeverPinged,
                       ManifestFetchData::kNeverPinged,
                       true,
                       0);
  static constexpr char kEmptyUpdateUrlData[] = "";

  ExtensionDownloaderTestHelper();

  ExtensionDownloaderTestHelper(const ExtensionDownloaderTestHelper&) = delete;
  ExtensionDownloaderTestHelper& operator=(
      const ExtensionDownloaderTestHelper&) = delete;

  ~ExtensionDownloaderTestHelper();

  MockExtensionDownloaderDelegate& delegate() { return delegate_; }
  ExtensionDownloader& downloader() { return downloader_; }
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return test_shared_url_loader_factory_;
  }
  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

  // Adds a fetch request directly to the internal downloader.
  void StartUpdateCheck(std::unique_ptr<ManifestFetchData> fetch_data);

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
};

// Creates a downloader task with most arguments set to default values.
// Note that as ExtensionDownloaderTask is just a simple struct, callers can
// configure additional properties if needed.
ExtensionDownloaderTask CreateDownloaderTask(const ExtensionId& id,
                                             const GURL& update_url = {});

// Creates extension info and associated task, adds both to `fetch_data`.
void AddExtensionToFetchDataForTesting(ManifestFetchData* fetch_data,
                                       const ExtensionId& id,
                                       const std::string& version,
                                       const GURL& update_url,
                                       DownloadPingData ping_data);

// Simplified version with fewer arguments.
void AddExtensionToFetchDataForTesting(ManifestFetchData* fetch_data,
                                       const ExtensionId& id,
                                       const std::string& version,
                                       const GURL& update_url);

// Struct for creating app entries in the update manifest XML.
struct UpdateManifestItem {
  explicit UpdateManifestItem(ExtensionId id);
  ~UpdateManifestItem();
  // We need copy items to be able to use them to initialize e.g. vector of
  // items via {item1, item2, ...} syntax.
  UpdateManifestItem(const UpdateManifestItem&);
  UpdateManifestItem& operator=(const UpdateManifestItem&);
  UpdateManifestItem(UpdateManifestItem&&);
  UpdateManifestItem& operator=(UpdateManifestItem&&);

  UpdateManifestItem&& codebase(std::string value) && {
    updatecheck_params.emplace("codebase", std::move(value));
    return std::move(*this);
  }
  UpdateManifestItem&& hash(std::string value) && {
    updatecheck_params.emplace("hash", std::move(value));
    return std::move(*this);
  }
  UpdateManifestItem&& hash_sha256(std::string value) && {
    updatecheck_params.emplace("hash_sha256", std::move(value));
    return std::move(*this);
  }
  UpdateManifestItem&& info(std::string value) && {
    updatecheck_params.emplace("info", std::move(value));
    return std::move(*this);
  }
  UpdateManifestItem&& prodversionmin(std::string value) && {
    updatecheck_params.emplace("prodversionmin", std::move(value));
    return std::move(*this);
  }
  UpdateManifestItem&& status(std::string value) && {
    updatecheck_params.emplace("status", std::move(value));
    return std::move(*this);
  }
  UpdateManifestItem&& version(std::string value) && {
    updatecheck_params.emplace("version", std::move(value));
    return std::move(*this);
  }

  ExtensionId id;
  std::map<std::string, std::string> updatecheck_params;
};

// A generic method to create an XML update manifest. For each extension an
// extension ID should be provided along with parameters of the updatecheck
// tag.
std::string CreateUpdateManifest(
    const std::vector<UpdateManifestItem>& extensions);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TEST_HELPER_H_
