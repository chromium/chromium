// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/download_test_observer.h"
#include "base/memory/raw_ptr.h"

#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

DownloadUpdatedObserver::DownloadUpdatedObserver(
    download::DownloadItem* item,
    DownloadUpdatedObserver::EventFilter filter)
    : item_(item), filter_(filter), waiting_(false), event_seen_(false) {
  item->AddObserver(this);
}

DownloadUpdatedObserver::~DownloadUpdatedObserver() {
  if (item_)
    item_->RemoveObserver(this);
}

bool DownloadUpdatedObserver::WaitForEvent() {
  if (item_ && filter_.Run(item_.get()))
    event_seen_ = true;
  if (event_seen_)
    return true;

  waiting_ = true;
  loop_.Run();
  waiting_ = false;
  return event_seen_;
}

void DownloadUpdatedObserver::OnDownloadUpdated(download::DownloadItem* item) {
  DCHECK_EQ(item_, item);
  if (filter_.Run(item_.get()))
    event_seen_ = true;
  if (waiting_ && event_seen_) {
    loop_.QuitWhenIdle();
  }
}

void DownloadUpdatedObserver::OnDownloadDestroyed(
    download::DownloadItem* item) {
  DCHECK_EQ(item_, item);
  item_->RemoveObserver(this);
  item_ = nullptr;
  if (waiting_) {
    loop_.QuitWhenIdle();
  }
}

DownloadTestObserver::DownloadTestObserver(
    DownloadManager* download_manager,
    size_t wait_count,
    DangerousDownloadAction dangerous_download_action)
    : download_manager_(download_manager),
      wait_count_(wait_count),
      finished_downloads_at_construction_(0),
      waiting_(false),
      dangerous_download_action_(dangerous_download_action) {}

DownloadTestObserver::~DownloadTestObserver() {
  for (DownloadSet::iterator it = downloads_observed_.begin();
       it != downloads_observed_.end(); ++it)
    (*it)->RemoveObserver(this);

  if (download_manager_)
    download_manager_->RemoveObserver(this);
}

void DownloadTestObserver::Init() {
  download_manager_->AddObserver(this);
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  download_manager_->GetAllDownloads(&downloads);
  for (std::vector<
           raw_ptr<download::DownloadItem, VectorExperimental>>::iterator it =
           downloads.begin();
       it != downloads.end(); ++it) {
    OnDownloadCreated(download_manager_, *it);
  }
  finished_downloads_at_construction_ = finished_downloads_.size();
  states_observed_.clear();
}

void DownloadTestObserver::ManagerGoingDown(DownloadManager* manager) {
  CHECK_EQ(manager, download_manager_);
  download_manager_ = nullptr;
  SignalIfFinished();
}

void DownloadTestObserver::WaitForFinished() {
  if (!IsFinished()) {
    waiting_ = true;
    loop_.Run();
    waiting_ = false;
  }
}

bool DownloadTestObserver::IsFinished() const {
  return (finished_downloads_.size() - finished_downloads_at_construction_ >=
          wait_count_) ||
         (download_manager_ == nullptr);
}

void DownloadTestObserver::OnDownloadCreated(DownloadManager* manager,
                                             download::DownloadItem* item) {
  // NOTE: This method is called both by DownloadManager when a download is
  // created as well as in DownloadTestObserver::Init() for downloads that
  // existed before |this| was created.
  OnDownloadUpdated(item);
  // If it isn't finished, start observing it.
  if (!base::Contains(finished_downloads_, item)) {
    item->AddObserver(this);
    downloads_observed_.insert(item);
  }
}

void DownloadTestObserver::OnDownloadDestroyed(
    download::DownloadItem* download) {
  // Stop observing. Do not do anything with it, as it is about to be gone.
  CHECK(base::Contains(downloads_observed_, download));
  downloads_observed_.erase(download);
  download->RemoveObserver(this);
}

