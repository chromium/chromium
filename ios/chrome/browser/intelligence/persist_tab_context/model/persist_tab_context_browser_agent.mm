// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"

#import "base/barrier_callback.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/metrics/persist_tab_context_metrics.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_bridge_service.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_bridge_service_factory.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/paths/paths_internal.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

namespace {

// Prefix used for filenames of persisted page context files.
constexpr std::string kPageContextPrefix = "page_context_";

// Suffix used for filenames of persisted page context files.
constexpr std::string kProtoSuffix = ".proto";

// The name of the subdirectory within the user's cache directory where
// persisted tab contexts are stored.
constexpr std::string kPersistedTabContexts = "persisted_tab_contexts";

// The delay applied before the PurgeExpiredPageContexts task is executed
// after the PersistTabContextBrowserAgent is initialized.
constexpr base::TimeDelta kPurgeTaskDelay = base::Seconds(3);

// Constructs the full file path for a given webstate_unique_id within the
// storage directory.
base::FilePath GetContextFilePath(const base::FilePath& storage_directory_path,
                                  const std::string& webstate_unique_id) {
  std::string filename = kPageContextPrefix + webstate_unique_id + kProtoSuffix;
  return storage_directory_path.Append(filename);
}

// Writes serialized page contexts containing web state URLs, titles, APCs and
// inner text to a user-specific storage path located in the app's cache.
void WriteContextToStorage(std::string_view serialized_page_context,
                           const std::string& webstate_unique_id,
                           base::FilePath storage_directory_path) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (storage_directory_path.empty()) {
    base::UmaHistogramEnumeration(
        kWriteTabContextResultHistogram,
        IOSPersistTabContextWriteResult::kStoragePathEmptyFailure);
    return;
  }

  base::FilePath file_path =
      GetContextFilePath(storage_directory_path, webstate_unique_id);

  if (!base::WriteFile(file_path, serialized_page_context)) {
    base::UmaHistogramEnumeration(
        kWriteTabContextResultHistogram,
        IOSPersistTabContextWriteResult::kWriteFailure);
    return;
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes(kPersistTabContextWriteTimeHistogram, elapsed_time);

  base::UmaHistogramEnumeration(kWriteTabContextResultHistogram,
                                IOSPersistTabContextWriteResult::kSuccess);

  base::UmaHistogramCounts10M(kPersistTabContextSizeHistogram,
                              serialized_page_context.size());
}

void DeleteContextFromStorage(const std::string& webstate_unique_id,
                              base::FilePath storage_directory_path) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (storage_directory_path.empty()) {
    base::UmaHistogramEnumeration(
        kDeleteTabContextResultHistogram,
        IOSPersistTabContextDeleteResult::kStoragePathEmptyFailure);
    return;
  }

  base::FilePath file_path =
      GetContextFilePath(storage_directory_path, webstate_unique_id);

  if (!base::DeleteFile(file_path)) {
    base::UmaHistogramEnumeration(
        kDeleteTabContextResultHistogram,
        IOSPersistTabContextDeleteResult::kDeleteFailure);
    return;
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes(kPersistTabContextDeleteTimeHistogram, elapsed_time);

  base::UmaHistogramEnumeration(kDeleteTabContextResultHistogram,
                                IOSPersistTabContextDeleteResult::kSuccess);
}

// Creates the profile-specific directory in which serialized page contexts will
// be stored. This function is scheduled using the sequenced task runner to
// ensure it is created before any attempts are made to write, read or delete
// to/from the directory while improving performance by only creating it once.
void CreateStorageDirectory(const base::FilePath& storage_directory_path) {
  if (base::CreateDirectory(storage_directory_path)) {
    base::UmaHistogramBoolean(kCreateDirectoryResultHistogram, true);
  } else {
    // Log the failure to UMA.
    base::UmaHistogramBoolean(kCreateDirectoryResultHistogram, false);
  }
}

// Reads the serialized proto data from the file. Returns std::nullopt on
// failure.
std::optional<std::string> ReadFileContents(
    const base::FilePath& storage_directory_path,
    const std::string& unique_id) {
  if (storage_directory_path.empty()) {
    base::UmaHistogramEnumeration(
        kReadTabContextResultHistogram,
        IOSPersistTabContextReadResult::kStoragePathEmptyFailure);
    return std::nullopt;
  }

  base::FilePath file_path =
      GetContextFilePath(storage_directory_path, unique_id);

  if (!base::PathExists(file_path)) {
    base::UmaHistogramEnumeration(
        kReadTabContextResultHistogram,
        IOSPersistTabContextReadResult::kFileNotFound);
    return std::nullopt;
  }

  std::string serialized_data;
  if (!base::ReadFileToString(file_path, &serialized_data)) {
    base::UmaHistogramEnumeration(kReadTabContextResultHistogram,
                                  IOSPersistTabContextReadResult::kReadFailure);
    return std::nullopt;
  }

  return serialized_data;
}

// Parses serialized data into a PageContext proto. Returns std::nullopt on
// failure.
std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
ParsePageContext(const std::string& serialized_data) {
  auto page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  if (!page_context->ParseFromString(serialized_data)) {
    base::UmaHistogramEnumeration(
        kReadTabContextResultHistogram,
        IOSPersistTabContextReadResult::kParseFailure);
    return std::nullopt;
  }

  return page_context;
}

// Reads and parses a page context from persistent storage using the given
// `webstate_unique_id`. The context is expected to be stored in a file named
// "page_context_<unique_id>.proto" within the profile-specific cache
// directory.
std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
ReadAndParseContextFromStorage(const base::FilePath& storage_directory_path,
                               const std::string& webstate_unique_id) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  std::optional<std::string> serialized_data =
      ReadFileContents(storage_directory_path, webstate_unique_id);
  if (!serialized_data) {
    DeleteContextFromStorage(webstate_unique_id, storage_directory_path);
    return std::nullopt;
  }

