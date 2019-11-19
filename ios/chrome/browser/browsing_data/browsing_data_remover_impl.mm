// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browsing_data/browsing_data_remover_impl.h"

#import <WebKit/WebKit.h>

#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#import "base/ios/block_types.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/autofill/core/browser/payments/strike_database.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/signin/ios/browser/account_consistency_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/autofill/strike_database_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_remover_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/external_files/external_file_remover.h"
#include "ios/chrome/browser/external_files/external_file_remover_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/web_history_service_factory.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#include "ios/chrome/browser/language/url_language_histogram_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_remover_helper.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#include "ios/chrome/browser/signin/account_consistency_service_factory.h"
#include "ios/chrome/browser/snapshots/snapshots_util.h"
#include "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#include "ios/net/http_cache_helper.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/browsing_data/browsing_data_removing_util.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_store.h"
#include "net/http/transport_security_state.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// A helper enum to report the deletion of cookies and/or cache. Do not
// reorder the entries, as this enum is passed to UMA.
enum CookieOrCacheDeletionChoice {
  NEITHER_COOKIES_NOR_CACHE,
  ONLY_COOKIES,
  ONLY_CACHE,
  BOTH_COOKIES_AND_CACHE,
  MAX_CHOICE_VALUE
};

template <typename T>
void IgnoreArgumentHelper(base::OnceClosure callback, T unused_argument) {
  std::move(callback).Run();
}

// A convenience method to turn a callback without arguments into one that
// accepts (and ignores) a single argument.
template <typename T>
base::OnceCallback<void(T)> IgnoreArgument(base::OnceClosure callback) {
  return base::BindOnce(&IgnoreArgumentHelper<T>, std::move(callback));
}

void BookmarkClearedAdapter(std::unique_ptr<BookmarkRemoverHelper> remover,
                            base::OnceClosure callback,
                            bool success) {
  CHECK(success) << "Failed to remove all user bookmarks.";
  std::move(callback).Run();
}

void ReadingListClearedAdapter(
    std::unique_ptr<reading_list::ReadingListRemoverHelper> remover,
    base::OnceClosure callback,
    bool success) {
  CHECK(success) << "Failed to remove all user reading list items.";
  std::move(callback).Run();
}

void NetCompletionCallbackAdapter(base::OnceClosure callback, int) {
  std::move(callback).Run();
}

void DeleteCallbackAdapter(base::OnceClosure callback, uint32_t) {
  std::move(callback).Run();
}

// Clears cookies.
void ClearCookies(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    const net::CookieDeletionInfo::TimeRange& creation_range,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  net::CookieStore* cookie_store =
      request_context_getter->GetURLRequestContext()->cookie_store();
  cookie_store->DeleteAllCreatedInTimeRangeAsync(
      creation_range, AdaptCallbackForRepeating(base::BindOnce(
                          &DeleteCallbackAdapter, std::move(callback))));
}

}  // namespace

BrowsingDataRemoverImpl::RemovalTask::RemovalTask(base::Time delete_begin,
                                                  base::Time delete_end,
                                                  BrowsingDataRemoveMask mask,
                                                  base::OnceClosure callback)
    : delete_begin(delete_begin),
      delete_end(delete_end),
      mask(mask),
      callback(std::move(callback)) {}

BrowsingDataRemoverImpl::RemovalTask::RemovalTask(
    RemovalTask&& other) noexcept = default;

BrowsingDataRemoverImpl::RemovalTask::~RemovalTask() = default;

BrowsingDataRemoverImpl::BrowsingDataRemoverImpl(
    ios::ChromeBrowserState* browser_state,
    SessionServiceIOS* session_service)
    : browser_state_(browser_state),
      session_service_(session_service),
      context_getter_(browser_state->GetRequestContext()),
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_state_);
}

BrowsingDataRemoverImpl::~BrowsingDataRemoverImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_removing_);
}

void BrowsingDataRemoverImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  browser_state_ = nullptr;

  if (is_removing_) {
    VLOG(1) << "BrowsingDataRemoverImpl shuts down with "
            << removal_queue_.size() << " pending tasks"
            << (pending_tasks_count_ ? " (including one in progress)" : "");

    SetRemoving(false);
  }

  UMA_HISTOGRAM_EXACT_LINEAR("History.ClearBrowsingData.TaskQueueAtShutdown",
                             removal_queue_.size(), 10);

  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunnerHandle::Get();

  // If we are still removing data, notify callbacks that their task has been
  // (albeit unsucessfuly) processed. If it becomes a problem that browsing
  // data might not actually be fully cleared when an observer is notified,
  // add a success flag.
  while (!removal_queue_.empty()) {
    RemovalTask task = std::move(removal_queue_.front());
    removal_queue_.pop();

    if (!task.callback.is_null()) {
      current_task_runner->PostTask(FROM_HERE, std::move(task.callback));
    }
  }
}

bool BrowsingDataRemoverImpl::IsRemoving() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_removing_;
}

void BrowsingDataRemoverImpl::SetRemoving(bool is_removing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_removing_ != is_removing);
  is_removing_ = is_removing;
}

void BrowsingDataRemoverImpl::Remove(browsing_data::TimePeriod time_period,
                                     BrowsingDataRemoveMask mask,
                                     base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_state_);

  // Should always remove something.
  DCHECK(mask != BrowsingDataRemoveMask::REMOVE_NOTHING);

  // In incognito, only data removal for all time is currently supported.
  DCHECK(!browser_state_->IsOffTheRecord() ||
         time_period == browsing_data::TimePeriod::ALL_TIME);

  // Partial clearing of downloads, bookmarks or reading lists is not supported.
  DCHECK(
      !(IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_DOWNLOADS) ||
        IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_BOOKMARKS) ||
        IsRemoveDataMaskSet(mask,
                            BrowsingDataRemoveMask::REMOVE_READING_LIST)) ||
      time_period == browsing_data::TimePeriod::ALL_TIME);

  // Removing visited links requires clearing the cookies.
  DCHECK(
      IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES) ||
      !IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_VISITED_LINKS));

  browsing_data::RecordDeletionForPeriod(time_period);
  removal_queue_.emplace(browsing_data::CalculateBeginDeleteTime(time_period),
                         browsing_data::CalculateEndDeleteTime(time_period),
                         mask, std::move(callback));

  // If this is the only scheduled task, execute it immediately. Otherwise,
  // it will be automatically executed when all tasks scheduled before it
  // finish.
  if (removal_queue_.size() == 1) {
    SetRemoving(true);
    RunNextTask();
  }
}

void BrowsingDataRemoverImpl::RunNextTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!removal_queue_.empty());
  RemovalTask& removal_task = removal_queue_.front();
  removal_task.task_started = base::Time::Now();
  RemoveImpl(removal_task.delete_begin, removal_task.delete_end,
             removal_task.mask);
}