void DownloadTestObserver::OnDownloadUpdated(download::DownloadItem* download) {
  // Real UI code gets the user's response after returning from the observer.
  if (download->IsDangerous() &&
      !base::Contains(dangerous_downloads_seen_, download->GetId())) {
    dangerous_downloads_seen_.insert(download->GetId());

    // Calling ValidateDangerousDownload() at this point will
    // cause the download to be completed twice.  Do what the real UI
    // code does: make the call as a delayed task.
    switch (dangerous_download_action_) {
      case ON_DANGEROUS_DOWNLOAD_ACCEPT:
        // Fake user click on "Accept".  Delay the actual click, as the
        // real UI would.
        GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(&DownloadTestObserver::AcceptDangerousDownload,
                           weak_factory_.GetWeakPtr(), download->GetId()));
        break;

      case ON_DANGEROUS_DOWNLOAD_DENY:
        // Fake a user click on "Deny".  Delay the actual click, as the
        // real UI would.
        GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(&DownloadTestObserver::DenyDangerousDownload,
                           weak_factory_.GetWeakPtr(), download->GetId()));
        break;

      case ON_DANGEROUS_DOWNLOAD_FAIL:
        ADD_FAILURE() << "Unexpected dangerous download item.";
        break;

      case ON_DANGEROUS_DOWNLOAD_IGNORE:
        break;

      case ON_DANGEROUS_DOWNLOAD_QUIT:
        DownloadInFinalState(download);
        break;

      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  if (IsDownloadInFinalState(download))
    DownloadInFinalState(download);
}

size_t DownloadTestObserver::NumDangerousDownloadsSeen() const {
  return dangerous_downloads_seen_.size();
}

size_t DownloadTestObserver::NumDownloadsSeenInState(
    download::DownloadItem::DownloadState state) const {
  StateMap::const_iterator it = states_observed_.find(state);

  if (it == states_observed_.end())
    return 0;

  return it->second;
}

void DownloadTestObserver::DownloadInFinalState(
    download::DownloadItem* download) {
  if (base::Contains(finished_downloads_, download)) {
    // We've already seen the final state on this download.
    return;
  }

  // Record the transition.
  finished_downloads_.insert(download);

  // Record the state.
  states_observed_[download->GetState()]++;  // Initializes to 0 the first time.

  SignalIfFinished();
}

void DownloadTestObserver::SignalIfFinished() {
  if (waiting_ && IsFinished()) {
    loop_.QuitWhenIdle();
  }
}

void DownloadTestObserver::AcceptDangerousDownload(uint32_t download_id) {
  // Download manager was shutdown before the UI thread could accept the
  // download.
  if (!download_manager_)
    return;
  download::DownloadItem* download =
      download_manager_->GetDownload(download_id);
  if (download && !download->IsDone())
    download->ValidateDangerousDownload();
}

void DownloadTestObserver::DenyDangerousDownload(uint32_t download_id) {
  // Download manager was shutdown before the UI thread could deny the
  // download.
  if (!download_manager_)
    return;
  download::DownloadItem* download =
      download_manager_->GetDownload(download_id);
  if (download && !download->IsDone())
    download->Remove();
}

DownloadTestObserverTerminal::DownloadTestObserverTerminal(
    DownloadManager* download_manager,
    size_t wait_count,
    DangerousDownloadAction dangerous_download_action)
        : DownloadTestObserver(download_manager,
                               wait_count,
                               dangerous_download_action) {
  // You can't rely on overriden virtual functions in a base class constructor;
  // the virtual function table hasn't been set up yet.  So, we have to do any
  // work that depends on those functions in the derived class constructor
  // instead.  In this case, it's because of |IsDownloadInFinalState()|.
  Init();
}

DownloadTestObserverTerminal::~DownloadTestObserverTerminal() {
}

bool DownloadTestObserverTerminal::IsDownloadInFinalState(
    download::DownloadItem* download) {
  return download->IsDone();
}