  std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
      page_context = ParsePageContext(*serialized_data);
  if (!page_context) {
    DeleteContextFromStorage(webstate_unique_id, storage_directory_path);
    return std::nullopt;
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes(kPersistTabContextReadTimeHistogram, elapsed_time);

  base::UmaHistogramEnumeration(kReadTabContextResultHistogram,
                                IOSPersistTabContextReadResult::kSuccess);

  return page_context;
}

// Performs multiple ReadAndParseContextFromStorage calls.
PersistTabContextBrowserAgent::PageContextMap DoMultipleContextReads(
    const base::FilePath& storage_directory_path,
    const std::vector<std::string>& webstate_unique_ids) {
  PersistTabContextBrowserAgent::PageContextMap result_map;
  for (const std::string& unique_id : webstate_unique_ids) {
    result_map[unique_id] =
        ReadAndParseContextFromStorage(storage_directory_path, unique_id);
  }

  return result_map;
}

// Calculates and logs the difference between the count of persisted page
// context files in `storage_directory_path` and the provided `web_state_count`.
// The result (`file_count` - `web_state_count`) is logged to the
// `kPersistTabContextStorageDifferenceHistogram` UMA metric.
void LogStorageDifference(base::FilePath storage_directory_path,
                          int web_state_count) {
  if (storage_directory_path.empty()) {
    return;
  }

  int file_count = 0;
  base::FileEnumerator enumerator(
      storage_directory_path, /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({kPageContextPrefix, "*", kProtoSuffix}));
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    file_count++;
  }

  // TODO(crbug.com/452926908) - The metric logged here for web state
  // differences can frequently be non-zero, making it difficult to definitively
  // confirm if web state management is always working as intended strictly
  // based on this metric. This is due to several potential race conditions:
  // 1.  Race conditions can occur between a background task counting web states
  // in one browser and concurrent tab closures happening in another browser
  // instance. This can lead to unexpected positive deltas. To improve metric
  // reliability and reduce these race conditions, consider refactoring to use a
  // keyed service instead of a BrowserAgent for this logic in the future.
  // 2.  If multiple browser agents are active, these startup timing issues
  // could potentially compound, leading to larger deltas.
  // 3.  Onboarding into the experiment with pre-existing tabs.
  int difference = file_count - web_state_count;
  base::UmaHistogramSparse(kPersistTabContextStorageDifferenceHistogram,
                           difference);
}

// Deletes page context proto files from `contexts_dir` whose last modified time
// is older than the set time to live.
void PurgeExpiredPageContexts(base::FilePath contexts_dir,
                              base::TimeDelta ttl) {
  if (!base::DirectoryExists(contexts_dir)) {
    return;
  }

  base::Time now = base::Time::Now();
  base::FileEnumerator enumerator(
      contexts_dir, /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({kPageContextPrefix, "*", kProtoSuffix}));
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    base::Time last_modified = info.GetLastModifiedTime();
    base::TimeDelta age = now - last_modified;

    if (age >= ttl) {
      bool deletion_result = base::DeleteFile(name);
      base::UmaHistogramBoolean(kPersistTabContextPurgeFileResultHistogram,
                                deletion_result);
    }
  }
}

