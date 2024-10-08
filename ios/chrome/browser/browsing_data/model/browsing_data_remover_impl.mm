// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_impl.h"

#import <set>
#import <string>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/location.h"
#import "base/logging.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/autofill/core/browser/address_data_manager.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/strike_databases/strike_database.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/browsing_data/core/cookie_or_cache_deletion_choice.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/language/core/browser/url_language_histogram.h"
#import "components/omnibox/browser/omnibox_prefs.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/model/strike_database_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_remover_helper.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_features.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/model/system_snapshots_cleaner.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/external_files/model/external_file_remover.h"
#import "ios/chrome/browser/external_files/model/external_file_remover_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/history/model/web_history_service_factory.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"
#import "ios/chrome/browser/language/model/url_language_histogram_factory.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/reading_list/model/reading_list_remover_helper.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_tmpl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "ios/net/http_cache_helper.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/browsing_data/browsing_data_removing_util.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/base/net_errors.h"
#import "net/cookies/cookie_store.h"
#import "net/http/transport_security_state.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_context_getter.h"
#import "url/gurl.h"

namespace {

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
      creation_range,
      base::BindOnce(&DeleteCallbackAdapter, std::move(callback)));
}

std::set<Browser*> GetAllBrowsersForProfile(ProfileIOS* profile) {
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  return browser_list->BrowsersOfType(BrowserList::BrowserType::kAll);
}

bool IsActivityIndicatorNeededAutomatic(bool is_off_the_record,
                                        BrowsingDataRemoveMask mask) {
  return !is_off_the_record &&
         IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_SITE_DATA);
}

bool IsActivityIndicatorNeeded(bool is_off_the_record,
                               BrowsingDataRemoveMask mask,
                               BrowsingDataRemover::RemovalParams params) {
  switch (params.show_activity_indicator) {
    case BrowsingDataRemover::ActivityIndicatorPolicy::kAutomatic:
      return IsActivityIndicatorNeededAutomatic(is_off_the_record, mask);
    case BrowsingDataRemover::ActivityIndicatorPolicy::kNoIndicator:
      return false;
    case BrowsingDataRemover::ActivityIndicatorPolicy::kForceIndicator:
      return true;
  }
  NOTREACHED();
}

bool IsWebStatesReloadNeeded(bool is_off_the_record,
                             BrowsingDataRemoveMask mask,
                             BrowsingDataRemover::RemovalParams params) {
  switch (params.reload_web_states) {
    case BrowsingDataRemover::WebStatesReloadPolicy::kAutomatic:
      return IsActivityIndicatorNeededAutomatic(is_off_the_record, mask);
    case BrowsingDataRemover::WebStatesReloadPolicy::kNoReload:
      return false;
    case BrowsingDataRemover::WebStatesReloadPolicy::kForceReload:
      return true;
  }
  NOTREACHED();
}

void CloseTabsHelper(base::WeakPtr<Browser> browser,
                     base::Time delete_begin,
                     base::Time delete_end,
                     const tabs_closure_util::WebStateIDToTime& tabs_info,
                     BrowsingDataRemover::RemovalParams params) {
  if (!browser) {
    return;
  }
  bool keep_active_tab =
      params.keep_active_tab ==
      BrowsingDataRemover::KeepActiveTabPolicy::kKeepActiveTab;
  tabs_closure_util::CloseTabs(browser->GetWebStateList(), delete_begin,
                               delete_end, tabs_info, keep_active_tab);
}

}  // namespace

BrowsingDataRemoverImpl::RemovalTask::RemovalTask(base::Time delete_begin,
                                                  base::Time delete_end,
                                                  BrowsingDataRemoveMask mask,
                                                  base::OnceClosure callback,
                                                  RemovalParams params)
    : delete_begin(delete_begin),
      delete_end(delete_end),
      mask(mask),
      callback(std::move(callback)),
      params(params) {}

BrowsingDataRemoverImpl::RemovalTask::RemovalTask(
    RemovalTask&& other) noexcept = default;

BrowsingDataRemoverImpl::RemovalTask::~RemovalTask() = default;

BrowsingDataRemoverImpl::BrowsingDataRemoverImpl(ProfileIOS* profile)
    : profile_(profile),
      context_getter_(profile->GetRequestContext()),
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);
}

BrowsingDataRemoverImpl::~BrowsingDataRemoverImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_removing_);
}

void BrowsingDataRemoverImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  profile_ = nullptr;

  if (is_removing_) {
    VLOG(1) << "BrowsingDataRemoverImpl shuts down with "
            << removal_queue_.size() << " pending tasks"
            << (pending_tasks_count_ ? " (including one in progress)" : "");

    SetRemoving(false);
  }

  UMA_HISTOGRAM_EXACT_LINEAR("History.ClearBrowsingData.TaskQueueAtShutdown",
                             removal_queue_.size(), 10);

  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

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
                                     base::OnceClosure callback,
                                     RemovalParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);

  // Should always remove something.
  DCHECK(mask != BrowsingDataRemoveMask::REMOVE_NOTHING);

  // In incognito, only data removal for all time is currently supported.
  DCHECK(!profile_->IsOffTheRecord() ||
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

  // Closing tabs should only be available through user action which is only
  // possible in non off the record.
  DCHECK(!IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::CLOSE_TABS) ||
         (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::CLOSE_TABS) &&
          !profile_->IsOffTheRecord()));

  browsing_data::RecordDeletionForPeriod(time_period);
  removal_queue_.emplace(browsing_data::CalculateBeginDeleteTime(time_period),
                         browsing_data::CalculateEndDeleteTime(time_period),
                         mask, std::move(callback), params);

  // If this is the only scheduled task, execute it immediately. Otherwise,
  // it will be automatically executed when all tasks scheduled before it
  // finish.
  if (removal_queue_.size() == 1) {
    SetRemoving(true);
    RunNextTask();
  }
}

void BrowsingDataRemoverImpl::RemoveInRange(base::Time start_time,
                                            base::Time end_time,
                                            BrowsingDataRemoveMask mask,
                                            base::OnceClosure callback,
                                            RemovalParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);

  // Should always remove something.
  DCHECK(mask != BrowsingDataRemoveMask::REMOVE_NOTHING);

  // In incognito, only data removal for all time is currently supported.
  DCHECK(!profile_->IsOffTheRecord());

  // Partial clearing of downloads, bookmarks or reading lists is not supported.
  DCHECK(!(
      IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_DOWNLOADS) ||
      IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_BOOKMARKS) ||
      IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_READING_LIST)));

  // Removing visited links requires clearing the cookies.
  DCHECK(
      IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES) ||
      !IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_VISITED_LINKS));

  // Closing tabs should only be available through user action which is only
  // possible in non off the record.
  DCHECK(!IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::CLOSE_TABS) ||
         (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::CLOSE_TABS) &&
          !profile_->IsOffTheRecord()));

  // browsing_data::RecordDeletionForPeriod(time_period);
  removal_queue_.emplace(start_time, end_time, mask, std::move(callback),
                         params);

  // If this is the only scheduled task, execute it immediately. Otherwise,
  // it will be automatically executed when all tasks scheduled before it
  // finish.
  if (removal_queue_.size() == 1) {
    SetRemoving(true);
    RunNextTask();
  }
}

void BrowsingDataRemoverImpl::SetCachedTabsInfo(
    tabs_closure_util::WebStateIDToTime cached_tabs_info) {
  cached_tabs_info_ = cached_tabs_info;
  cached_tabs_info_initialized_ = true;
}

void BrowsingDataRemoverImpl::RunNextTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!removal_queue_.empty());
  RemovalTask& removal_task = removal_queue_.front();
  removal_task.task_started = base::Time::Now();

  PrepareForRemoval(removal_task.mask, removal_task.params);
  RemoveImpl(removal_task.delete_begin, removal_task.delete_end,
             removal_task.mask, removal_task.params);
}

void BrowsingDataRemoverImpl::PrepareForRemoval(BrowsingDataRemoveMask mask,
                                                RemovalParams params) {
  if (!IsActivityIndicatorNeeded(profile_->IsOffTheRecord(), mask, params)) {
    return;
  }

  std::set<Browser*> all_browsers = GetAllBrowsersForProfile(profile_);

  for (Browser* browser : all_browsers) {
    CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
    // Not all browsers have a handler for the protocol
    // BrowserCoordinatorCommands.
    if ([dispatcher
            dispatchingForProtocol:@protocol(BrowserCoordinatorCommands)]) {
      id<BrowserCoordinatorCommands> handler =
          HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
      _activityOverlayCallback = [handler showActivityOverlay];
    }
  }
}