DownloadTestObserverInProgress::DownloadTestObserverInProgress(
    DownloadManager* download_manager,
    size_t wait_count)
        : DownloadTestObserver(download_manager,
                               wait_count,
                               ON_DANGEROUS_DOWNLOAD_ACCEPT) {
  // You can't override virtual functions in a base class constructor; the
  // virtual function table hasn't been set up yet.  So, we have to do any
  // work that depends on those functions in the derived class constructor
  // instead.  In this case, it's because of |IsDownloadInFinalState()|.
  Init();
}

DownloadTestObserverInProgress::~DownloadTestObserverInProgress() {
}

bool DownloadTestObserverInProgress::IsDownloadInFinalState(
    download::DownloadItem* download) {
  return (download->GetState() == download::DownloadItem::IN_PROGRESS) &&
         !download->GetTargetFilePath().empty();
}

DownloadTestObserverInterrupted::DownloadTestObserverInterrupted(
    DownloadManager* download_manager,
    size_t wait_count,
    DangerousDownloadAction dangerous_download_action)
        : DownloadTestObserver(download_manager,
                               wait_count,
                               dangerous_download_action) {
  // You can't rely on overriden virtual functions in a base class constructor;
  // the virtual function table hasn't been set up yet.  So, we have to do any
  // work that depends on those functions in the derived class constructor
  // instead.  In this case, it's because of |IsDownloadInFinalState()|.
  Init();
}

DownloadTestObserverInterrupted::~DownloadTestObserverInterrupted() {
}

bool DownloadTestObserverInterrupted::IsDownloadInFinalState(
    download::DownloadItem* download) {
  return download->GetState() == download::DownloadItem::INTERRUPTED;
}

void PingIOThread(int cycle, base::OnceClosure callback);

// Helper method to post a task to IO thread to ensure remaining operations on
// the IO thread complete.
void PingFileThread(int cycle, base::OnceClosure callback) {
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PingIOThread, cycle, std::move(callback)));
}

// Post a task to file thread, and wait for it to be posted back on to the IO
// thread if |cycle| is larger than 1. This ensures that all remaining
// operations on the IO thread complete.
void PingIOThread(int cycle, base::OnceClosure callback) {
  if (--cycle) {
    DownloadManager::GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&PingFileThread, cycle, std::move(callback)));
  } else {
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
  }
}

DownloadTestFlushObserver::DownloadTestFlushObserver(
    DownloadManager* download_manager)
    : download_manager_(download_manager),
      waiting_for_zero_inprogress_(true) {}

void DownloadTestFlushObserver::WaitForFlush() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  download_manager_->AddObserver(this);
  // The wait condition may have been met before WaitForFlush() was called.
  CheckDownloadsInProgress(true);
  run_loop_.Run();
}

void DownloadTestFlushObserver::OnDownloadCreated(
    DownloadManager* manager,
    download::DownloadItem* item) {
  CheckDownloadsInProgress(true);
}

void DownloadTestFlushObserver::ManagerGoingDown(DownloadManager* manager) {
  download_manager_ = nullptr;
}

void DownloadTestFlushObserver::OnDownloadDestroyed(
    download::DownloadItem* download) {
  // Stop observing. Do not do anything with it, as it is about to be gone.
  CHECK(base::Contains(downloads_observed_, download));
  downloads_observed_.erase(download);
  download->RemoveObserver(this);
}

void DownloadTestFlushObserver::OnDownloadUpdated(
    download::DownloadItem* download) {
  // No change in download::DownloadItem set on manager.
  CheckDownloadsInProgress(false);
}

