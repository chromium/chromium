// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/url_downloader.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "components/reading_list/core/offline_url_utils.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/dom_distiller/distiller_viewer.h"
#include "ios/chrome/browser/reading_list/reading_list_distiller_page.h"
#include "ios/chrome/browser/reading_list/reading_list_distiller_page_factory.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {
// This script disables context menu on img elements.
// The pages are stored locally and long pressing on them will trigger a context
// menu on the file:// URL which cannot be opened. Disable the context menu.
const char kDisableImageContextMenuScript[] =
    "<script>"
    "document.addEventListener('DOMContentLoaded', function (event) {"
    "    var imgMenuDisabler = document.createElement('style');"
    "    imgMenuDisabler.innerHTML = 'img { -webkit-touch-callout: none; }';"
    "    document.head.appendChild(imgMenuDisabler);"
    "}, false);"
    "</script>";
}  // namespace

// URLDownloader

URLDownloader::URLDownloader(
    dom_distiller::DistillerFactory* distiller_factory,
    reading_list::ReadingListDistillerPageFactory* distiller_page_factory,
    PrefService* prefs,
    base::FilePath chrome_profile_path,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const DownloadCompletion& download_completion,
    const SuccessCompletion& delete_completion)
    : distiller_page_factory_(distiller_page_factory),
      distiller_factory_(distiller_factory),
      pref_service_(prefs),
      download_completion_(download_completion),
      delete_completion_(delete_completion),
      working_(false),
      base_directory_(chrome_profile_path),
      mime_type_(),
      url_loader_factory_(std::move(url_loader_factory)),
      task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      task_tracker_() {}

URLDownloader::~URLDownloader() {
  task_tracker_.TryCancelAll();
}

void URLDownloader::OfflinePathExists(const base::FilePath& path,
                                      base::Callback<void(bool)> callback) {
  task_tracker_.PostTaskAndReplyWithResult(task_runner_.get(), FROM_HERE,
                                           base::Bind(&base::PathExists, path),
                                           callback);
}

void URLDownloader::RemoveOfflineURL(const GURL& url) {
  // Remove all download tasks for this url as it would be pointless work.
  CancelDownloadOfflineURL(url);
  tasks_.push_back(std::make_pair(DELETE, url));
  HandleNextTask();
}

void URLDownloader::DownloadOfflineURL(const GURL& url) {
  if (!base::Contains(tasks_, std::make_pair(DOWNLOAD, url))) {
    tasks_.push_back(std::make_pair(DOWNLOAD, url));
    HandleNextTask();
  }
}

void URLDownloader::CancelDownloadOfflineURL(const GURL& url) {
  tasks_.erase(
      std::remove(tasks_.begin(), tasks_.end(), std::make_pair(DOWNLOAD, url)),
      tasks_.end());
}

void URLDownloader::DownloadCompletionHandler(
    const GURL& url,
    const std::string& title,
    const base::FilePath& offline_path,
    SuccessState success) {
  DCHECK(working_);

  auto post_delete = base::Bind(
      [](URLDownloader* _this, const GURL& url, const std::string& title,
         const base::FilePath& offline_path, SuccessState success) {
        _this->download_completion_.Run(url, _this->distilled_url_, success,
                                        offline_path, _this->saved_size_,
                                        title);
        _this->distiller_.reset();
        _this->working_ = false;
        _this->HandleNextTask();
      },
      base::Unretained(this), url, title, offline_path, success);

  // If downloading failed, clean up any partial download.
  if (success == ERROR) {
    base::FilePath directory_path =
        reading_list::OfflineURLDirectoryAbsolutePath(base_directory_, url);
    task_tracker_.PostTaskAndReply(
        task_runner_.get(), FROM_HERE,
        base::Bind(
            [](const base::FilePath& offline_directory_path) {
              base::DeleteFile(offline_directory_path, true);
            },
            directory_path),
        post_delete);
  } else {
    post_delete.Run();
  }
}

void URLDownloader::DeleteCompletionHandler(const GURL& url, bool success) {
  DCHECK(working_);
  delete_completion_.Run(url, success);
  working_ = false;
  HandleNextTask();
}

