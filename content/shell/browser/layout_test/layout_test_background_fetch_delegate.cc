// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_background_fetch_delegate.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "components/download/content/factory/download_service_factory.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/background_service/features.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// Implementation of a Download Service client that will be servicing
// Background Fetch requests when running layout tests.
class LayoutTestBackgroundFetchDelegate::LayoutTestBackgroundFetchDownloadClient
    : public download::Client {
 public:
  explicit LayoutTestBackgroundFetchDownloadClient(
      base::WeakPtr<content::BackgroundFetchDelegate::Client> client)
      : client_(std::move(client)) {}

  ~LayoutTestBackgroundFetchDownloadClient() override {
    if (client_)
      client_->OnDelegateShutdown();
  }

  // Registers the |guid| as belonging to a Background Fetch job identified by
  // |job_unique_id|. Downloads may only be registered once.
  void RegisterDownload(const std::string& guid,
                        const std::string& job_unique_id) {
    DCHECK(!guid_to_unique_job_id_mapping_.count(guid));
    guid_to_unique_job_id_mapping_[guid] = job_unique_id;
  }

  // download::Client implementation:
  void OnServiceInitialized(
      bool state_lost,
      const std::vector<download::DownloadMetaData>& downloads) override {}

  void OnServiceUnavailable() override {}

  download::Client::ShouldDownload OnDownloadStarted(
      const std::string& guid,
      const std::vector<GURL>& url_chain,
      const scoped_refptr<const net::HttpResponseHeaders>& headers) override {
    DCHECK(guid_to_unique_job_id_mapping_.count(guid));
    if (!client_)
      return download::Client::ShouldDownload::ABORT;

    guid_to_response_[guid] =
        std::make_unique<content::BackgroundFetchResponse>(url_chain,
                                                           std::move(headers));

    client_->OnDownloadStarted(
        guid_to_unique_job_id_mapping_[guid], guid,
        std::make_unique<content::BackgroundFetchResponse>(
            guid_to_response_[guid]->url_chain,
            guid_to_response_[guid]->headers));

    return download::Client::ShouldDownload::CONTINUE;
  }

  void OnDownloadUpdated(const std::string& guid,
                         uint64_t bytes_downloaded) override {
    DCHECK(guid_to_unique_job_id_mapping_.count(guid));
    if (!client_)
      return;

    client_->OnDownloadUpdated(guid_to_unique_job_id_mapping_[guid], guid,
                               bytes_downloaded);
  }

  void OnDownloadFailed(const std::string& guid,
                        const download::CompletionInfo& info,
                        download::Client::FailureReason reason) override {
    DCHECK(guid_to_unique_job_id_mapping_.count(guid));
    if (!client_)
      return;

    content::BackgroundFetchResult::FailureReason failure_reason;
    switch (reason) {
      case download::Client::FailureReason::NETWORK:
        failure_reason = content::BackgroundFetchResult::FailureReason::NETWORK;
        break;
      case download::Client::FailureReason::TIMEDOUT:
        failure_reason =
            content::BackgroundFetchResult::FailureReason::TIMEDOUT;
        break;
      case download::Client::FailureReason::UNKNOWN:
        failure_reason =
            content::BackgroundFetchResult::FailureReason::FETCH_ERROR;
        break;
      case download::Client::FailureReason::ABORTED:
      case download::Client::FailureReason::CANCELLED:
        // The client cancelled or aborted it so no need to notify it.
        return;
      default:
        NOTREACHED();
        return;
    }

    std::unique_ptr<BackgroundFetchResult> result =
        std::make_unique<BackgroundFetchResult>(
            std::move(guid_to_response_[guid]), base::Time::Now(),
            failure_reason);

    // Inform the client about the failed |result|. Then remove the mapping as
    // no further communication is expected from the download service.
    client_->OnDownloadComplete(guid_to_unique_job_id_mapping_[guid], guid,
                                std::move(result));

    guid_to_unique_job_id_mapping_.erase(guid);
    guid_to_response_.erase(guid);
  }

  void OnDownloadSucceeded(const std::string& guid,
                           const download::CompletionInfo& info) override {
    DCHECK(guid_to_unique_job_id_mapping_.count(guid));
    if (!client_)
      return;

    std::unique_ptr<BackgroundFetchResult> result =
        std::make_unique<BackgroundFetchResult>(
            std::move(guid_to_response_[guid]), base::Time::Now(), info.path,
            info.blob_handle, info.bytes_downloaded);

    // Inform the client about the successful |result|. Then remove the mapping
    // as no further communication is expected from the download service.
    client_->OnDownloadComplete(guid_to_unique_job_id_mapping_[guid], guid,
                                std::move(result));

    guid_to_unique_job_id_mapping_.erase(guid);
    guid_to_response_.erase(guid);
  }

  bool CanServiceRemoveDownloadedFile(const std::string& guid,
                                      bool force_delete) override {
    return true;
  }

  void GetUploadData(const std::string& guid,
                     download::GetUploadDataCallback callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
  }

 private:
  base::WeakPtr<content::BackgroundFetchDelegate::Client> client_;
  base::flat_map<std::string, std::string> guid_to_unique_job_id_mapping_;
  base::flat_map<std::string, std::unique_ptr<content::BackgroundFetchResponse>>
      guid_to_response_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestBackgroundFetchDownloadClient);
};

