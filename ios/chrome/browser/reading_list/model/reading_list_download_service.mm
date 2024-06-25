// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_download_service.h"

#import <memory>
#import <utility>

#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/memory/scoped_refptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/string_util.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/reading_list/core/offline_url_utils.h"
#import "components/reading_list/core/reading_list_entry.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/reading_list/model/reading_list_distiller_page_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "net/base/network_change_notifier.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
// Status of the download when it ends, for UMA report.
// These match tools/metrics/histograms/histograms.xml.
enum UMADownloadStatus {
  // The download was successful.
  SUCCESS = 0,
  // The download failed and it won't be retried.
  FAILURE = 1,
  // The download failed and it will be retried.
  RETRY = 2,
  // Add new enum above STATUS_MAX.
  STATUS_MAX
};

// Number of time the download must fail before the download occurs only in
// wifi.
const int kNumberOfFailsBeforeWifiOnly = 5;
// Number of time the download must fail before we give up trying to download
// it.
const int kNumberOfFailsBeforeStop = 7;

// Scans `root` directory and deletes all subdirectories not listed
// in `directories_to_keep`.
// Must be called on File thread.
void CleanUpFiles(base::FilePath root,
                  const std::set<std::string>& processed_directories) {
  base::FileEnumerator file_enumerator(root, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath sub_directory = file_enumerator.Next();
       !sub_directory.empty(); sub_directory = file_enumerator.Next()) {
    std::string directory_name = sub_directory.BaseName().value();
    if (!processed_directories.count(directory_name)) {
      base::DeletePathRecursively(sub_directory);
    }
  }
}

}  // namespace

ReadingListDownloadService::ReadingListDownloadService(
    ReadingListModel* reading_list_model,
    PrefService* prefs,
    base::FilePath chrome_profile_path,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<dom_distiller::DistillerFactory> distiller_factory,
    std::unique_ptr<reading_list::ReadingListDistillerPageFactory>
        distiller_page_factory)
    : reading_list_model_(reading_list_model),
      chrome_profile_path_(chrome_profile_path),
      had_connection_(!net::NetworkChangeNotifier::IsOffline()),
      distiller_page_factory_(std::move(distiller_page_factory)),
      distiller_factory_(std::move(distiller_factory)),
      weak_ptr_factory_(this) {
  DCHECK(reading_list_model);

  url_downloader_ = std::make_unique<URLDownloader>(
      distiller_factory_.get(), distiller_page_factory_.get(), prefs,
      chrome_profile_path, url_loader_factory,
      base::BindRepeating(&ReadingListDownloadService::OnDownloadEnd,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ReadingListDownloadService::OnDeleteEnd,
                          weak_ptr_factory_.GetWeakPtr()));
  network_observation_.Observe(
      GetApplicationContext()->GetNetworkConnectionTracker());
}

ReadingListDownloadService::~ReadingListDownloadService() = default;

void ReadingListDownloadService::Initialize() {
  model_observation_.Observe(reading_list_model_.get());
}

base::FilePath ReadingListDownloadService::OfflineRoot() const {
  return reading_list::OfflineRootDirectoryPath(chrome_profile_path_);
}

void ReadingListDownloadService::Shutdown() {
  model_observation_.Reset();
  network_observation_.Reset();
}

void ReadingListDownloadService::ReadingListModelLoaded(
    const ReadingListModel* model) {
  DCHECK_EQ(reading_list_model_, model);
  SyncWithModel();
}

void ReadingListDownloadService::ReadingListWillRemoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  DCHECK_EQ(reading_list_model_, model);
  DCHECK(model->GetEntryByURL(url));
  RemoveDownloadedEntry(url);
}

void ReadingListDownloadService::ReadingListDidAddEntry(
    const ReadingListModel* model,
    const GURL& url,
    reading_list::EntrySource source) {
  DCHECK_EQ(reading_list_model_, model);
  ProcessNewEntry(url);
}

void ReadingListDownloadService::ReadingListDidMoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  DCHECK_EQ(reading_list_model_, model);
  ProcessNewEntry(url);
}

void ReadingListDownloadService::Clear() {
  distiller_page_factory_->ReleaseAllRetainedWebState();
}

void ReadingListDownloadService::ProcessNewEntry(const GURL& url) {
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model_->GetEntryByURL(url);
  if (!entry || entry->IsRead()) {
    url_downloader_->CancelDownloadOfflineURL(url);
  } else {
    ScheduleDownloadEntry(url);
  }
}

void ReadingListDownloadService::SyncWithModel() {
  DCHECK(reading_list_model_->loaded());
  std::set<std::string> processed_directories;
  std::set<GURL> unprocessed_entries;
  for (const auto& url : reading_list_model_->GetKeys()) {
    scoped_refptr<const ReadingListEntry> entry =
        reading_list_model_->GetEntryByURL(url);
    switch (entry->DistilledState()) {
      case ReadingListEntry::PROCESSED:
        processed_directories.insert(reading_list::OfflineURLDirectoryID(url));
        break;
      case ReadingListEntry::WAITING:
      case ReadingListEntry::PROCESSING:
      case ReadingListEntry::WILL_RETRY:
        unprocessed_entries.insert(url);
        break;
      case ReadingListEntry::DISTILLATION_ERROR:
        break;
    }
  }
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&::CleanUpFiles, OfflineRoot(), processed_directories),
      base::BindOnce(&ReadingListDownloadService::DownloadUnprocessedEntries,
                     weak_ptr_factory_.GetWeakPtr(), unprocessed_entries));
}

void ReadingListDownloadService::DownloadUnprocessedEntries(
    const std::set<GURL>& unprocessed_entries) {
  for (const GURL& url : unprocessed_entries) {
    this->ScheduleDownloadEntry(url);
  }
}