void URLDownloader::HandleNextTask() {
  if (working_ || tasks_.empty()) {
    return;
  }
  working_ = true;

  Task task = tasks_.front();
  tasks_.pop_front();
  GURL url = task.second;
  base::FilePath directory_path =
      reading_list::OfflineURLDirectoryAbsolutePath(base_directory_, url);

  if (task.first == DELETE) {
    task_tracker_.PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::Bind(&base::DeleteFile, directory_path, true),
        base::Bind(&URLDownloader::DeleteCompletionHandler,
                   base::Unretained(this), url));
  } else if (task.first == DOWNLOAD) {
    DCHECK(!distiller_);
    OfflinePathExists(directory_path, base::Bind(&URLDownloader::DownloadURL,
                                                 base::Unretained(this), url));
  }
}

void URLDownloader::DownloadURL(const GURL& url, bool offline_url_exists) {
  if (offline_url_exists) {
    DownloadCompletionHandler(url, std::string(), base::FilePath(),
                              DOWNLOAD_EXISTS);
    return;
  }

  original_url_ = url;
  distilled_url_ = url;
  saved_size_ = 0;
  std::unique_ptr<reading_list::ReadingListDistillerPage>
      reading_list_distiller_page =
          distiller_page_factory_->CreateReadingListDistillerPage(url, this);

  distiller_.reset(new dom_distiller::DistillerViewer(
      distiller_factory_, std::move(reading_list_distiller_page), pref_service_,
      url,
      base::Bind(&URLDownloader::DistillerCallback, base::Unretained(this))));
}

void URLDownloader::DistilledPageRedirectedToURL(const GURL& page_url,
                                                 const GURL& redirected_url) {
  DCHECK(original_url_ == page_url);
  distilled_url_ = redirected_url;
}

void URLDownloader::DistilledPageHasMimeType(const GURL& original_url,
                                             const std::string& mime_type) {
  DCHECK(original_url_ == original_url);
  mime_type_ = mime_type;
}

void URLDownloader::OnURLLoadComplete(const GURL& original_url,
                                      base::FilePath response_path) {
  // At the moment, only pdf files are downloaded using URLFetcher.
  DCHECK(mime_type_ == "application/pdf");
  base::FilePath path = reading_list::OfflinePagePath(
      original_url_, reading_list::OFFLINE_TYPE_PDF);
  std::string mime_type;
  if (url_loader_->ResponseInfo()) {
    mime_type = url_loader_->ResponseInfo()->mime_type;
  }
  if (response_path.empty() || mime_type != mime_type_) {
    return DownloadCompletionHandler(original_url_, "", path, ERROR);
  }

  task_tracker_.PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::Bind(&URLDownloader::SavePDFFile, base::Unretained(this),
                 response_path),
      base::Bind(&URLDownloader::DownloadCompletionHandler,
                 base::Unretained(this), original_url, "", path));

  url_loader_.reset();
}

void URLDownloader::CancelTask() {
  task_tracker_.TryCancelAll();
  distiller_.reset();
}

void URLDownloader::FetchPDFFile() {
  const GURL& pdf_url =
      distilled_url_.is_valid() ? distilled_url_ : original_url_;
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = pdf_url;
  resource_request->load_flags = net::LOAD_SKIP_CACHE_VALIDATION;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 NO_TRAFFIC_ANNOTATION_YET);
  url_loader_->DownloadToTempFile(
      url_loader_factory_.get(),
      base::BindOnce(&URLDownloader::OnURLLoadComplete, base::Unretained(this),
                     pdf_url));
}

URLDownloader::SuccessState URLDownloader::SavePDFFile(
    const base::FilePath& temporary_path) {
  if (CreateOfflineURLDirectory(original_url_)) {
    base::FilePath path = reading_list::OfflinePagePath(
        original_url_, reading_list::OFFLINE_TYPE_PDF);
    base::FilePath absolute_path =
        reading_list::OfflineURLAbsolutePathFromRelativePath(base_directory_,
                                                             path);

    if (base::Move(temporary_path, absolute_path)) {
      int64_t pdf_file_size;
      base::GetFileSize(absolute_path, &pdf_file_size);
      saved_size_ += pdf_file_size;
      return DOWNLOAD_SUCCESS;
    } else {
      return ERROR;
    }
  }

  return ERROR;
}