LayoutTestBackgroundFetchDelegate::LayoutTestBackgroundFetchDelegate(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

LayoutTestBackgroundFetchDelegate::~LayoutTestBackgroundFetchDelegate() =
    default;

void LayoutTestBackgroundFetchDelegate::GetIconDisplaySize(
    GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(gfx::Size(192, 192));
}

void LayoutTestBackgroundFetchDelegate::GetPermissionForOrigin(
    const url::Origin& origin,
    const ResourceRequestInfo::WebContentsGetter& wc_getter,
    GetPermissionForOriginCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(BackgroundFetchPermission::ALLOWED);
}

void LayoutTestBackgroundFetchDelegate::CreateDownloadJob(
    std::unique_ptr<BackgroundFetchDescription> fetch_description) {}

void LayoutTestBackgroundFetchDelegate::DownloadUrl(
    const std::string& job_unique_id,
    const std::string& download_guid,
    const std::string& method,
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const net::HttpRequestHeaders& headers) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Lazily create the |download_service_| because only very few layout tests
  // actually require Background Fetch.
  if (!download_service_) {
    auto clients = std::make_unique<download::DownloadClientMap>();
    auto background_fetch_client =
        std::make_unique<LayoutTestBackgroundFetchDownloadClient>(client());

    // Store a reference to the client for storing a GUID -> Unique ID mapping.
    background_fetch_client_ = background_fetch_client.get();

    clients->emplace(download::DownloadClient::BACKGROUND_FETCH,
                     std::move(background_fetch_client));

    // Use a ScopedFeatureList to enable and configure the download service as
    // if done by Finch. We have a strict dependency on it.
    {
      base::test::ScopedFeatureList download_service_configuration;
      download_service_configuration.InitAndEnableFeatureWithParameters(
          download::kDownloadServiceFeature, {{"start_up_delay_ms", "0"}});

      download_service_ =
          base::WrapUnique(download::BuildInMemoryDownloadService(
              browser_context_, std::move(clients),
              GetNetworkConnectionTracker(), base::FilePath(),
              BrowserContext::GetBlobStorageContext(browser_context_),
              base::CreateSingleThreadTaskRunnerWithTraits(
                  {BrowserThread::IO})));
    }
  }

  background_fetch_client_->RegisterDownload(download_guid, job_unique_id);

  download::DownloadParams params;
  params.guid = download_guid;
  params.client = download::DownloadClient::BACKGROUND_FETCH;
  params.request_params.method = method;
  params.request_params.url = url;
  params.request_params.request_headers = headers;
  params.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation);

  download_service_->StartDownload(params);
}

void LayoutTestBackgroundFetchDelegate::Abort(
    const std::string& job_unique_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(peter): Implement the ability to abort the |job_unique_id|.
}

void LayoutTestBackgroundFetchDelegate::UpdateUI(
    const std::string& job_unique_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon) {}

}  // namespace content
