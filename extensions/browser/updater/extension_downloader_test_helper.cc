// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader_test_helper.h"

#include "base/run_loop.h"
#include "extensions/common/verifier_formats.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

using testing::_;
using testing::Invoke;

namespace extensions {

const net::BackoffEntry::Policy kZeroBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    0,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.1,

    // Maximum amount of time we are willing to delay our request in ms.
    600000,  // Ten minutes.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

MockExtensionDownloaderDelegate::MockExtensionDownloaderDelegate() = default;

MockExtensionDownloaderDelegate::~MockExtensionDownloaderDelegate() = default;

void MockExtensionDownloaderDelegate::Wait() {
  std::unique_ptr<base::RunLoop> runner = std::make_unique<base::RunLoop>();
  quit_closure_ = runner->QuitClosure();
  runner->Run();
  quit_closure_.Reset();
}

void MockExtensionDownloaderDelegate::Quit() {
  quit_closure_.Run();
}

void MockExtensionDownloaderDelegate::DelegateTo(
    ExtensionDownloaderDelegate* delegate) {
  ON_CALL(*this, OnExtensionDownloadFailed(_, _, _, _, _))
      .WillByDefault(Invoke(
          delegate, &ExtensionDownloaderDelegate::OnExtensionDownloadFailed));
  ON_CALL(*this, OnExtensionDownloadStageChanged(_, _))
      .WillByDefault(Invoke(
          delegate,
          &ExtensionDownloaderDelegate::OnExtensionDownloadStageChanged));
  ON_CALL(*this, OnExtensionDownloadFinished_(_, _, _, _, _, _))
      .WillByDefault(Invoke(
          [delegate](const CRXFileInfo& file, bool file_ownership_passed,
                     const GURL& download_url, const PingResult& ping_result,
                     const std::set<int>& request_ids,
                     InstallCallback& callback) {
            delegate->OnExtensionDownloadFinished(
                file, file_ownership_passed, download_url, ping_result,
                request_ids, std::move(callback));
          }));
  ON_CALL(*this, OnExtensionDownloadRetryForTests())
      .WillByDefault(Invoke(
          delegate,
          &ExtensionDownloaderDelegate::OnExtensionDownloadRetryForTests));
  ON_CALL(*this, GetPingDataForExtension(_, _))
      .WillByDefault(Invoke(
          delegate, &ExtensionDownloaderDelegate::GetPingDataForExtension));
  ON_CALL(*this, IsExtensionPending(_))
      .WillByDefault(
          Invoke(delegate, &ExtensionDownloaderDelegate::IsExtensionPending));
  ON_CALL(*this, GetExtensionExistingVersion(_, _))
      .WillByDefault(Invoke(
          delegate, &ExtensionDownloaderDelegate::GetExtensionExistingVersion));
}

MockExtensionCache::MockExtensionCache() = default;
MockExtensionCache::~MockExtensionCache() = default;

void MockExtensionCache::Start(base::OnceClosure callback) {
  std::move(callback).Run();
}

void MockExtensionCache::Shutdown(base::OnceClosure callback) {
  std::move(callback).Run();
}

ExtensionDownloaderTestHelper::ExtensionDownloaderTestHelper()
    : test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      delegate_(),
      downloader_(&delegate_,
                  test_shared_url_loader_factory_,
                  GetTestVerifierFormat()) {}

ExtensionDownloaderTestHelper::~ExtensionDownloaderTestHelper() = default;

void ExtensionDownloaderTestHelper::StartUpdateCheck(
    std::unique_ptr<ManifestFetchData> fetch_data) {
  downloader_.StartUpdateCheck(std::move(fetch_data));
}

network::TestURLLoaderFactory::PendingRequest*
ExtensionDownloaderTestHelper::GetPendingRequest(size_t index) {
  if (index >= test_url_loader_factory_.pending_requests()->size()) {
    return nullptr;
  }
  return &(*test_url_loader_factory_.pending_requests())[index];
}

void ExtensionDownloaderTestHelper::ClearURLLoaderFactoryResponses() {
  test_url_loader_factory_.ClearResponses();
}

std::unique_ptr<ExtensionDownloader>
ExtensionDownloaderTestHelper::CreateDownloader() {
  return std::make_unique<ExtensionDownloader>(
      &delegate_, test_shared_url_loader_factory_, GetTestVerifierFormat());
}

ExtensionDownloaderTask CreateDownloaderTask(const ExtensionId& id,
                                             const GURL& update_url) {
  return ExtensionDownloaderTask(
      id, update_url, mojom::ManifestLocation::kInternal,
      false /* is_corrupt_reinstall */, 0 /* request_id */,
      DownloadFetchPriority::kBackground);
}

void AddExtensionToFetchDataForTesting(ManifestFetchData* fetch_data,
                                       const ExtensionId& id,
                                       const std::string& version,
                                       const GURL& update_url,
                                       DownloadPingData ping_data) {
  fetch_data->AddExtension(id, version, &ping_data,
                           ExtensionDownloaderTestHelper::kEmptyUpdateUrlData,
                           std::string(), mojom::ManifestLocation::kInternal,
                           DownloadFetchPriority::kBackground);
  fetch_data->AddAssociatedTask(CreateDownloaderTask(id, update_url));
}

void AddExtensionToFetchDataForTesting(ManifestFetchData* fetch_data,
                                       const ExtensionId& id,
                                       const std::string& version,
                                       const GURL& update_url) {
  AddExtensionToFetchDataForTesting(
      fetch_data, id, version, update_url,
      ExtensionDownloaderTestHelper::kNeverPingedData);
}

UpdateManifestItem::UpdateManifestItem(ExtensionId id) : id(std::move(id)) {}
UpdateManifestItem::~UpdateManifestItem() = default;
UpdateManifestItem::UpdateManifestItem(const UpdateManifestItem&) = default;
UpdateManifestItem& UpdateManifestItem::operator=(const UpdateManifestItem&) =
    default;
UpdateManifestItem::UpdateManifestItem(UpdateManifestItem&&) = default;
UpdateManifestItem& UpdateManifestItem::operator=(UpdateManifestItem&&) =
    default;

std::string CreateUpdateManifest(
    const std::vector<UpdateManifestItem>& extensions) {
  std::string content =
      "<?xml version='1.0' encoding='UTF-8'?>"
      "<gupdate xmlns='http://www.google.com/update2/response'"
      "                protocol='2.0'>";
  for (const auto& update_item : extensions) {
    content += base::StringPrintf(
        " <app appid='%s'>"
        "  <updatecheck",
        update_item.id.c_str());
    for (const auto& [name, value] : update_item.updatecheck_params) {
      content += base::StringPrintf(" %s='%s'", name.c_str(), value.c_str());
    }
    content +=
        " />"
        " </app>";
  }
  content += "</gupdate>";
  return content;
}

}  // namespace extensions
