// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_IMPL_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_IMPL_H_

#import "base/containers/queue.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/task/cancelable_task_tracker.h"
#import "base/time/time.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/prefs/pref_member.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

@class WKWebView;

class Browser;
namespace base {
class ScopedClosureRunner;
}
namespace net {
class URLRequestContextGetter;
}

// BrowsingDataRemoverImpl is the concrete implementation of the
// BrowsingDataRemover abstract interface.
class BrowsingDataRemoverImpl : public BrowsingDataRemover {
 public:
  // Creates a BrowsingDataRemoverImpl to remove browser data from the
  // specified ProfileIOS. Use Remove to initiate the removal.
  explicit BrowsingDataRemoverImpl(ProfileIOS* profile);

  BrowsingDataRemoverImpl(const BrowsingDataRemoverImpl&) = delete;
  BrowsingDataRemoverImpl& operator=(const BrowsingDataRemoverImpl&) = delete;

  ~BrowsingDataRemoverImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowsingDataRemover implementation.
  bool IsRemoving() const override;
  void Remove(browsing_data::TimePeriod time_period,
              BrowsingDataRemoveMask remove_mask,
              base::OnceClosure callback,
              RemovalParams params = RemovalParams::Default()) override;
  void RemoveInRange(base::Time start_time,
                     base::Time end_time,
                     BrowsingDataRemoveMask mask,
                     base::OnceClosure callback,
                     RemovalParams params = RemovalParams::Default()) override;
  // May be called with a meaningful value prior to using `Remove` or
  // `RemoveInRange` if `BrowsingDataRemoveMask::CLOSE_TABS` is selected, to
  // avoid having to fetch this info from persisted storage again.
  void SetCachedTabsInfo(
      tabs_closure_util::WebStateIDToTime cached_tabs_info) override;

 private:
  // Represents a single removal task. Contains all parameters to execute it.
  struct RemovalTask {
    RemovalTask(base::Time delete_begin,
                base::Time delete_end,
                BrowsingDataRemoveMask mask,
                base::OnceClosure callback,
                RemovalParams params);
    RemovalTask(RemovalTask&& other) noexcept;
    ~RemovalTask();

    base::Time delete_begin;
    base::Time delete_end;
    BrowsingDataRemoveMask mask;
    base::OnceClosure callback;
    RemovalParams params;
    base::Time task_started;
  };

  // Setter for `is_removing_`; DCHECKs that we can only start removing if we're
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

  // If necessary, shows an activity indicator while the deletion is ongoing.
  void PrepareForRemoval(BrowsingDataRemoveMask mask, RemovalParams params);

  // If necessary, removes the activity indicator, reloads all web states and
  // resets NTPs.
  void CleanupAfterRemoval(BrowsingDataRemoveMask mask, RemovalParams params);

  // Removes the specified items related to browsing.
  void RemoveImpl(base::Time delete_begin,
                  base::Time delete_end,
                  BrowsingDataRemoveMask mask,
                  RemovalParams params);

  // Removes the browsing data stored in WKWebsiteDataStore if needed.
  void RemoveDataFromWKWebsiteDataStore(base::Time delete_begin,
                                        BrowsingDataRemoveMask mask);

  // Implementation for `BrowsingDataRemoveMask::CLOSE_TABS`: If
  // `cached_tabs_info_` has been set, uses that to determine which tabs to
  // close and closes them. Otherwise, fetches the relevant information from
  // persisted storage first.
  void MaybeFetchTabsInfoThenCloseTabs(base::Time delete_begin,
                                       base::Time delete_end,
                                       RemovalParams params);

  // Called when the information about tabs from a single browser has been
  // loaded from persisted storage. Closes tabs from that browser.
  void OnTabsInformationLoaded(base::WeakPtr<Browser> weak_browser,
                               base::Time delete_begin,
                               base::Time delete_end,
                               RemovalParams params,
                               base::OnceClosure callback,
                               tabs_closure_util::WebStateIDToTime result);

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

  // ProfileIOS we're to remove from.
  raw_ptr<ProfileIOS> profile_ = nullptr;

  // Used to delete data from HTTP cache.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Holds the cached information for tabs. It's used when closing tabs is
  // selected. May be set with a meaningful value in order to more efficiently
  // close tabs.
  tabs_closure_util::WebStateIDToTime cached_tabs_info_;
  bool cached_tabs_info_initialized_;

  // Is the object currently in the process of removing data?
  bool is_removing_ = false;

  // Number of pending tasks.
  int pending_tasks_count_ = 0;

  // Removal tasks to be processed.
  base::queue<RemovalTask> removal_queue_;

  // Callback to remove the activity overlay started by the browser coordinator
  // itself.
  base::ScopedClosureRunner _activityOverlayCallback;

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

  base::CallbackListSubscription template_url_subscription_;

  base::WeakPtrFactory<BrowsingDataRemoverImpl> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_IMPL_H_
