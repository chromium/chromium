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
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
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
  DragDownloadFileUI(const GURL& url,
                     const Referrer& referrer,
                     const std::string& referrer_encoding,
                     std::optional<url::Origin> initiator_origin,
                     int render_process_id,
                     int render_frame_id,
                     OnCompleted on_completed)
      : on_completed_(std::move(on_completed)),
        url_(url),
        referrer_(referrer),
        referrer_encoding_(referrer_encoding),
        initiator_origin_(initiator_origin),
        render_process_id_(render_process_id),
        render_frame_id_(render_frame_id) {
    DCHECK(on_completed_);
    DCHECK_GE(render_frame_id_, 0);
    // May be called on any thread.
    // Do not call weak_ptr_factory_.GetWeakPtr() outside the UI thread.
  }

  DragDownloadFileUI(const DragDownloadFileUI&) = delete;
  DragDownloadFileUI& operator=(const DragDownloadFileUI&) = delete;

  void InitiateDownload(base::File file,
                        const base::FilePath& file_path) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    RenderFrameHost* host =
        RenderFrameHost::FromID(render_process_id_, render_frame_id_);
    if (!host)
      return;
    // TODO(crbug.com/40470366) This should use the frame actually
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
    auto params = std::make_unique<download::DownloadUrlParameters>(
        url_, render_process_id_, render_frame_id_, traffic_annotation);
    params->set_referrer(referrer_.url);
    params->set_referrer_policy(
        Referrer::ReferrerPolicyForUrlRequest(referrer_.policy));
    params->set_referrer_encoding(referrer_encoding_);
    params->set_initiator(initiator_origin_);
    params->set_callback(base::BindOnce(&DragDownloadFileUI::OnDownloadStarted,
                                        weak_ptr_factory_.GetWeakPtr()));
    params->set_file_path(file_path);
    params->set_file(std::move(file));  // Nulls file.
    params->set_download_source(download::DownloadSource::DRAG_AND_DROP);
    host->GetBrowserContext()->GetDownloadManager()->DownloadUrl(
        std::move(params));
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
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(on_completed_), false));
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
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(download_item_, item);
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
  GURL url_;
  Referrer referrer_;
  std::string referrer_encoding_;
  std::optional<url::Origin> initiator_origin_;
  int render_process_id_;
  int render_frame_id_;
  raw_ptr<download::DownloadItem> download_item_ = nullptr;

  // Only used in the callback from DownloadManager::DownloadUrl().
  base::WeakPtrFactory<DragDownloadFileUI> weak_ptr_factory_{this};
};

DragDownloadFile::DragDownloadFile(const base::FilePath& file_path,
                                   base::File file,
                                   const GURL& url,
                                   const Referrer& referrer,
                                   const std::string& referrer_encoding,
                                   std::optional<url::Origin> initiator_origin,
                                   WebContents* web_contents)
    : file_path_(file_path), file_(std::move(file)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* host = web_contents->GetPrimaryMainFrame();
  drag_ui_ = new DragDownloadFileUI(
      url, referrer, referrer_encoding, initiator_origin,
      host->GetProcess()->GetID(), host->GetRoutingID(),
      base::BindOnce(&DragDownloadFile::DownloadCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  DCHECK(!file_path_.empty());
}

DragDownloadFile::~DragDownloadFile() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (state_ != INITIALIZED)
    return;
  state_ = STARTED;

  DCHECK(!observer_.get());
  observer_ = observer;
  DCHECK(observer_.get());

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
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
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DragDownloadFileUI::Cancel,
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
