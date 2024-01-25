// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DOWNLOAD_SERVICE_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DOWNLOAD_SERVICE_H_

#include <string>

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "ios/chrome/browser/reading_list/model/url_downloader.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class GURL;
class PrefService;
class ReadingListModel;
namespace base {
class FilePath;
}

namespace reading_list {
class ReadingListDistillerPageFactory;
}

// Observes the reading list and downloads offline versions of its articles.
// Any calls made to DownloadEntry before the model is loaded will be ignored.
// When the model is loaded, offline directory is automatically synced with the
// entries in the model.
class ReadingListDownloadService
    : public KeyedService,
      public ReadingListModelObserver,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  ReadingListDownloadService(
      ReadingListModel* reading_list_model,
      PrefService* prefs,
      base::FilePath chrome_profile_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<dom_distiller::DistillerFactory> distiller_factory,
      std::unique_ptr<reading_list::ReadingListDistillerPageFactory>
          distiller_page_factory);

  ReadingListDownloadService(const ReadingListDownloadService&) = delete;
  ReadingListDownloadService& operator=(const ReadingListDownloadService&) =
      delete;

  ~ReadingListDownloadService() override;

  // Initializes the reading list download service.
  void Initialize();

  // Clear the current download queue.
  void Clear();

  // The root folder containing all the offline files.
  virtual base::FilePath OfflineRoot() const;

  // KeyedService implementation.
  void Shutdown() override;

  // ReadingListModelObserver implementation
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListWillRemoveEntry(const ReadingListModel* model,
                                  const GURL& url) override;
  void ReadingListDidAddEntry(const ReadingListModel* model,
                              const GURL& url,
                              reading_list::EntrySource entry_source) override;
  void ReadingListDidMoveEntry(const ReadingListModel* model,
                               const GURL& url) override;

 private:
  // Checks the model and determines which entries are processed and which
  // entries need to be processed.
  // Initiates a cleanup of `OfflineRoot()` directory removing sub_directories
  // not corresponding to a processed ReadingListEntry.
  // Schedules unprocessed entries for distillation.
  void SyncWithModel();
  // Schedules all entries in `unprocessed_entries` for distillation.
  void DownloadUnprocessedEntries(const std::set<GURL>& unprocessed_entries);
  // Processes a new entry and schedules a download if needed.
  void ProcessNewEntry(const GURL& url);
  // Schedules a download of an offline version of the reading list entry,
  // according to the delay of the entry. Must only be called after reading list
  // model is loaded.
  void ScheduleDownloadEntry(const GURL& url);
  // Tries to save an offline version of the reading list entry if it is not yet
  // saved. Must only be called after reading list model is loaded.
  void DownloadEntry(const GURL& url);
  // Removes the offline version of the reading list entry if it exists. Must
  // only be called after reading list model is loaded.
  void RemoveDownloadedEntry(const GURL& url);
  // Callback for entry download.
  void OnDownloadEnd(const GURL& url,
                     const GURL& distilled_url,
                     URLDownloader::SuccessState success,
                     const base::FilePath& distilled_path,
                     int64_t size,
                     const std::string& title);

  // Callback for entry deletion.
  void OnDeleteEnd(const GURL& url, bool success);

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  raw_ptr<ReadingListModel> reading_list_model_;
  base::FilePath chrome_profile_path_;
  std::unique_ptr<URLDownloader> url_downloader_;
  std::vector<GURL> url_to_download_cellular_;
  std::vector<GURL> url_to_download_wifi_;
  bool had_connection_;
  std::unique_ptr<reading_list::ReadingListDistillerPageFactory>
      distiller_page_factory_;
  std::unique_ptr<dom_distiller::DistillerFactory> distiller_factory_;

  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      model_observation_{this};
  base::ScopedObservation<
      network::NetworkConnectionTracker,
      network::NetworkConnectionTracker::NetworkConnectionObserver>
      network_observation_{this};

  base::WeakPtrFactory<ReadingListDownloadService> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DOWNLOAD_SERVICE_H_