void BrowsingDataRemoverImpl::RemoveImpl(base::Time delete_begin,
                                         base::Time delete_end,
                                         BrowsingDataRemoveMask mask) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedClosureRunner synchronous_clear_operations(
      CreatePendingTaskCompletionClosure());

  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunnerHandle::Get();

  // Note: Before adding any method below, make sure that it can finish clearing
  // browsing data even if |browser_state)| is destroyed after this method call.

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_HISTORY)) {
    if (session_service_) {
      NSString* state_path = base::SysUTF8ToNSString(
          browser_state_->GetStatePath().AsUTF8Unsafe());
      [session_service_
          deleteLastSessionFileInDirectory:state_path
                                completion:
                                    CreatePendingTaskCompletionClosure()];
    }

    // Remove the screenshots taken by the system when backgrounding the
    // application. Partial removal based on timePeriod is not required.
    ClearIOSSnapshots(CreatePendingTaskCompletionClosure());
  }

  constexpr base::TaskTraits task_traits = {
      web::WebThread::IO, base::TaskShutdownBehavior::BLOCK_SHUTDOWN};

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)) {
    base::RecordAction(base::UserMetricsAction("ClearBrowsingData_Cookies"));
    base::PostTask(
        FROM_HERE, task_traits,
        base::BindOnce(
            &ClearCookies, context_getter_,
            net::CookieDeletionInfo::TimeRange(delete_begin, delete_end),
            base::BindOnce(base::IgnoreResult(&base::TaskRunner::PostTask),
                           current_task_runner, FROM_HERE,
                           CreatePendingTaskCompletionClosure())));
  }

  // There is no need to clean the remaining types of data for off-the-record
  // ChromeBrowserStates as no data is saved. Early return to avoid scheduling
  // unnecessary work.
  if (browser_state_->IsOffTheRecord()) {
    return;
  }

  // On other platforms, it is possible to specify different types of origins
  // to clear data for (e.g., unprotected web vs. extensions). On iOS, this
  // mask is always implicitly the unprotected web, which is the only type that
  // is relevant. This metric is left here for historical consistency.
  base::RecordAction(
      base::UserMetricsAction("ClearBrowsingData_MaskContainsUnprotectedWeb"));

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_HISTORY)) {
    history::HistoryService* history_service =
        ios::HistoryServiceFactory::GetForBrowserState(
            browser_state_, ServiceAccessType::EXPLICIT_ACCESS);

    if (history_service) {
      base::RecordAction(base::UserMetricsAction("ClearBrowsingData_History"));
      history_service->DeleteLocalAndRemoteHistoryBetween(
          ios::WebHistoryServiceFactory::GetForBrowserState(browser_state_),
          delete_begin, delete_end, CreatePendingTaskCompletionClosure(),
          &history_task_tracker_);
    }

    // Need to clear the host cache and accumulated speculative data, as it also
    // reveals some history: we have no mechanism to track when these items were
    // created, so we'll clear them all. Better safe than sorry.
    IOSChromeIOThread* ios_chrome_io_thread =
        GetApplicationContext()->GetIOSChromeIOThread();
    if (ios_chrome_io_thread) {
      base::PostTaskAndReply(
          FROM_HERE, task_traits,
          base::BindOnce(&IOSChromeIOThread::ClearHostCache,
                         base::Unretained(ios_chrome_io_thread)),
          CreatePendingTaskCompletionClosure());
    }

    // As part of history deletion we also delete the auto-generated keywords.
    // Because the TemplateURLService is shared between incognito and
    // non-incognito profiles, don't do this in incognito.
    if (!browser_state_->IsOffTheRecord()) {
      TemplateURLService* keywords_model =
          ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
      if (keywords_model && !keywords_model->loaded()) {
        template_url_subscription_ =
            keywords_model->RegisterOnLoadedCallback(AdaptCallbackForRepeating(
                base::BindOnce(&BrowsingDataRemoverImpl::OnKeywordsLoaded,
                               GetWeakPtr(), delete_begin, delete_end,
                               CreatePendingTaskCompletionClosure())));
        keywords_model->Load();
      } else if (keywords_model) {
        keywords_model->RemoveAutoGeneratedBetween(delete_begin, delete_end);
      }

      ClipboardRecentContent::GetInstance()->SuppressClipboardContent();
    }

    // If the caller is removing history for all hosts, then clear ancillary
    // historical information.
    // We also delete the list of recently closed tabs. Since these expire,
    // they can't be more than a day old, so we can simply clear them all.
    sessions::TabRestoreService* tab_service =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(browser_state_);
    if (tab_service) {
      tab_service->ClearEntries();
      tab_service->DeleteLastSession();
    }

    // The saved Autofill profiles and credit cards can include the origin from
    // which these profiles and credit cards were learned.  These are a form of
    // history, so clear them as well.
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
            browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
    if (web_data_service.get()) {
      web_data_service->RemoveOriginURLsModifiedBetween(delete_begin,
                                                        delete_end);
      // Ask for a call back when the above call is finished.
      web_data_service->GetDBTaskRunner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(), CreatePendingTaskCompletionClosure());

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForBrowserState(
              browser_state_);

      if (data_manager)
        data_manager->Refresh();
    }

    // Remove language histogram history.
    language::UrlLanguageHistogram* language_histogram =
        UrlLanguageHistogramFactory::GetForBrowserState(browser_state_);
    if (language_histogram) {
      language_histogram->ClearHistory(delete_begin, delete_end);
    }
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_PASSWORDS)) {
    base::RecordAction(base::UserMetricsAction("ClearBrowsingData_Passwords"));
    password_manager::PasswordStore* password_store =
        IOSChromePasswordStoreFactory::GetForBrowserState(
            browser_state_, ServiceAccessType::EXPLICIT_ACCESS)
            .get();

    if (password_store) {
      password_store->RemoveLoginsCreatedBetween(
          delete_begin, delete_end,
          AdaptCallbackForRepeating(CreatePendingTaskCompletionClosure()));
    }
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_FORM_DATA)) {
    base::RecordAction(base::UserMetricsAction("ClearBrowsingData_Autofill"));
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
            browser_state_, ServiceAccessType::EXPLICIT_ACCESS);

    if (web_data_service.get()) {
      web_data_service->RemoveFormElementsAddedBetween(delete_begin,
                                                       delete_end);
      web_data_service->RemoveAutofillDataModifiedBetween(delete_begin,
                                                          delete_end);

      // Clear out the Autofill StrikeDatabase in its entirety.
      autofill::StrikeDatabase* strike_database =
          autofill::StrikeDatabaseFactory::GetForBrowserState(browser_state_);
      if (strike_database)
        strike_database->ClearAllStrikes();

      // Ask for a call back when the above calls are finished.
      web_data_service->GetDBTaskRunner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(), CreatePendingTaskCompletionClosure());

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForBrowserState(
              browser_state_);

      if (data_manager)
        data_manager->Refresh();
    }
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_CACHE)) {
    base::RecordAction(base::UserMetricsAction("ClearBrowsingData_Cache"));
    ClearHttpCache(context_getter_,
                   base::CreateSingleThreadTaskRunner(task_traits),
                   delete_begin, delete_end,
                   base::BindOnce(&NetCompletionCallbackAdapter,
                                  CreatePendingTaskCompletionClosure()));
  }

  // Remove omnibox zero-suggest cache results.
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_CACHE) ||
      IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)) {
    browser_state_->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                          std::string());
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_DOWNLOADS)) {
    ExternalFileRemover* external_file_remover =
        ExternalFileRemoverFactory::GetForBrowserState(browser_state_);
    if (external_file_remover) {
      external_file_remover->RemoveAfterDelay(
          base::TimeDelta::FromSeconds(0),
          CreatePendingTaskCompletionClosure());
    }
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_BOOKMARKS)) {
    auto bookmarks_remover_helper =
        std::make_unique<BookmarkRemoverHelper>(browser_state_);
    auto* bookmarks_remover_helper_ptr = bookmarks_remover_helper.get();

    // Pass the ownership of BookmarkRemoverHelper to the callback. This is
    // safe as the callback is always invoked, even if ChromeBrowserState is
    // destroyed, and BookmarkRemoverHelper supports being deleted while the
    // callback is run.
    bookmarks_remover_helper_ptr->RemoveAllUserBookmarksIOS(base::BindOnce(
        &BookmarkClearedAdapter, std::move(bookmarks_remover_helper),
        CreatePendingTaskCompletionClosure()));
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_READING_LIST)) {
    auto reading_list_remover_helper =
        std::make_unique<reading_list::ReadingListRemoverHelper>(
            browser_state_);
    auto* reading_list_remover_helper_ptr = reading_list_remover_helper.get();

    // Pass the ownership of reading_list::ReadingListRemoverHelper to the
    // callback. This is safe as the callback is always invoked, even if
    // ChromeBrowserState is destroyed, and ReadingListRemoverHelper supports
    // being deleted while the callback is run..
    reading_list_remover_helper_ptr->RemoveAllUserReadingListItemsIOS(
        base::BindOnce(&ReadingListClearedAdapter,
                       std::move(reading_list_remover_helper),
                       CreatePendingTaskCompletionClosure()));
  }

  if (IsRemoveDataMaskSet(mask,
                          BrowsingDataRemoveMask::REMOVE_LAST_USER_ACCOUNT)) {
    // The user just changed the account and chose to clear the previously
    // existing data. As browsing data is being cleared, it is fine to clear the
    // last username, as there will be no data to be merged.
    browser_state_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastAccountId);
    browser_state_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);
  }

  // Always wipe accumulated network related data (TransportSecurityState and
  // HttpServerPropertiesManager data).
  browser_state_->ClearNetworkingHistorySince(
      delete_begin,
      AdaptCallbackForRepeating(CreatePendingTaskCompletionClosure()));

  // Remove browsing data stored in WKWebsiteDataStore if necessary.
  RemoveDataFromWKWebsiteDataStore(delete_begin, mask);

  // Record the combined deletion of cookies and cache.
  CookieOrCacheDeletionChoice choice;
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_CACHE)) {
    choice = IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)
                 ? BOTH_COOKIES_AND_CACHE
                 : ONLY_CACHE;
  } else {
    choice = IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)
                 ? ONLY_COOKIES
                 : NEITHER_COOKIES_NOR_CACHE;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCache", choice,
      MAX_CHOICE_VALUE);
}