void BrowsingDataRemoverImpl::CleanupAfterRemoval(BrowsingDataRemoveMask mask,
                                                  RemovalParams params) {
  // Activates browsing and enables web views.
  // Must be called only on the main thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::set<Browser*> all_browsers = GetAllBrowsersForProfile(profile_);

  const bool is_off_the_record = profile_->IsOffTheRecord();
  const bool activity_indicator_needed =
      IsActivityIndicatorNeeded(is_off_the_record, mask, params);
  const bool is_webstate_reload_needed =
      IsWebStatesReloadNeeded(is_off_the_record, mask, params);

  for (Browser* browser : all_browsers) {
    if (is_webstate_reload_needed) {
      // User interaction needs to be disabled as a way to force reload all the
      // web states and to reset NTPs.
      WebUsageEnablerBrowserAgent::FromBrowser(browser)->SetWebUsageEnabled(
          false);
    }

    if (activity_indicator_needed) {
      CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
      // Not all browsers have a handler for the protocol
      // BrowserCoordinatorCommands.
      if ([dispatcher
              dispatchingForProtocol:@protocol(BrowserCoordinatorCommands)]) {
        _activityOverlayCallback.RunAndReset();
      }
    }

    if (is_webstate_reload_needed) {
      WebUsageEnablerBrowserAgent::FromBrowser(browser)->SetWebUsageEnabled(
          true);
    }

    if (TabUsageRecorderBrowserAgent* tab_usage_recorder =
            TabUsageRecorderBrowserAgent::FromBrowser(browser)) {
      tab_usage_recorder->RecordPrimaryBrowserChange(true);
    }
  }
}