DownloadTestFlushObserver::~DownloadTestFlushObserver() {
  if (!download_manager_)
    return;

  download_manager_->RemoveObserver(this);
  for (DownloadSet::iterator it = downloads_observed_.begin();
       it != downloads_observed_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
}

// If we're waiting for that flush point, check the number
// of downloads in the IN_PROGRESS state and take appropriate
// action. If requested, also observes all downloads while iterating.
void DownloadTestFlushObserver::CheckDownloadsInProgress(
    bool observe_downloads) {
  if (waiting_for_zero_inprogress_) {
    int count = 0;

    std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
    download_manager_->GetAllDownloads(&downloads);
    for (std::vector<
             raw_ptr<download::DownloadItem, VectorExperimental>>::iterator it =
             downloads.begin();
         it != downloads.end(); ++it) {
      if ((*it)->GetState() == download::DownloadItem::IN_PROGRESS)
        count++;
      if (observe_downloads) {
        if (!base::Contains(downloads_observed_, *it)) {
          (*it)->AddObserver(this);
          downloads_observed_.insert(*it);
        }
        // Download items are forever, and we don't want to make
        // assumptions about future state transitions, so once we
        // start observing them, we don't stop until destruction.
      }
    }

    if (count == 0) {
      waiting_for_zero_inprogress_ = false;
      // Stop observing download::DownloadItems.  We maintain the observation
      // of DownloadManager so that we don't have to independently track
      // whether we are observing it for conditional destruction.
      for (download::DownloadItem* item : downloads_observed_) {
        item->RemoveObserver(this);
      }
      downloads_observed_.clear();

      // Trigger next step.  We need to go past the IO thread twice, as
      // there's a self-task posting in the IO thread cancel path.
      DownloadManager::GetTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&PingFileThread, 2, run_loop_.QuitClosure()));
    }
  }
}

DownloadTestItemCreationObserver::DownloadTestItemCreationObserver()
    : download_id_(download::DownloadItem::kInvalidId),
      interrupt_reason_(download::DOWNLOAD_INTERRUPT_REASON_NONE),
      called_back_count_(0),
      waiting_(false) {}

DownloadTestItemCreationObserver::~DownloadTestItemCreationObserver() {
}

void DownloadTestItemCreationObserver::WaitForDownloadItemCreation() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (called_back_count_ == 0) {
    waiting_ = true;
    loop_.Run();
    waiting_ = false;
  }
}

void DownloadTestItemCreationObserver::DownloadItemCreationCallback(
    download::DownloadItem* item,
    download::DownloadInterruptReason interrupt_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (item)
    download_id_ = item->GetId();
  interrupt_reason_ = interrupt_reason;
  ++called_back_count_;
  DCHECK_EQ(1u, called_back_count_);

  if (waiting_) {
    loop_.QuitWhenIdle();
  }
}

download::DownloadUrlParameters::OnStartedCallback
DownloadTestItemCreationObserver::callback() {
  return base::BindOnce(
      &DownloadTestItemCreationObserver::DownloadItemCreationCallback, this);
}

SavePackageFinishedObserver::SavePackageFinishedObserver(
    DownloadManager* manager,
    base::OnceClosure callback,
    std::set<download::DownloadItem::DownloadState> final_states)
    : download_manager_(manager),
      download_(nullptr),
      callback_(std::move(callback)),
      final_states_(std::move(final_states)) {
  download_manager_->AddObserver(this);
}

SavePackageFinishedObserver::~SavePackageFinishedObserver() {
  if (download_manager_)
    download_manager_->RemoveObserver(this);

  if (download_)
    download_->RemoveObserver(this);
}

void SavePackageFinishedObserver::OnDownloadUpdated(
    download::DownloadItem* download) {
  if (final_states_.count(download->GetState())) {
    std::move(callback_).Run();
  }
}

void SavePackageFinishedObserver::OnDownloadDestroyed(
    download::DownloadItem* download) {
  download_->RemoveObserver(this);
  download_ = nullptr;
}

void SavePackageFinishedObserver::OnDownloadCreated(
    DownloadManager* manager,
    download::DownloadItem* download) {
  download_ = download;
  download->AddObserver(this);
}

void SavePackageFinishedObserver::ManagerGoingDown(DownloadManager* manager) {
  download_->RemoveObserver(this);
  download_ = nullptr;
  download_manager_->RemoveObserver(this);
  download_manager_ = nullptr;
}

}  // namespace content