// TODO(crbug.com/619783): removing data from WkWebsiteDataStore should be
// implemented by //ios/web. Once this is available remove this and use the
// new API.
void BrowsingDataRemoverImpl::RemoveDataFromWKWebsiteDataStore(
    base::Time delete_begin,
    BrowsingDataRemoveMask mask) {
  if (base::FeatureList::IsEnabled(kWebClearBrowsingData)) {
    web::ClearBrowsingDataMask types =
        web::ClearBrowsingDataMask::kRemoveNothing;
    if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_APPCACHE)) {
      types |= web::ClearBrowsingDataMask::kRemoveAppCache;
    }
    if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)) {
      types |= web::ClearBrowsingDataMask::kRemoveCookies;
    }
    if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_INDEXEDDB)) {
      types |= web::ClearBrowsingDataMask::kRemoveIndexedDB;
    }
    if (IsRemoveDataMaskSet(mask,
                            BrowsingDataRemoveMask::REMOVE_LOCAL_STORAGE)) {
      types |= web::ClearBrowsingDataMask::kRemoveLocalStorage;
    }
    if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_WEBSQL)) {
      types |= web::ClearBrowsingDataMask::kRemoveWebSQL;
    }
    if (IsRemoveDataMaskSet(mask,
                            BrowsingDataRemoveMask::REMOVE_CACHE_STORAGE)) {
      types |= web::ClearBrowsingDataMask::kRemoveCacheStorage;
    }
    if (IsRemoveDataMaskSet(mask,
                            BrowsingDataRemoveMask::REMOVE_VISITED_LINKS)) {
      types |= web::ClearBrowsingDataMask::kRemoveVisitedLinks;
    }

    web::ClearBrowsingData(browser_state_, types, delete_begin,
                           CreatePendingTaskCompletionClosure());
    return;
  }

  // Converts browsing data types from BrowsingDataRemoveMask to
  // WKWebsiteDataStore strings.
  NSMutableSet* data_types_to_remove = [[NSMutableSet alloc] init];

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_CACHE_STORAGE)) {
    [data_types_to_remove addObject:WKWebsiteDataTypeDiskCache];
    [data_types_to_remove addObject:WKWebsiteDataTypeMemoryCache];
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_APPCACHE)) {
    [data_types_to_remove
        addObject:WKWebsiteDataTypeOfflineWebApplicationCache];
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_LOCAL_STORAGE)) {
    [data_types_to_remove addObject:WKWebsiteDataTypeSessionStorage];
    [data_types_to_remove addObject:WKWebsiteDataTypeLocalStorage];
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_WEBSQL)) {
    [data_types_to_remove addObject:WKWebsiteDataTypeWebSQLDatabases];
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_INDEXEDDB)) {
    [data_types_to_remove addObject:WKWebsiteDataTypeIndexedDBDatabases];
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)) {
    [data_types_to_remove addObject:WKWebsiteDataTypeCookies];
  }

  if (![data_types_to_remove count]) {
    return;
  }

  base::WeakPtr<BrowsingDataRemoverImpl> weak_ptr = GetWeakPtr();
  __block base::OnceClosure closure = CreatePendingTaskCompletionClosure();
  ProceduralBlock completion_block = ^{
    if (BrowsingDataRemoverImpl* strong_ptr = weak_ptr.get())
      strong_ptr->dummy_web_view_ = nil;
    std::move(closure).Run();
  };

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_VISITED_LINKS)) {
    ProceduralBlock previous_completion_block = completion_block;

    // TODO(crbug.com/557963): Purging the WKProcessPool is a workaround for
    // the fact that there is no public API to clear visited links in
    // WKWebView. Remove this workaround if/when that API is made public.
    // Note: Purging the WKProcessPool for clearing visisted links does have
    // the side-effect of also losing the in-memory cookies of WKWebView but
    // it is not a problem in practice since there is no UI to only have
    // visited links be removed but not cookies.
    completion_block = ^{
      if (BrowsingDataRemoverImpl* strong_ptr = weak_ptr.get()) {
        web::WKWebViewConfigurationProvider::FromBrowserState(
            strong_ptr->browser_state_)
            .Purge();
      }
      previous_completion_block();
    };
  }

  // TODO(crbug.com/661630): |dummy_web_view_| is created to allow
  // the -[WKWebsiteDataStore removeDataOfTypes:] API to access the cookiestore
  // and clear cookies. This is a workaround for
  // https://bugs.webkit.org/show_bug.cgi?id=149078. Remove this
  // workaround when it's not needed anymore.
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)) {
    if (!dummy_web_view_)
      dummy_web_view_ = [[WKWebView alloc] initWithFrame:CGRectZero];
  }

  NSDate* delete_begin_date =
      [NSDate dateWithTimeIntervalSince1970:delete_begin.ToDoubleT()];
  [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:data_types_to_remove
                                             modifiedSince:delete_begin_date
                                         completionHandler:completion_block];
}