// Recursively deletes the entire persisted contexts directory.
void DeletePersistedContextsDirectory(base::FilePath contexts_dir) {
  if (!base::DirectoryExists(contexts_dir)) {
    base::UmaHistogramEnumeration(
        kPersistTabContextDeleteDirectoryResultHistogram,
        IOSPersistTabContextDeleteDirectoryResult::kDirectoryNotFound);
    return;
  }

  bool delete_successful = base::DeletePathRecursively(contexts_dir);
  base::UmaHistogramEnumeration(
      kPersistTabContextDeleteDirectoryResultHistogram,
      delete_successful
          ? IOSPersistTabContextDeleteDirectoryResult::kSuccess
          : IOSPersistTabContextDeleteDirectoryResult::kDeleteFailure);
}

// Calculates the total number of WebStates across all Browser instances
// associated with the given `profile` which are eligibile to have their
// contexts persisted.
int GetPersistedWebStateCountForProfile(ProfileIOS* profile) {
  CHECK(profile);
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  CHECK(browser_list);

  const std::set<Browser*>& browsers =
      browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular);

  int total_web_state_count = 0;
  for (Browser* browser : browsers) {
    if (!browser || !browser->GetWebStateList()) {
      continue;
    }

    for (int i = 0; i < browser->GetWebStateList()->count(); i++) {
      web::WebState* web_state = browser->GetWebStateList()->GetWebStateAt(i);
      if (!CanExtractPageContextForWebState(web_state)) {
        continue;
      }

      total_web_state_count++;
    }
  }
  return total_web_state_count;
}

// Helper function to adapt the GetPageContentCache callback to the one used in
// this component, which uses unique_ptr.
void OnGetPageContent(
    base::OnceCallback<void(
        std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>)>
        callback,
    std::optional<optimization_guide::proto::PageContext> page_context) {
  if (!page_context) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(
      std::make_unique<optimization_guide::proto::PageContext>(*page_context));
}

}  // namespace

#pragma mark - Public

PersistTabContextBrowserAgent::PersistTabContextBrowserAgent(Browser* browser)
    : BrowserUserData(browser),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      use_page_content_cache_(GetPersistTabContextStorageType() ==
                              PersistTabStorageType::kSQLite),
      extract_context_on_page_load_(
          GetPersistTabContextExtractionTiming() ==
          PersistTabExtractionTiming::kOnWasHiddenAndPageLoad),
      store_inner_text_only_(GetPersistTabContextDataExtracted() ==
                             PersistTabDataExtracted::kInnerTextOnly) {
  ProfileIOS* profile = browser->GetProfile();
  CHECK(profile);
  base::FilePath cache_directory_path;
  ios::GetUserCacheDirectory(profile->GetStatePath(), &cache_directory_path);
  storage_directory_path_ = cache_directory_path.Append(kPersistedTabContexts);

  if (IsPersistTabContextEnabled()) {
    persist_tab_context_state_agent_ = [[PersistTabContextStateAgent alloc]
        initWithTransitionCallback:
            base::BindRepeating(
                &PersistTabContextBrowserAgent::OnSceneActivationLevelChanged,
                weak_factory_.GetWeakPtr())];
    [browser->GetSceneState() addObserver:persist_tab_context_state_agent_];
    StartObserving(browser, Policy::kAccordingToFeature);

    PrefService* prefs = profile->GetPrefs();
    CHECK(prefs);
    int total_web_state_count = GetPersistedWebStateCountForProfile(profile);

    if (use_page_content_cache_) {
      page_content_cache_service_ =
          PageContentCacheBridgeServiceFactory::GetForProfile(profile);

      // Purge the direct storage system. Intended for clients migrating to the
      // SQLite storage system from the direct filesystem.
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&DeletePersistedContextsDirectory,
                         storage_directory_path_),
          kPurgeTaskDelay);

      // Log the difference between web states and persisted files/entries.
      if (page_content_cache_service_) {
        page_content_cache_service_->GetAllTabIds(base::BindOnce(
            [](int web_state_count, std::vector<int64_t> cached_ids) {
              int difference = cached_ids.size() - web_state_count;
              base::UmaHistogramSparse(
                  kPersistTabContextStorageDifferenceHistogram, difference);
            },
            total_web_state_count));
      }
    } else {
      base::TimeDelta ttl = GetPersistedContextEffectiveTTL(prefs);
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CreateStorageDirectory, storage_directory_path_));
      // Schedule a cleanup task with a delay.
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PurgeExpiredPageContexts, storage_directory_path_,
                         ttl),
          kPurgeTaskDelay);

      // Log the difference between web states and persisted files/entries.
      task_runner_->PostTask(FROM_HERE, base::BindOnce(&LogStorageDifference,
                                                       storage_directory_path_,
                                                       total_web_state_count));
    }
  } else {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DeletePersistedContextsDirectory,
                       storage_directory_path_),
        kPurgeTaskDelay);
  }
}

