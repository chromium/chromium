// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/drag_download_file.h"

#include <utility>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_stats.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

namespace {

using OnCompleted = base::OnceCallback<void(bool)>;

}  // namespace

// Both DragDownloadFile and DragDownloadFileUI run on the UI thread.
class DragDownloadFile::DragDownloadFileUI
    : public download::DownloadItem::Observer {
 public:
  DragDownloadFileUI(WeakDocumentPtr source_document,
                     const GURL& url,
                     const Referrer& referrer,
                     const std::string& referrer_encoding,
                     OnCompleted on_completed)
      : on_completed_(std::move(on_completed)),
        source_document_(std::move(source_document)),
        url_(url),
        referrer_(referrer),
        referrer_encoding_(referrer_encoding) {
    CHECK(on_completed_, base::NotFatalUntil::M159);
    // May be called on any thread.
    // Do not call weak_ptr_factory_.GetWeakPtr() outside the UI thread.
  }

  DragDownloadFileUI(const DragDownloadFileUI&) = delete;
  DragDownloadFileUI& operator=(const DragDownloadFileUI&) = delete;

  void InitiateDownload(base::File file,
                        const base::FilePath& file_path) {
    CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

    RenderFrameHost* host = source_document_.AsRenderFrameHostIfValid();
    if (!host)
      return;
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
    auto params = host->CreateDownloadUrlParameters(url_, traffic_annotation);
    params->set_referrer(referrer_.url);
    params->set_referrer_policy(
        Referrer::ReferrerPolicyForUrlRequest(referrer_.policy));
    params->set_referrer_encoding(referrer_encoding_);

    params->set_initiator(host->GetLastCommittedOrigin());

    params->set_callback(base::BindOnce(&DragDownloadFileUI::OnDownloadStarted,
                                        weak_ptr_factory_.GetWeakPtr()));
    params->set_file_path(file_path);
    params->set_file(std::move(file));  // Nulls file.
    params->set_download_source(download::DownloadSource::DRAG_AND_DROP);
    host->GetBrowserContext()->GetDownloadManager()->DownloadUrl(
        std::move(params));
  }

  void Cancel() {
    CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
    if (download_item_)
      download_item_->Cancel(true);
  }

  void Delete() {
    CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
    delete this;
  }

 private:
  ~DragDownloadFileUI() override {
    CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
    if (download_item_)
      download_item_->RemoveObserver(this);
  }

  void OnDownloadStarted(download::DownloadItem* item,
                         download::DownloadInterruptReason interrupt_reason) {
    CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
    if (!item || item->GetState() != download::DownloadItem::IN_PROGRESS) {
      CHECK(!item || item->GetLastReason() !=
                         download::DOWNLOAD_INTERRUPT_REASON_NONE,
            base::NotFatalUntil::M159);
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(on_completed_), false));
      return;
    }
    CHECK_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE, interrupt_reason,
             base::NotFatalUntil::M159);
    download_item_ = item;
    download_item_->AddObserver(this);
  }

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* item) override {
    CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
    CHECK_EQ(download_item_, item, base::NotFatalUntil::M159);
    download::DownloadItem::DownloadState state = download_item_->GetState();
    if (state == download::DownloadItem::COMPLETE ||
        state == download::DownloadItem::CANCELLED ||
        state == download::DownloadItem::INTERRUPTED) {
      if (on_completed_) {
        GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(on_completed_),
                           state == download::DownloadItem::COMPLETE));
      }
      download_item_->RemoveObserver(this);
      download_item_ = nullptr;
    }
    // Ignore other states.
  }

  void OnDownloadDestroyed(download::DownloadItem* item) override {
    CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
    CHECK_EQ(download_item_, item, base::NotFatalUntil::M159);
    if (on_completed_) {
      const bool is_complete =
          download_item_->GetState() == download::DownloadItem::COMPLETE;
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(on_completed_), is_complete));
    }
    download_item_->RemoveObserver(this);
    download_item_ = nullptr;
  }

  OnCompleted on_completed_;
  WeakDocumentPtr source_document_;
  GURL url_;
  Referrer referrer_;
  std::string referrer_encoding_;
  raw_ptr<download::DownloadItem> download_item_ = nullptr;

  // Only used in the callback from DownloadManager::DownloadUrl().
  base::WeakPtrFactory<DragDownloadFileUI> weak_ptr_factory_{this};
};

DragDownloadFile::DragDownloadFile(WeakDocumentPtr source_document,
                                   const base::FilePath& file_path,
                                   base::File file,
                                   const GURL& url,
                                   const Referrer& referrer,
                                   const std::string& referrer_encoding)
    : file_path_(file_path), file_(std::move(file)) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  drag_ui_ = new DragDownloadFileUI(
      std::move(source_document), url, referrer, referrer_encoding,
      base::BindOnce(&DragDownloadFile::DownloadCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  CHECK(!file_path_.empty(), base::NotFatalUntil::M159);
}

DragDownloadFile::~DragDownloadFile() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  // This is the only place that drag_ui_ can be deleted from. Post a message to
  // the UI thread so that it calls RemoveObserver on the right thread, and so
  // that this task will run after the InitiateDownload task runs on the UI
  // thread.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DragDownloadFileUI::Delete, base::Unretained(drag_ui_)));
  drag_ui_ = nullptr;
}

void DragDownloadFile::Start(ui::DownloadFileObserver* observer) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  if (state_ != INITIALIZED)
    return;
  state_ = STARTED;

  CHECK(!observer_.get(), base::NotFatalUntil::M159);
  observer_ = observer;
  CHECK(observer_.get(), base::NotFatalUntil::M159);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DragDownloadFileUI::InitiateDownload,
                     base::Unretained(drag_ui_), std::move(file_), file_path_));
}

bool DragDownloadFile::Wait() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  if (state_ == STARTED)
    nested_loop_.Run();
  CHECK(weak_ptr, base::NotFatalUntil::M159);
  return state_ == SUCCESS;
}

void DragDownloadFile::Stop() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (drag_ui_) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DragDownloadFileUI::Cancel,
                                  base::Unretained(drag_ui_)));
  }
}

void DragDownloadFile::DownloadCompleted(bool is_successful) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

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
