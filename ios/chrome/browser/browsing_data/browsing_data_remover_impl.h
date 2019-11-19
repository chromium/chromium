// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_IMPL_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/prefs/pref_member.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"

@class SessionServiceIOS;
@class WKWebView;

namespace ios {
class ChromeBrowserState;
}

namespace net {
class URLRequestContextGetter;
}

// BrowsingDataRemoverImpl is the concrete implementation of the
// BrowsingDataRemover abstract interface.
class BrowsingDataRemoverImpl : public BrowsingDataRemover {
 public:
  // Creates a BrowsingDataRemoverImpl to remove browser data from the
  // specified ChromeBrowserstate. Use Remove to initiate the removal.
  BrowsingDataRemoverImpl(ios::ChromeBrowserState* browser_state,
                          SessionServiceIOS* session_service);
  ~BrowsingDataRemoverImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowsingDataRemover implementation.
  bool IsRemoving() const override;
  void Remove(browsing_data::TimePeriod time_period,
              BrowsingDataRemoveMask remove_mask,
              base::OnceClosure callback) override;

 private:
  // Represents a single removal task. Contains all parameters to execute it.
  struct RemovalTask {
    RemovalTask(base::Time delete_begin,
                base::Time delete_end,
                BrowsingDataRemoveMask mask,
                base::OnceClosure callback);
    RemovalTask(RemovalTask&& other) noexcept;
    ~RemovalTask();

    base::Time delete_begin;
    base::Time delete_end;
    BrowsingDataRemoveMask mask;
    base::OnceClosure callback;
    base::Time task_started;
  };

  // Setter for |is_removing_|; DCHECKs that we can only start removing if we're
  // not already removing, and vice-versa.
  void SetRemoving(bool is_removing);

  // Callback for when TemplateURLService has finished loading. Clears the data,
  // and invoke the callback.
  void OnKeywordsLoaded(base::Time delete_begin,
                        base::Time delete_end,
                        base::OnceClosure callback);

  // Execute the next removal task. Called after the previous task was finished
  // or directly from Remove.
  void RunNextTask();

  // Removes the specified items related to browsing.
  void RemoveImpl(base::Time delete_begin,
                  base::Time delete_end,
                  BrowsingDataRemoveMask mask);

  // Removes the browsing data stored in WKWebsiteDataStore if needed.
  void RemoveDataFromWKWebsiteDataStore(base::Time delete_begin,
                                        BrowsingDataRemoveMask mask);

  // Invokes the current task callback that the removal has completed.
  void NotifyRemovalComplete();

  // Called by the closures returned by CreatePendingTaskCompletionClosure().
  // Checks if all tasks have completed, and if so, calls Notify().
  void OnTaskComplete();

  // Increments the number of pending tasks by one, and returns a OnceClosure
  // that calls OnTaskComplete(). The Remover is complete once all the closures
  // created by this method have been invoked.
  base::OnceClosure CreatePendingTaskCompletionClosure();

  // Returns a weak pointer to BrowsingDataRemoverImpl for internal
  // purposes.
  base::WeakPtr<BrowsingDataRemoverImpl> GetWeakPtr();

  // This object is sequence affine.
  SEQUENCE_CHECKER(sequence_checker_);

  // ChromeBrowserState we're to remove from.
  ios::ChromeBrowserState* browser_state_ = nullptr;

  // SessionService to use (allow injection of a specific instance for testing).
  SessionServiceIOS* session_service_ = nil;

  // Dummy WKWebView. A WKWebView object is created before deleting cookies. and
  // is deleted after deleting cookies is completed. this is a workaround that
  // makes sure that there is a WKWebView object alive while accessing
  // WKHTTPCookieStore.
  WKWebView* dummy_web_view_ = nil;

  // Used to delete data from HTTP cache.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Is the object currently in the process of removing data?
  bool is_removing_ = false;

  // Number of pending tasks.
  int pending_tasks_count_ = 0;

  // Removal tasks to be processed.
  base::queue<RemovalTask> removal_queue_;

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_subscription_;

  base::WeakPtrFactory<BrowsingDataRemoverImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverImpl);
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_IMPL_H_