void BrowsingDataRemoverImpl::RemoveImpl(base::Time delete_begin,
                                         base::Time delete_end,
                                         BrowsingDataRemoveMask mask,
                                         RemovalParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedClosureRunner synchronous_clear_operations(
      CreatePendingTaskCompletionClosure());

  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  // Note: Before adding any method below, make sure that it can finish clearing
  // browsing data even if `profile` is destroyed after this method call.

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_HISTORY)) {
    // Remove the screenshots taken by the system when backgrounding the
    // application. Partial removal based on timePeriod is not required.
    ClearIOSSnapshots(CreatePendingTaskCompletionClosure());

    // Remove all HTTPS-Only Mode allowlist decisions.
    HttpsUpgradeService* https_upgrade_service =
        HttpsUpgradeServiceFactory::GetForProfile(profile_);
    https_upgrade_service->ClearAllowlist(delete_begin, delete_end);
  }

  auto io_thread_task_runner = web::GetIOThreadTaskRunner({});

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)) {
    if (!profile_->IsOffTheRecord()) {
      // ClearBrowsingData_Cookies should not be reported when cookies are
      // cleared as part of an incognito browser shutdown.
      base::RecordAction(base::UserMetricsAction("ClearBrowsingData_Cookies"));
    }
    net::CookieDeletionInfo::TimeRange deletion_time_range =
        net::CookieDeletionInfo::TimeRange(delete_begin, delete_end);
    io_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ClearCookies, context_getter_, deletion_time_range,
            base::BindOnce(base::IgnoreResult(&base::TaskRunner::PostTask),
                           current_task_runner, FROM_HERE,
                           CreatePendingTaskCompletionClosure())));
    if (!profile_->IsOffTheRecord()) {
      GetApplicationContext()->GetSafeBrowsingService()->ClearCookies(
          deletion_time_range,
          base::BindOnce(base::IgnoreResult(&base::TaskRunner::PostTask),
                         current_task_runner, FROM_HERE,
                         CreatePendingTaskCompletionClosure()));
    }
  }

  // There is no need to clean the remaining types of data for off-the-record
  // ProfileIOS as no data is saved. Early return to avoid scheduling
  // unnecessary work.
  if (profile_->IsOffTheRecord()) {
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
        ios::HistoryServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);

    if (history_service) {
      base::RecordAction(base::UserMetricsAction("ClearBrowsingData_History"));
      history_service->DeleteLocalAndRemoteHistoryBetween(
          ios::WebHistoryServiceFactory::GetForProfile(profile_), delete_begin,
          delete_end, history::kNoAppIdFilter,
          CreatePendingTaskCompletionClosure(), &history_task_tracker_);
    }

    // Need to clear the host cache and accumulated speculative data, as it also
    // reveals some history: we have no mechanism to track when these items were
    // created, so we'll clear them all. Better safe than sorry.
    IOSChromeIOThread* ios_chrome_io_thread =
        GetApplicationContext()->GetIOSChromeIOThread();
    if (ios_chrome_io_thread) {
      io_thread_task_runner->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&IOSChromeIOThread::ClearHostCache,
                         base::Unretained(ios_chrome_io_thread)),
          CreatePendingTaskCompletionClosure());
    }

    // As part of history deletion we also delete the auto-generated keywords.
    // Because the TemplateURLService is shared between incognito and
    // non-incognito profiles, don't do this in incognito.
    if (!profile_->IsOffTheRecord()) {
      TemplateURLService* keywords_model =
          ios::TemplateURLServiceFactory::GetForProfile(profile_);
      if (keywords_model && !keywords_model->loaded()) {
        template_url_subscription_ = keywords_model->RegisterOnLoadedCallback(
            base::BindOnce(&BrowsingDataRemoverImpl::OnKeywordsLoaded,
                           GetWeakPtr(), delete_begin, delete_end,
                           CreatePendingTaskCompletionClosure()));
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
        IOSChromeTabRestoreServiceFactory::GetForProfile(profile_);
    if (tab_service) {
      tab_service->DeleteLastSession();
      tab_service->ClearEntries();
    }

    // Remove language histogram history.
    language::UrlLanguageHistogram* language_histogram =
        UrlLanguageHistogramFactory::GetForProfile(profile_);
    if (language_histogram) {
      language_histogram->ClearHistory(delete_begin, delete_end);
    }

    crash_helper::ClearReportsBetween(delete_begin, delete_end);
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_PASSWORDS)) {
    base::RecordAction(base::UserMetricsAction("ClearBrowsingData_Passwords"));
    password_manager::PasswordStoreInterface* profile_password_store =
        IOSChromeProfilePasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS)
            .get();

    if (profile_password_store) {
      // It doesn't matter whether any logins were removed so bool argument can
      // be omitted.
      profile_password_store->RemoveLoginsCreatedBetween(
          FROM_HERE, delete_begin, delete_end,
          IgnoreArgument<bool>(CreatePendingTaskCompletionClosure()));
    }

    password_manager::PasswordStoreInterface* account_password_store =
        IOSChromeAccountPasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS)
            .get();

    if (account_password_store) {
      account_password_store->RemoveLoginsCreatedBetween(
          FROM_HERE, delete_begin, delete_end,
          IgnoreArgument<bool>(CreatePendingTaskCompletionClosure()));
    }
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_FORM_DATA)) {
    base::RecordAction(base::UserMetricsAction("ClearBrowsingData_Autofill"));
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        ios::WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);

    if (web_data_service.get()) {
      web_data_service->RemoveFormElementsAddedBetween(delete_begin,
                                                       delete_end);

      // Clear out the Autofill StrikeDatabase in its entirety.
      autofill::StrikeDatabase* strike_database =
          autofill::StrikeDatabaseFactory::GetForProfile(profile_);
      if (strike_database) {
        strike_database->ClearAllStrikes();
      }

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForProfile(profile_);
      data_manager->address_data_manager().RemoveLocalProfilesModifiedBetween(
          delete_begin, delete_end);
      data_manager->payments_data_manager().RemoveLocalDataModifiedBetween(
          delete_begin, delete_end);

      // Ask for a call back when the above calls are finished.
      web_data_service->GetDBTaskRunner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(), CreatePendingTaskCompletionClosure());
    }
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_CACHE)) {
    base::RecordAction(base::UserMetricsAction("ClearBrowsingData_Cache"));
    ClearHttpCache(context_getter_, io_thread_task_runner, delete_begin,
                   delete_end,
                   base::BindOnce(&NetCompletionCallbackAdapter,
                                  CreatePendingTaskCompletionClosure()));
  }

  // Remove omnibox zero-suggest cache results.
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_CACHE) ||
      IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)) {
    profile_->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                    std::string());
    profile_->GetPrefs()->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                                  base::Value::Dict());
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_DOWNLOADS)) {
    ExternalFileRemover* external_file_remover =
        ExternalFileRemoverFactory::GetForProfile(profile_);
    if (external_file_remover) {
      external_file_remover->RemoveAfterDelay(
          base::Seconds(0), CreatePendingTaskCompletionClosure());
    }
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_BOOKMARKS)) {
    auto bookmarks_remover_helper =
        std::make_unique<BookmarkRemoverHelper>(profile_);
    auto* bookmarks_remover_helper_ptr = bookmarks_remover_helper.get();

    // Pass the ownership of BookmarkRemoverHelper to the callback. This is
    // safe as the callback is always invoked, even if ProfileIOS is
    // destroyed, and BookmarkRemoverHelper supports being deleted while the
    // callback is run.
    bookmarks_remover_helper_ptr->RemoveAllUserBookmarksIOS(
        FROM_HERE, base::BindOnce(&BookmarkClearedAdapter,
                                  std::move(bookmarks_remover_helper),
                                  CreatePendingTaskCompletionClosure()));
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_READING_LIST)) {
    auto reading_list_remover_helper =
        std::make_unique<reading_list::ReadingListRemoverHelper>(profile_);
    auto* reading_list_remover_helper_ptr = reading_list_remover_helper.get();

    // Pass the ownership of reading_list::ReadingListRemoverHelper to the
    // callback. This is safe as the callback is always invoked, even if
    // ProfileIOS is destroyed, and ReadingListRemoverHelper supports
    // being deleted while the callback is run..
    reading_list_remover_helper_ptr->RemoveAllUserReadingListItemsIOS(
        FROM_HERE, base::BindOnce(&ReadingListClearedAdapter,
                                  std::move(reading_list_remover_helper),
                                  CreatePendingTaskCompletionClosure()));
  }

  if (IsRemoveDataMaskSet(mask,
                          BrowsingDataRemoveMask::REMOVE_LAST_USER_ACCOUNT)) {
    // The user just changed the account and chose to clear the previously
    // existing data. As browsing data is being cleared, it is fine to clear the
    // last username, as there will be no data to be merged.
    profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastSyncingGaiaId);
    profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastSignedInUsername);
    profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastSyncingUsername);
  }

  // Remove stored zoom levels.
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_SITE_DATA)) {
    FontSizeTabHelper::ClearUserZoomPrefs(profile_->GetPrefs());
  }

  // Close tabs.
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::CLOSE_TABS)) {
    base::RecordAction(base::UserMetricsAction("ClearBrowsingData_Tabs"));
    MaybeFetchTabsInfoThenCloseTabs(delete_begin, delete_end, params);
  }

  // Always wipe accumulated network related data (TransportSecurityState and
  // HttpServerPropertiesManager data).
  profile_->ClearNetworkingHistorySince(delete_begin,
                                        CreatePendingTaskCompletionClosure());

  // Remove browsing data stored in WKWebsiteDataStore if necessary.
  RemoveDataFromWKWebsiteDataStore(delete_begin, mask);

  // Record the combined deletion of cookies and cache.
  browsing_data::CookieOrCacheDeletionChoice choice;
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_CACHE)) {
    choice =
        IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)
            ? browsing_data::CookieOrCacheDeletionChoice::kBothCookiesAndCache
            : browsing_data::CookieOrCacheDeletionChoice::kOnlyCache;
  } else {
    choice = IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)
                 ? browsing_data::CookieOrCacheDeletionChoice::kOnlyCookies
                 : browsing_data::CookieOrCacheDeletionChoice::
                       kNeitherCookiesNorCache;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCache", choice);
}