void ReadingListDownloadService::ScheduleDownloadEntry(const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model_->GetEntryByURL(url);
  if (!entry ||
      entry->DistilledState() == ReadingListEntry::DISTILLATION_ERROR ||
      entry->DistilledState() == ReadingListEntry::PROCESSED ||
      entry->IsRead()) {
    return;
  }
  GURL local_url(url);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ReadingListDownloadService::DownloadEntry,
                     weak_ptr_factory_.GetWeakPtr(), local_url),
      entry->TimeUntilNextTry());
}

void ReadingListDownloadService::DownloadEntry(const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model_->GetEntryByURL(url);
  if (!entry ||
      entry->DistilledState() == ReadingListEntry::DISTILLATION_ERROR ||
      entry->DistilledState() == ReadingListEntry::PROCESSED ||
      entry->IsRead()) {
    return;
  }

  if (net::NetworkChangeNotifier::IsOffline()) {
    // There is no connection, save it for download only if we did not exceed
    // the maximaxum number of tries.
    if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeWifiOnly) {
      url_to_download_cellular_.push_back(entry->URL());
    }
    if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeStop) {
      url_to_download_wifi_.push_back(entry->URL());
    }
    return;
  }

  // There is a connection.
  if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeWifiOnly) {
    // Try to download the page, whatever the connection.
    reading_list_model_->SetEntryDistilledStateIfExists(
        entry->URL(), ReadingListEntry::PROCESSING);
    url_downloader_->DownloadOfflineURL(entry->URL());

  } else if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeStop) {
    // Try to download the page only if the connection is wifi.
    auto connection_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
    // GetConnectionType will return false if the type isn't known yet, and
    // connection_type will be unchanged, so we can ignore the return value and
    // let this treat the connection as non-wifi.
    GetApplicationContext()->GetNetworkConnectionTracker()->GetConnectionType(
        &connection_type,
        base::BindOnce(&ReadingListDownloadService::OnConnectionChanged,
                       weak_ptr_factory_.GetWeakPtr()));
    if (connection_type == network::mojom::ConnectionType::CONNECTION_WIFI) {
      // The connection is wifi, download the page.
      reading_list_model_->SetEntryDistilledStateIfExists(
          entry->URL(), ReadingListEntry::PROCESSING);
      url_downloader_->DownloadOfflineURL(entry->URL());

    } else {
      // The connection is not wifi, save it for download when the connection
      // changes to wifi.
      url_to_download_wifi_.push_back(entry->URL());
    }
  }
}

void ReadingListDownloadService::RemoveDownloadedEntry(const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  url_downloader_->RemoveOfflineURL(url);
}

void ReadingListDownloadService::OnDownloadEnd(
    const GURL& url,
    const GURL& distilled_url,
    URLDownloader::SuccessState success,
    const base::FilePath& distilled_path,
    int64_t size,
    const std::string& title) {
  DCHECK(reading_list_model_->loaded());
  URLDownloader::SuccessState real_success_value = success;
  if (distilled_path.empty()) {
    real_success_value = URLDownloader::ERROR;
  }
  switch (real_success_value) {
    case URLDownloader::DOWNLOAD_SUCCESS:
    case URLDownloader::DOWNLOAD_EXISTS: {
      reading_list_model_->SetEntryDistilledInfoIfExists(
          url, distilled_path, distilled_url, size, base::Time::Now());

      std::string trimmed_title = base::CollapseWhitespaceASCII(title, false);
      if (!trimmed_title.empty()) {
        reading_list_model_->SetEntryTitleIfExists(url, trimmed_title);
      }

      scoped_refptr<const ReadingListEntry> entry =
          reading_list_model_->GetEntryByURL(url);
      if (entry) {
        UMA_HISTOGRAM_COUNTS_100("ReadingList.Download.Failures",
                                 entry->FailedDownloadCounter());
      }
      UMA_HISTOGRAM_ENUMERATION("ReadingList.Download.Status", SUCCESS,
                                STATUS_MAX);
      break;
    }
    case URLDownloader::ERROR:
    case URLDownloader::PERMANENT_ERROR: {
      scoped_refptr<const ReadingListEntry> entry =
          reading_list_model_->GetEntryByURL(url);
      // Add this failure to the total failure count.
      if (entry && real_success_value == URLDownloader::ERROR &&
          entry->FailedDownloadCounter() + 1 < kNumberOfFailsBeforeStop) {
        reading_list_model_->SetEntryDistilledStateIfExists(
            url, ReadingListEntry::WILL_RETRY);
        ScheduleDownloadEntry(url);
        UMA_HISTOGRAM_ENUMERATION("ReadingList.Download.Status", RETRY,
                                  STATUS_MAX);
      } else {
        UMA_HISTOGRAM_ENUMERATION("ReadingList.Download.Status", FAILURE,
                                  STATUS_MAX);
        reading_list_model_->SetEntryDistilledStateIfExists(
            url, ReadingListEntry::DISTILLATION_ERROR);
      }
      break;
    }
  }
}

void ReadingListDownloadService::OnDeleteEnd(const GURL& url, bool success) {
  // Nothing to update as this is only called when deleting reading list entries
}

void ReadingListDownloadService::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type == network::mojom::ConnectionType::CONNECTION_NONE) {
    had_connection_ = false;
    return;
  }

  if (!had_connection_) {
    had_connection_ = true;
    for (auto& url : url_to_download_cellular_) {
      ScheduleDownloadEntry(url);
    }
  }
  if (type == network::mojom::ConnectionType::CONNECTION_WIFI) {
    for (auto& url : url_to_download_wifi_) {
      ScheduleDownloadEntry(url);
    }
  }
}
