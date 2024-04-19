// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_DOWNLOAD_MANAGER_H_
#define CONTENT_PUBLIC_TEST_MOCK_DOWNLOAD_MANAGER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/input_stream.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

class DownloadRequestHandle;

namespace content {

// To avoid leaking download_request_handle.h to embedders.
void PrintTo(const DownloadRequestHandle& params, std::ostream* os);

class MockDownloadManager : public DownloadManager {
 public:
  // Structure to make it possible to match more than 10 arguments on
  // CreateDownloadItem.
  struct CreateDownloadItemAdapter {
    std::string guid;
    uint32_t id;
    base::FilePath current_path;
    base::FilePath target_path;
    std::vector<GURL> url_chain;
    GURL referrer_url;
    std::string serialized_embedder_download_data;
    GURL tab_url;
    GURL tab_referrer_url;
    std::optional<url::Origin> request_initiator;
    std::string mime_type;
    std::string original_mime_type;
    base::Time start_time;
    base::Time end_time;
    std::string etag;
    std::string last_modified;
    int64_t received_bytes;
    int64_t total_bytes;
    std::string hash;
    download::DownloadItem::DownloadState state;
    download::DownloadDangerType danger_type;
    download::DownloadInterruptReason interrupt_reason;
    bool opened;
    base::Time last_access_time;
    bool transient;
    std::vector<download::DownloadItem::ReceivedSlice> received_slices;

    CreateDownloadItemAdapter(
        const std::string& guid,
        uint32_t id,
        const base::FilePath& current_path,
        const base::FilePath& target_path,
        const std::vector<GURL>& url_chain,
        const GURL& referrer_url,
        const std::string& serialized_embedder_download_data,
        const GURL& tab_url,
        const GURL& tab_refererr_url,
        const std::optional<url::Origin>& request_initiator,
        const std::string& mime_type,
        const std::string& original_mime_type,
        base::Time start_time,
        base::Time end_time,
        const std::string& etag,
        const std::string& last_modified,
        int64_t received_bytes,
        int64_t total_bytes,
        const std::string& hash,
        download::DownloadItem::DownloadState state,
        download::DownloadDangerType danger_type,
        download::DownloadInterruptReason interrupt_reason,
        bool opened,
        base::Time last_access_time,
        bool transient,
        const std::vector<download::DownloadItem::ReceivedSlice>&
            received_slices);

    // Required by clang compiler.
    CreateDownloadItemAdapter(const CreateDownloadItemAdapter& rhs);
    ~CreateDownloadItemAdapter();

    bool operator==(const CreateDownloadItemAdapter& rhs) const;
  };

  MockDownloadManager();
  ~MockDownloadManager() override;

  // DownloadManager:
  MOCK_METHOD1(SetDelegate, void(DownloadManagerDelegate* delegate));
  MOCK_METHOD0(GetDelegate, DownloadManagerDelegate*());
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD1(GetAllDownloads, void(DownloadVector* downloads));
  MOCK_METHOD1(GetUninitializedActiveDownloadsIfAny,
               void(DownloadVector* downloads));
  MOCK_METHOD1(Init, bool(BrowserContext* browser_context));
  MOCK_METHOD3(RemoveDownloadsByURLAndTime,
               int(const base::RepeatingCallback<bool(const GURL&)>& url_filter,
                   base::Time remove_begin,
                   base::Time remove_end));
  MOCK_METHOD1(DownloadUrlMock, void(download::DownloadUrlParameters*));
  void DownloadUrl(
      std::unique_ptr<download::DownloadUrlParameters> params) override {
    DownloadUrl(std::move(params), nullptr);
  }
  void DownloadUrl(std::unique_ptr<download::DownloadUrlParameters> params,
                   scoped_refptr<network::SharedURLLoaderFactory>
                       blob_url_loader_factory) override {
    DownloadUrlMock(params.get());
  }
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));

  // Redirects to mock method to get around gmock 10 argument limit.
  download::DownloadItem* CreateDownloadItem(
      const std::string& guid,
      uint32_t id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const StoragePartitionConfig& storage_partition_config,
      const GURL& tab_url,
      const GURL& tab_refererr_url,
      const std::optional<url::Origin>& request_initiator,
      const std::string& mime_type,
      const std::string& original_mime_type,
      base::Time start_time,
      base::Time end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64_t received_bytes,
      int64_t total_bytes,
      const std::string& hash,
      download::DownloadItem::DownloadState state,
      download::DownloadDangerType danger_type,
      download::DownloadInterruptReason interrupt_reason,
      bool opened,
      base::Time last_access_time,
      bool transient,
      const std::vector<download::DownloadItem::ReceivedSlice>& received_slices)
      override;

  MOCK_METHOD1(MockCreateDownloadItem,
               download::DownloadItem*(CreateDownloadItemAdapter adapter));
  MOCK_METHOD1(PostInitialization,
               void(DownloadInitializationDependency dependency));
  MOCK_METHOD0(IsManagerInitialized, bool());
  MOCK_METHOD0(InProgressCount, int());
  MOCK_METHOD0(BlockingShutdownCount, int());
  MOCK_METHOD0(GetBrowserContext, BrowserContext*());
  MOCK_METHOD0(CheckForHistoryFilesRemoval, void());
  MOCK_METHOD1(GetDownload, download::DownloadItem*(uint32_t id));
  MOCK_METHOD1(GetDownloadByGuid, download::DownloadItem*(const std::string&));
  MOCK_METHOD1(GetNextId, void(base::OnceCallback<void(uint32_t)>));
  MOCK_METHOD1(CanDownload, bool(download::DownloadUrlParameters*));
  MOCK_METHOD1(GetStoragePartitionConfigForSiteUrl,
               StoragePartitionConfig(const GURL&));

  // Implement a simple serialization and deserialization of
  // StoragePartitionConfig for the mock.
  std::string StoragePartitionConfigToSerializedEmbedderDownloadData(
      const StoragePartitionConfig& storage_partition_config) override;
  StoragePartitionConfig SerializedEmbedderDownloadDataToStoragePartitionConfig(
      const std::string& serialized_embedder_download_data) override;

  void OnHistoryQueryComplete(
      base::OnceClosure load_history_downloads_cb) override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_DOWNLOAD_MANAGER_H_