PersistTabContextBrowserAgent::~PersistTabContextBrowserAgent() {
  if (persist_tab_context_state_agent_) {
    StopObserving();
    [browser_->GetSceneState() removeObserver:persist_tab_context_state_agent_];
  }
  page_context_wrapper_ = nil;
  web_state_observation_.Reset();
}

void PersistTabContextBrowserAgent::GetSingleContextAsync(
    const std::string& webstate_unique_id,
    base::OnceCallback<void(
        std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>)>
        callback) {
  if (use_page_content_cache_) {
    ReadAndParseContextFromContentCache(webstate_unique_id,
                                        std::move(callback));
  } else {
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&ReadAndParseContextFromStorage, storage_directory_path_,
                       webstate_unique_id),
        std::move(callback));
  }
}

void PersistTabContextBrowserAgent::GetMultipleContextsAsync(
    const std::vector<std::string>& webstate_unique_ids,
    base::OnceCallback<void(PageContextMap)> callback) {
  if (webstate_unique_ids.empty()) {
    std::move(callback).Run({});
    return;
  }

  if (use_page_content_cache_) {
    auto barrier_callback = base::BarrierCallback<ContextPair>(
        webstate_unique_ids.size(),
        base::BindOnce(&PersistTabContextBrowserAgent::OnAllContextsRetrieved,
                       weak_factory_.GetWeakPtr(), std::move(callback)));

    for (const std::string& unique_id : webstate_unique_ids) {
      auto id_binding_callback = base::BindOnce(
          &PersistTabContextBrowserAgent::OnSingleContextRetrieved,
          weak_factory_.GetWeakPtr(), unique_id, barrier_callback);

      ReadAndParseContextFromContentCache(unique_id,
                                          std::move(id_binding_callback));
    }
  } else {
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&DoMultipleContextReads, storage_directory_path_,
                       webstate_unique_ids),
        base::BindOnce(std::move(callback)));
  }
}

void PersistTabContextBrowserAgent::OnWebStateInserted(
    web::WebState* web_state) {
  // Nothing to do.
}

void PersistTabContextBrowserAgent::OnWebStateRemoved(
    web::WebState* web_state) {
  // Nothing to do.
}

void PersistTabContextBrowserAgent::OnWebStateDeleted(
    web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  std::string webstate_unique_id =
      base::NumberToString(web_state->GetUniqueIdentifier().identifier());

  if (use_page_content_cache_) {
    // TODO(crbug.com/467065000):  - Update the agents public methods to take in
    // WebStateID's, or at least int64's
    int64_t tab_id;
    if (!base::StringToInt64(webstate_unique_id, &tab_id)) {
      return;
    }
    DeleteContextFromContentCache(tab_id);
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeleteContextFromStorage, webstate_unique_id,
                                  storage_directory_path_));
  }
}

void PersistTabContextBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  web_state_observation_.Reset();
  if (new_active) {
    web_state_observation_.Observe(new_active);
  }
}

#pragma mark - WebStateObserver

void PersistTabContextBrowserAgent::WasHidden(web::WebState* web_state) {
  ExtractAndStoreContext(web_state);
}

void PersistTabContextBrowserAgent::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (extract_context_on_page_load_ &&
      load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    ExtractAndStoreContext(web_state);
  }
}

#pragma mark - Private

void PersistTabContextBrowserAgent::ExtractAndStoreContext(
    web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  std::string webstate_unique_id =
      base::NumberToString(web_state->GetUniqueIdentifier().identifier());

  // Check if the tab should be persisted, and skip + clean up any remaining
  // context if it shouldn't.
  if (!CanExtractPageContextForWebState(web_state)) {
    if (use_page_content_cache_) {
      DeleteContextFromContentCache(
          web_state->GetUniqueIdentifier().identifier());
    } else {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DeleteContextFromStorage, webstate_unique_id,
                         storage_directory_path_));
    }
  }

  // Cancel any ongoing page context operation.
  if (page_context_wrapper_) {
    page_context_wrapper_ = nil;
  }

  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state
      completionCallback:
          base::BindOnce(&PersistTabContextBrowserAgent::OnPageContextExtracted,
                         weak_factory_.GetWeakPtr(), web_state->GetWeakPtr())];
  if (store_inner_text_only_) {
    [page_context_wrapper_ setShouldGetAnnotatedPageContent:NO];
  } else {
    [page_context_wrapper_ setShouldGetAnnotatedPageContent:YES];
  }

  [page_context_wrapper_ setShouldGetInnerText:YES];
  [page_context_wrapper_ setIsLowPriorityExtraction:YES];
  [page_context_wrapper_ populatePageContextFieldsAsync];
}