void BrowsingDataRemoverImpl::RemoveDataFromWKWebsiteDataStore(
    base::Time delete_begin,
    BrowsingDataRemoveMask mask) {
  web::ClearBrowsingDataMask types = web::ClearBrowsingDataMask::kRemoveNothing;
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_APPCACHE)) {
    types |= web::ClearBrowsingDataMask::kRemoveAppCache;
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)) {
    types |= web::ClearBrowsingDataMask::kRemoveCookies;
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_INDEXEDDB)) {
    types |= web::ClearBrowsingDataMask::kRemoveIndexedDB;
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_LOCAL_STORAGE)) {
    types |= web::ClearBrowsingDataMask::kRemoveLocalStorage;
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_WEBSQL)) {
    types |= web::ClearBrowsingDataMask::kRemoveWebSQL;
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_CACHE_STORAGE)) {
    types |= web::ClearBrowsingDataMask::kRemoveCacheStorage;
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_VISITED_LINKS)) {
    types |= web::ClearBrowsingDataMask::kRemoveVisitedLinks;
  }

  web::ClearBrowsingData(profile_, types, delete_begin,
                         CreatePendingTaskCompletionClosure());
}

void BrowsingDataRemoverImpl::OnKeywordsLoaded(base::Time delete_begin,
                                               base::Time delete_end,
                                               base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Deletes the entries from the model, and if we're not waiting on anything
  // else notifies observers and deletes this BrowsingDataRemoverImpl.
  TemplateURLService* model =
      ios::TemplateURLServiceFactory::GetForProfile(profile_);
  model->RemoveAutoGeneratedBetween(delete_begin, delete_end);
  template_url_subscription_ = {};
  std::move(callback).Run();
}

