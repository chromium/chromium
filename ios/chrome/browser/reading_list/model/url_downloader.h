// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_URL_DOWNLOADER_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_URL_DOWNLOADER_H_

#import <string>

#import "base/containers/circular_deque.h"
#import "base/files/file_path.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/task/cancelable_task_tracker.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_viewer.h"
#import "ios/chrome/browser/reading_list/model/reading_list_distiller_page.h"

class PrefService;
class GURL;
namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace reading_list {
class ReadingListDistillerPageFactory;
}

// This class downloads and deletes offline versions of URLs.
// If the URL points to an HTML file, `URLDownloader` uses DOM distiller to
// fetch the page and simplify it.
// If the URL points to a PDF file, the PDF is simply downloaded and saved to
// the disk.
// Only one item is downloaded or deleted at a time using a queue of tasks that
// are handled sequentially. Items (page + images) are saved to individual
// folders within an offline folder, using md5 hashing to create unique file
// names. When a deletion is requested, all previous downloads for that URL are
// cancelled as they would be deleted.
class URLDownloader : reading_list::ReadingListDistillerPageDelegate {
  friend class MockURLDownloader;

 public:
  // And enum indicating different download outcomes.
  enum SuccessState {
    // The URL was correctly downloaded and an offline version is not available.
    DOWNLOAD_SUCCESS,
    // The URL was already available offline. No action was done.
    DOWNLOAD_EXISTS,
    // The URL could not be downloaded because of an error. Client may want to
    // try again later.
    ERROR,
    // The URL could not be downloaded because of an error. Client should not
    // try again.
    PERMANENT_ERROR,
  };

  // A completion callback that takes a GURL and a bool indicating the
  // outcome and returns void.
  using SuccessCompletion = base::RepeatingCallback<void(const GURL&, bool)>;

  // A download completion callback that takes, in order, the GURL that was
  // downloaded, the GURL of the page that was downloaded after redirections, a
  // SuccessState indicating the outcome of the download, the path to the
  // downloaded page (relative to `OfflineRootDirectoryPath()`, and the title of
  // the url, and returns void.
  // The path to downloaded file and title should not be used in case of
  // failure.
  using DownloadCompletion = base::RepeatingCallback<void(const GURL&,
                                                          const GURL&,
                                                          SuccessState,
                                                          const base::FilePath&,
                                                          int64_t size,
                                                          const std::string&)>;

  // Create a URL downloader with completion callbacks for downloads and
  // deletions. The completion callbacks will be called with the original url
  // and a boolean indicating success. For downloads, if distillation was
  // successful, it will also include the distilled url and extracted title.
  URLDownloader(
      dom_distiller::DistillerFactory* distiller_factory,
      reading_list::ReadingListDistillerPageFactory* distiller_page_factory,
      PrefService* prefs,
      base::FilePath chrome_profile_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const DownloadCompletion& download_completion,
      const SuccessCompletion& delete_completion);

  URLDownloader(const URLDownloader&) = delete;
  URLDownloader& operator=(const URLDownloader&) = delete;

  ~URLDownloader() override;

  // Asynchronously download an offline version of the URL.
  void DownloadOfflineURL(const GURL& url);

  // Cancels the download job an offline version of the URL.
  void CancelDownloadOfflineURL(const GURL& url);

  // Asynchronously remove the offline version of the URL if it exists.
  void RemoveOfflineURL(const GURL& url);

  // URL loader completion callback.
  void OnURLLoadComplete(const GURL& original_url,
                         base::FilePath response_path);

  // Cancels the current download task.
  void CancelTask();

 private:
  enum TaskType { DELETE, DOWNLOAD };
  using Task = std::pair<TaskType, GURL>;

  // Calls callback with true if an offline path exists. `path` must be
  // absolute.
  void OfflinePathExists(const base::FilePath& url,
                         base::OnceCallback<void(bool)> callback);
  // Handles the next task in the queue, if no task is currently being handled.
  void HandleNextTask();
  // Callback for completed (or failed) download, handles calling
  // downloadCompletion on a PDF or an HTML file. Unwind result and calls
  // DownloadCompletionHandler.
  void DownloadPDFOrHTMLCompletionHandler(
      const GURL& url,
      const std::string& title,
      const base::FilePath& offline_path,
      std::pair<SuccessState, int64_t> result);
  // Callback for completed (or failed) download, handles calling
  // downloadCompletion and starting the next task.
  void DownloadCompletionHandler(const GURL& url,
                                 const std::string& title,
                                 const base::FilePath& path,
                                 SuccessState success);
  // Callback for completed (or failed) deletion.
  void PostDelete(const GURL& url,
                  const std::string& title,
                  const base::FilePath& offline_path,
                  SuccessState success);
  // Callback for completed (or failed) deletion, handles calling
  // deleteCompletion and starting the next task.
  void DeleteCompletionHandler(const GURL& url, bool success);

  // Downloads `url`, depending on `offlineURLExists` state.
  virtual void DownloadURL(const GURL& url, bool offlineURLExists);

  // ReadingListDistillerPageDelegate methods
  void DistilledPageRedirectedToURL(const GURL& original_url,
                                    const GURL& final_url) override;
  void DistilledPageHasMimeType(const GURL& original_url,
                                const std::string& mime_type) override;

  // Callback for distillation completion.
  void DistillerCallback(
      const GURL& pageURL,
      const std::string& html,
      const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
          images,
      const std::string& title,
      const std::string& csp_nonce);

  // PDF processing methods

  // Starts fetching the PDF file. If `original_url_` triggered a redirection,
  // directly save `distilled_url_`.
  virtual void FetchPDFFile();

  raw_ptr<reading_list::ReadingListDistillerPageFactory>
      distiller_page_factory_;
  raw_ptr<dom_distiller::DistillerFactory> distiller_factory_;
  raw_ptr<PrefService> pref_service_;
  const DownloadCompletion download_completion_;
  const SuccessCompletion delete_completion_;

  base::circular_deque<Task> tasks_;
  bool working_;
  base::FilePath base_directory_;
  GURL original_url_;
  GURL distilled_url_;
  int64_t saved_size_;
  std::string mime_type_;
  // URL loader used to redownload the document and save it in the sandbox.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  // URLLoaderFactory needed for the URLLoader.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<dom_distiller::DistillerViewerInterface> distiller_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::CancelableTaskTracker task_tracker_;

  base::WeakPtrFactory<URLDownloader> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_URL_DOWNLOADER_H_