void PersistTabContextBrowserAgent::OnSceneActivationLevelChanged(
    SceneActivationLevel level) {
  if (level != SceneActivationLevelBackground) {
    return;
  }

  web::WebState* active_web_state = web_state_observation_.GetSource();

  if (active_web_state) {
    WasHidden(active_web_state);
  }
}

void PersistTabContextBrowserAgent::OnPageContextExtracted(
    base::WeakPtr<web::WebState> weak_web_state,
    PageContextWrapperCallbackResponse response) {
  web::WebState* web_state = weak_web_state.get();
  if (!response.has_value() || !web_state) {
    return;
  }

  if (use_page_content_cache_) {
    WriteContextToContentCache(web_state, response);
  } else {
    std::string webstate_unique_id =
        base::NumberToString(web_state->GetUniqueIdentifier().identifier());
    std::string serialized_page_context;
    response.value()->SerializeToString(&serialized_page_context);

    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WriteContextToStorage,
                                  std::move(serialized_page_context),
                                  webstate_unique_id, storage_directory_path_));
  }
}

void PersistTabContextBrowserAgent::WriteContextToContentCache(
    web::WebState* web_state,
    const PageContextWrapperCallbackResponse& response) {
  if (!page_content_cache_service_) {
    return;
  }
  int64_t tab_id = web_state->GetUniqueIdentifier().identifier();
  const GURL& url = web_state->GetLastCommittedURL();
  const base::Time visit_timestamp = web_state->GetLastActiveTime();
  const base::Time extraction_timestamp = base::Time::Now();
  const optimization_guide::proto::PageContext& page_context =
      *response.value();

  page_content_cache_service_->CachePageContent(
      tab_id, url, visit_timestamp, extraction_timestamp, page_context);

  base::UmaHistogramCounts10M(kPersistTabContextSizeHistogram,
                              page_context.ByteSizeLong());
}

void PersistTabContextBrowserAgent::ReadAndParseContextFromContentCache(
    const std::string& webstate_unique_id,
    base::OnceCallback<void(
        std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>)>
        callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  // TODO(crbug.com/467065000): Update the agents public methods to take in
  // WebStateID's, or at least int64's
  int64_t tab_id;
  if (!base::StringToInt64(webstate_unique_id, &tab_id) ||
      !page_content_cache_service_) {
    base::UmaHistogramEnumeration(
        kReadTabContextResultHistogram,
        IOSPersistTabContextReadResult::kStoragePathEmptyFailure);
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Intercept callback for metrics
  auto wrapped_callback = base::BindOnce(
      [](base::OnceCallback<void(std::optional<std::unique_ptr<
                                     optimization_guide::proto::PageContext>>)>
             original_callback,
         base::TimeTicks start_time,
         std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
             result) {
        if (result) {
          base::UmaHistogramEnumeration(
              kReadTabContextResultHistogram,
              IOSPersistTabContextReadResult::kSuccess);
        } else {
          base::UmaHistogramEnumeration(
              kReadTabContextResultHistogram,
              IOSPersistTabContextReadResult::kFileNotFound);
        }

        base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
        base::UmaHistogramTimes(kPersistTabContextReadTimeHistogram,
                                elapsed_time);

        std::move(original_callback).Run(std::move(result));
      },
      std::move(callback), start_time);

  page_content_cache_service_->GetPageContentForTab(
      tab_id, base::BindOnce(&OnGetPageContent, std::move(wrapped_callback)));
}

void PersistTabContextBrowserAgent::DeleteContextFromContentCache(
    int64_t tab_id) {
  if (!page_content_cache_service_) {
    return;
  }
  page_content_cache_service_->RemovePageContentForTab(tab_id);
}

void PersistTabContextBrowserAgent::OnAllContextsRetrieved(
    base::OnceCallback<void(PageContextMap)> final_callback,
    std::vector<ContextPair> results) {
  PageContextMap result_map;
  for (auto& [id, context] : results) {
    result_map[id] = std::move(context);
  }
  std::move(final_callback).Run(std::move(result_map));
}

void PersistTabContextBrowserAgent::OnSingleContextRetrieved(
    std::string web_state_id,
    base::RepeatingCallback<void(ContextPair)> barrier_callback,
    std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
        result) {
  std::move(barrier_callback)
      .Run(std::make_pair(web_state_id, std::move(result)));
}