void BrowsingDataRemoverImpl::OnKeywordsLoaded(base::Time delete_begin,
                                               base::Time delete_end,
                                               base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Deletes the entries from the model, and if we're not waiting on anything
  // else notifies observers and deletes this BrowsingDataRemoverImpl.
  TemplateURLService* model =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
  model->RemoveAutoGeneratedBetween(delete_begin, delete_end);
  template_url_subscription_.reset();
  std::move(callback).Run();
}

void BrowsingDataRemoverImpl::NotifyRemovalComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!removal_queue_.empty());

  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunnerHandle::Get();

  if (AccountConsistencyService* account_consistency_service =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              browser_state_)) {
    account_consistency_service->OnBrowsingDataRemoved();
  }

  {
    RemovalTask task = std::move(removal_queue_.front());
    // Only log clear browsing data on regular browsing mode. In OTR mode, only
    // few types of data are cleared and the rest is handled by deleting the
    // browser state, so logging in these cases will render the histogram not
    // useful.
    if (!browser_state_->IsOffTheRecord()) {
      base::TimeDelta delta = base::Time::Now() - task.task_started;
      bool is_deletion_start_earliest = task.delete_begin.is_null();
      bool is_deletion_end_now = task.delete_end.is_max();
      if (is_deletion_start_earliest && is_deletion_end_now) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "History.ClearBrowsingData.Duration.FullDeletion", delta);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "History.ClearBrowsingData.Duration.PartialDeletion", delta);
      }
    }
    removal_queue_.pop();

    // Schedule the task to be executed soon. This ensure that the IsRemoving()
    // value is correct when the callback is invoked.
    if (!task.callback.is_null()) {
      current_task_runner->PostTask(FROM_HERE, std::move(task.callback));
    }

    // Notify the observer that some browsing data has been removed.
    current_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&BrowsingDataRemoverImpl::NotifyBrowsingDataRemoved,
                       GetWeakPtr(), task.mask));
  }

  if (removal_queue_.empty()) {
    SetRemoving(false);
    return;
  }

  // Yield execution before executing the next removal task.
  current_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowsingDataRemoverImpl::RunNextTask, GetWeakPtr()));
}

void BrowsingDataRemoverImpl::OnTaskComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/305259): This should also observe session clearing (what
  // about other things such as passwords, etc.?) and wait for them to complete
  // before continuing.

  DCHECK_GT(pending_tasks_count_, 0);
  if (--pending_tasks_count_ > 0)
    return;

  NotifyRemovalComplete();
}

base::OnceClosure
BrowsingDataRemoverImpl::CreatePendingTaskCompletionClosure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++pending_tasks_count_;
  return base::BindOnce(&BrowsingDataRemoverImpl::OnTaskComplete, GetWeakPtr());
}

base::WeakPtr<BrowsingDataRemoverImpl> BrowsingDataRemoverImpl::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::WeakPtr<BrowsingDataRemoverImpl> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();

  // Immediately bind the weak pointer to the current sequence. This makes it
  // easier to discover potential use on the wrong sequence.
  weak_ptr.get();

  return weak_ptr;
}