void URLDownloader::DistillerCallback(
    const GURL& page_url,
    const std::string& html,
    const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
        images,
    const std::string& title) {
  if (html.empty()) {
    // The page may not be HTML. Check the mime-type to see if another handler
    // can save offline content.
    if (mime_type_ == "application/pdf") {
      // PDF handler just downloads the PDF file.
      FetchPDFFile();
      return;
    }
    // This content cannot be processed, return an error value to the client.
    DownloadCompletionHandler(page_url, std::string(), base::FilePath(), ERROR);
    return;
  }

  std::vector<dom_distiller::DistillerViewer::ImageInfo> images_block = images;
  std::string block_html = html;
  task_tracker_.PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::Bind(&URLDownloader::SaveDistilledHTML, base::Unretained(this),
                 page_url, images_block, block_html),
      base::Bind(&URLDownloader::DownloadCompletionHandler,
                 base::Unretained(this), page_url, title,
                 reading_list::OfflinePagePath(
                     page_url, reading_list::OFFLINE_TYPE_HTML)));
}

URLDownloader::SuccessState URLDownloader::SaveDistilledHTML(
    const GURL& url,
    const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
        images,
    const std::string& html) {
  if (CreateOfflineURLDirectory(url)) {
    return SaveHTMLForURL(SaveAndReplaceImagesInHTML(url, html, images), url)
               ? DOWNLOAD_SUCCESS
               : ERROR;
  }
  return ERROR;
}

bool URLDownloader::CreateOfflineURLDirectory(const GURL& url) {
  base::FilePath directory_path =
      reading_list::OfflineURLDirectoryAbsolutePath(base_directory_, url);
  if (!DirectoryExists(directory_path)) {
    return CreateDirectoryAndGetError(directory_path, nil);
  }
  return true;
}

bool URLDownloader::SaveImage(const GURL& url,
                              const GURL& image_url,
                              const std::string& data,
                              std::string* image_name) {
  std::string image_hash = base::MD5String(image_url.spec());
  *image_name = image_hash;
  base::FilePath directory_path =
      reading_list::OfflineURLDirectoryAbsolutePath(base_directory_, url);
  base::FilePath path = directory_path.Append(image_hash);
  if (!base::PathExists(path)) {
    int written = base::WriteFile(path, data.c_str(), data.length());
    if (written <= 0) {
      return false;
    }
    saved_size_ += written;
    return true;
  }
  return true;
}

std::string URLDownloader::SaveAndReplaceImagesInHTML(
    const GURL& url,
    const std::string& html,
    const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
        images) {
  std::string mutable_html = html;
  bool local_images_found = false;
  for (size_t i = 0; i < images.size(); i++) {
    if (images[i].url.SchemeIs(url::kDataScheme)) {
      // Data URI, the data part of the image is empty, no need to store it.
      continue;
    }
    std::string local_image_name;
    // Mixed content is HTTP images on HTTPS pages.
    bool image_is_mixed_content = distilled_url_.SchemeIsCryptographic() &&
                                  !images[i].url.SchemeIsCryptographic();
    // Only save images if it is not mixed content and image data is valid.
    if (!image_is_mixed_content && images[i].url.is_valid() &&
        !images[i].data.empty()) {
      if (!SaveImage(url, images[i].url, images[i].data, &local_image_name)) {
        return std::string();
      }
    }
    std::string image_url = net::EscapeForHTML(images[i].url.spec());
    size_t image_url_size = image_url.size();
    size_t pos = mutable_html.find(image_url, 0);
    while (pos != std::string::npos) {
      local_images_found = true;
      mutable_html.replace(pos, image_url_size, local_image_name);
      pos = mutable_html.find(image_url, pos + local_image_name.size());
    }
  }
  if (local_images_found) {
    mutable_html += kDisableImageContextMenuScript;
  }

  return mutable_html;
}

bool URLDownloader::SaveHTMLForURL(std::string html, const GURL& url) {
  if (html.empty()) {
    return false;
  }
  base::FilePath path = reading_list::OfflineURLAbsolutePathFromRelativePath(
      base_directory_,
      reading_list::OfflinePagePath(url, reading_list::OFFLINE_TYPE_HTML));
  int written = base::WriteFile(path, html.c_str(), html.length());
  if (written <= 0) {
    return false;
  }
  saved_size_ += written;
  return true;
}