void BrowsingDataRemoverImpl::MaybeFetchTabsInfoThenCloseTabs(
    base::Time delete_begin,
    base::Time delete_end,
    RemovalParams params) {
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  SessionRestorationService* service =
      SessionRestorationServiceFactory::GetForProfile(profile_);
  for (Browser* browser : browser_list->BrowsersOfType(
           BrowserList::BrowserType::kRegularAndInactive)) {
    if (cached_tabs_info_initialized_) {
      current_task_runner->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&CloseTabsHelper, browser->AsWeakPtr(), delete_begin,
                         delete_end, cached_tabs_info_, params),
          CreatePendingTaskCompletionClosure());
    } else {
      service->LoadDataFromStorage(
          browser,
          base::BindRepeating(
              &tabs_closure_util::GetLastCommittedTimestampFromStorage),
          base::BindOnce(&BrowsingDataRemoverImpl::OnTabsInformationLoaded,
                         GetWeakPtr(), browser->AsWeakPtr(), delete_begin,
                         delete_end, params,
                         CreatePendingTaskCompletionClosure()));
    }
  }
}

void BrowsingDataRemoverImpl::OnTabsInformationLoaded(
    base::WeakPtr<Browser> weak_browser,
    base::Time delete_begin,
    base::Time delete_end,
    RemovalParams params,
    base::OnceClosure callback,
    tabs_closure_util::WebStateIDToTime result) {
  Browser* browser = weak_browser.get();
  if (browser) {
    tabs_closure_util::WebStateIDToTime tabs_info =
        tabs_closure_util::GetTabsInfoForCache(result, delete_begin,
                                               delete_end);
    bool keep_active_tab =
        params.keep_active_tab == KeepActiveTabPolicy::kKeepActiveTab;
    tabs_closure_util::CloseTabs(browser->GetWebStateList(), delete_begin,
                                 delete_end, tabs_info, keep_active_tab);
  }
  std::move(callback).Run();
}

void BrowsingDataRemoverImpl::NotifyRemovalComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!removal_queue_.empty());

  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  if (AccountConsistencyService* account_consistency_service =
          ios::AccountConsistencyServiceFactory::GetForProfile(profile_)) {
    account_consistency_service->OnBrowsingDataRemoved();
  }
  if (OptimizationGuideService* optimization_guide_service =
          OptimizationGuideServiceFactory::GetForProfile(profile_)) {
    optimization_guide_service->OnBrowsingDataRemoved();
  }

  {
    RemovalTask task = std::move(removal_queue_.front());
    // Only log clear browsing data on regular browsing mode. In OTR mode, only
    // few types of data are cleared and the rest is handled by deleting the
    // profile, so logging in these cases will render the histogram not
    // useful.
    if (!profile_->IsOffTheRecord()) {
      base::TimeDelta delta = base::Time::Now() - task.task_started;
      bool is_deletion_start_earliest = task.delete_begin.is_null();
      bool is_deletion_end_now = task.delete_end.is_max();
      if (is_deletion_start_earliest && is_deletion_end_now) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "History.ClearBrowsingData.Duration.FullDeletion", delta);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "History.ClearBrowsingData.Duration.TimeRangeDeletion", delta);
      }
    }
    removal_queue_.pop();

    CleanupAfterRemoval(task.mask, task.params);

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
    cached_tabs_info_initialized_ = false;
    return;
  }

  // Yield execution before executing the next removal task.
  current_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowsingDataRemoverImpl::RunNextTask, GetWeakPtr()));
}

void BrowsingDataRemoverImpl::OnTaskComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40336135): This should also observe session clearing (what
  // about other things such as passwords, etc.?) and wait for them to complete
  // before continuing.

  DCHECK_GT(pending_tasks_count_, 0);
  if (--pending_tasks_count_ > 0) {
    return;
  }

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
