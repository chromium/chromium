// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader_test_helper.h"

#include "base/run_loop.h"
#include "extensions/common/verifier_formats.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

using testing::_;
using testing::Invoke;

namespace extensions {

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
  ON_CALL(*this, OnExtensionDownloadFailed(_, _, _, _))
      .WillByDefault(Invoke(
          delegate, &ExtensionDownloaderDelegate::OnExtensionDownloadFailed));
  ON_CALL(*this, OnExtensionDownloadStageChanged(_, _))
      .WillByDefault(Invoke(
          delegate,
          &ExtensionDownloaderDelegate::OnExtensionDownloadStageChanged));
  ON_CALL(*this, OnExtensionDownloadFinished(_, _, _, _, _, _, _))
      .WillByDefault(Invoke(
          delegate, &ExtensionDownloaderDelegate::OnExtensionDownloadFinished));
  ON_CALL(*this, OnExtensionDownloadRetryForTests())
      .WillByDefault(Invoke(
          delegate,
          &ExtensionDownloaderDelegate::OnExtensionDownloadRetryForTests));
  ON_CALL(*this, GetPingDataForExtension(_, _))
      .WillByDefault(Invoke(
          delegate, &ExtensionDownloaderDelegate::GetPingDataForExtension));
  ON_CALL(*this, GetUpdateUrlData(_))
      .WillByDefault(
          Invoke(delegate, &ExtensionDownloaderDelegate::GetUpdateUrlData));
  ON_CALL(*this, IsExtensionPending(_))
      .WillByDefault(
          Invoke(delegate, &ExtensionDownloaderDelegate::IsExtensionPending));
  ON_CALL(*this, GetExtensionExistingVersion(_, _))
      .WillByDefault(Invoke(
          delegate, &ExtensionDownloaderDelegate::GetExtensionExistingVersion));
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

network::TestURLLoaderFactory::PendingRequest*
ExtensionDownloaderTestHelper::GetPendingRequest(size_t index) {
  if (index >= test_url_loader_factory_.pending_requests()->size())
    return nullptr;
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

}  // namespace extensions
