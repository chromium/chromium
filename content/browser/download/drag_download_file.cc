// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/drag_download_file.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_stats.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_request_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

namespace {

typedef base::Callback<void(bool)> OnCompleted;

}  // namespace

// Both DragDownloadFile and DragDownloadFileUI run on the UI thread.
class DragDownloadFile::DragDownloadFileUI
    : public download::DownloadItem::Observer {
 public:
  DragDownloadFileUI(const GURL& url,
                     const Referrer& referrer,
                     const std::string& referrer_encoding,
                     WebContents* web_contents,
                     const OnCompleted& on_completed)
      : on_completed_(on_completed),
        url_(url),
        referrer_(referrer),
        referrer_encoding_(referrer_encoding),
        web_contents_(web_contents),
        download_item_(nullptr) {
    DCHECK(!on_completed_.is_null());
    DCHECK(web_contents_);
    // May be called on any thread.
    // Do not call weak_ptr_factory_.GetWeakPtr() outside the UI thread.
  }

  void InitiateDownload(base::File file,
                        const base::FilePath& file_path) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // TODO(https://crbug.com/614134) This should use the frame actually
    // containing the link being dragged rather than the main frame of the tab.
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("drag_download_file", R"(
        semantics {
          sender: "Drag To Download"
          description:
            "Users can download files by dragging them out of browser and into "
            "a disk related area."
          trigger: "When user drags a file from the browser."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but it is only "
            "activated by direct user action."
          chrome_policy {
            DownloadRestrictions {
              DownloadRestrictions: 3
            }
          }
        })");
    std::unique_ptr<download::DownloadUrlParameters> params(
        DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
            web_contents_, url_, traffic_annotation));
    params->set_referrer(referrer_.url);
    params->set_referrer_policy(
        Referrer::ReferrerPolicyForUrlRequest(referrer_.policy));
    params->set_referrer_encoding(referrer_encoding_);
    params->set_callback(base::BindOnce(&DragDownloadFileUI::OnDownloadStarted,
                                        weak_ptr_factory_.GetWeakPtr()));
    params->set_file_path(file_path);
    params->set_file(std::move(file));  // Nulls file.
    params->set_download_source(download::DownloadSource::DRAG_AND_DROP);
    BrowserContext::GetDownloadManager(web_contents_->GetBrowserContext())
        ->DownloadUrl(std::move(params));
  }

  void Cancel() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (download_item_)
      download_item_->Cancel(true);
  }

  void Delete() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    delete this;
  }

 private:
  ~DragDownloadFileUI() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (download_item_)
      download_item_->RemoveObserver(this);
  }

  void OnDownloadStarted(download::DownloadItem* item,
                         download::DownloadInterruptReason interrupt_reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!item || item->GetState() != download::DownloadItem::IN_PROGRESS) {
      DCHECK(!item ||
             item->GetLastReason() != download::DOWNLOAD_INTERRUPT_REASON_NONE);
      base::PostTask(FROM_HERE, {BrowserThread::UI},
                     base::BindOnce(on_completed_, false));
      return;
    }
    DCHECK_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE, interrupt_reason);
    download_item_ = item;
    download_item_->AddObserver(this);
  }

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* item) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(download_item_, item);
    download::DownloadItem::DownloadState state = download_item_->GetState();
    if (state == download::DownloadItem::COMPLETE ||
        state == download::DownloadItem::CANCELLED ||
        state == download::DownloadItem::INTERRUPTED) {
      if (!on_completed_.is_null()) {
        base::PostTask(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(on_completed_,
                           state == download::DownloadItem::COMPLETE));
        on_completed_.Reset();
      }
      download_item_->RemoveObserver(this);
      download_item_ = nullptr;
    }
    // Ignore other states.
  }

  void OnDownloadDestroyed(download::DownloadItem* item) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(download_item_, item);
    if (!on_completed_.is_null()) {
      const bool is_complete =
          download_item_->GetState() == download::DownloadItem::COMPLETE;
      base::PostTask(FROM_HERE, {BrowserThread::UI},
                     base::BindOnce(on_completed_, is_complete));
      on_completed_.Reset();
    }
    download_item_->RemoveObserver(this);
    download_item_ = nullptr;
  }

  OnCompleted on_completed_;
  GURL url_;
  Referrer referrer_;
  std::string referrer_encoding_;
  WebContents* web_contents_;
  download::DownloadItem* download_item_;

  // Only used in the callback from DownloadManager::DownloadUrl().
  base::WeakPtrFactory<DragDownloadFileUI> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DragDownloadFileUI);
};

DragDownloadFile::DragDownloadFile(const base::FilePath& file_path,
                                   base::File file,
                                   const GURL& url,
                                   const Referrer& referrer,
                                   const std::string& referrer_encoding,
                                   WebContents* web_contents)
    : file_path_(file_path),
      file_(std::move(file)),
      state_(INITIALIZED),
      drag_ui_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  drag_ui_ =
      new DragDownloadFileUI(url, referrer, referrer_encoding, web_contents,
                             base::Bind(&DragDownloadFile::DownloadCompleted,
                                        weak_ptr_factory_.GetWeakPtr()));
  DCHECK(!file_path_.empty());
}

DragDownloadFile::~DragDownloadFile() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This is the only place that drag_ui_ can be deleted from. Post a message to
  // the UI thread so that it calls RemoveObserver on the right thread, and so
  // that this task will run after the InitiateDownload task runs on the UI
  // thread.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DragDownloadFileUI::Delete, base::Unretained(drag_ui_)));
  drag_ui_ = nullptr;
}

void DragDownloadFile::Start(ui::DownloadFileObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (state_ != INITIALIZED)
    return;
  state_ = STARTED;

  DCHECK(!observer_.get());
  observer_ = observer;
  DCHECK(observer_.get());

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DragDownloadFileUI::InitiateDownload,
                     base::Unretained(drag_ui_), std::move(file_), file_path_));
}

bool DragDownloadFile::Wait() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  if (state_ == STARTED)
    nested_loop_.Run();
  DCHECK(weak_ptr);
  return state_ == SUCCESS;
}

void DragDownloadFile::Stop() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (drag_ui_) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&DragDownloadFileUI::Cancel,
                                  base::Unretained(drag_ui_)));
  }
}

void DragDownloadFile::DownloadCompleted(bool is_successful) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  state_ = is_successful ? SUCCESS : FAILURE;

  scoped_refptr<ui::DownloadFileObserver> file_observer = observer_;
  // Release the observer since we do not need it any more.
  observer_ = nullptr;
  if (nested_loop_.running())
    nested_loop_.Quit();

  if (is_successful)
    file_observer->OnDownloadCompleted(file_path_);
  else
    file_observer->OnDownloadAborted();
}

}  // namespace content
