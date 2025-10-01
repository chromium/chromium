// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/paths/paths_internal.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

// TODO(crbug.com/445963646): Add metrics logging to browser agent.
// TODO(crbug.com/447646545): Add test coverage for persist tab context

namespace {

constexpr std::string kPageContextPrefix = "page_context_";
constexpr std::string kProtoSuffix = ".proto";
constexpr std::string kPersistedTabContexts = "persisted_tab_contexts";

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
  if (storage_directory_path.empty()) {
    // TODO(crbug.com/445963646): Add metrics logging to browser agent.
    return;
  }

  base::FilePath file_path =
      GetContextFilePath(storage_directory_path, webstate_unique_id);

  if (!base::WriteFile(file_path, serialized_page_context)) {
    // TODO(crbug.com/445963646): Add metrics logging to browser agent.
  }
}

void DeleteContextFromStorage(const std::string& webstate_unique_id,
                              base::FilePath storage_directory_path) {
  if (storage_directory_path.empty()) {
    // TODO(crbug.com/445963646): Add metrics logging to browser agent.
    return;
  }

  base::FilePath file_path =
      GetContextFilePath(storage_directory_path, webstate_unique_id);

  if (!base::DeleteFile(file_path)) {
    // TODO(crbug.com/445963646): Add metrics logging to browser agent.
    return;
  }
}

// Creates the profile-specific directory in which serialized page contexts will
// be stored. This function is scheduled using the sequenced task runner to
// ensure it is created before any attempts are made to write, read or delete
// to/from the directory while improving performance by only creating it once.
void CreateStorageDirectory(const base::FilePath& storage_directory_path) {
  if (!base::CreateDirectory(storage_directory_path)) {
    // TODO(crbug.com/445963646): Add metrics logging to browser agent.
  }
}

}  // namespace

PersistTabContextBrowserAgent::PersistTabContextBrowserAgent(Browser* browser)
    : BrowserUserData(browser),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  ProfileIOS* profile = browser->GetProfile();
  CHECK(profile);
  base::FilePath cache_directory_path;
  ios::GetUserCacheDirectory(profile->GetStatePath(), &cache_directory_path);
  storage_directory_path_ = cache_directory_path.Append(kPersistedTabContexts);
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&CreateStorageDirectory,
                                                   storage_directory_path_));
  StartObserving(browser, Policy::kAccordingToFeature);
  persist_tab_context_state_agent_ = [[PersistTabContextStateAgent alloc]
      initWithTransitionCallback:
          base::BindRepeating(
              &PersistTabContextBrowserAgent::OnSceneActivationLevelChanged,
              weak_factory_.GetWeakPtr())];
  [browser->GetSceneState() addObserver:persist_tab_context_state_agent_];
}

PersistTabContextBrowserAgent::~PersistTabContextBrowserAgent() {
  StopObserving();
  [browser_->GetSceneState() removeObserver:persist_tab_context_state_agent_];
  page_context_wrapper_ = nil;
  web_state_observation_.Reset();
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
    const std::string& webstate_unique_id,
    PageContextWrapperCallbackResponse response) {
  if (!response.has_value()) {
    // TODO(crbug.com/445963646): Add metrics logging to browser agent.
    return;
  }
  // TODO(crbug.com/445963646): Add metrics logging to browser agent.

  std::string serialized_page_context;
  response.value()->SerializeToString(&serialized_page_context);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteContextToStorage, std::move(serialized_page_context),
                     webstate_unique_id, storage_directory_path_));
}

void PersistTabContextBrowserAgent::WasHidden(web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  // Cancel any ongoing page context operation.
  if (page_context_wrapper_) {
    page_context_wrapper_ = nil;
  }

  std::string webstate_unique_id =
      base::NumberToString(web_state->GetUniqueIdentifier().identifier());

  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state
      completionCallback:
          base::BindOnce(&PersistTabContextBrowserAgent::OnPageContextExtracted,
                         weak_factory_.GetWeakPtr(), webstate_unique_id)];
  [page_context_wrapper_ setShouldGetAnnotatedPageContent:YES];
  [page_context_wrapper_ setShouldGetInnerText:YES];
  [page_context_wrapper_ setIsLowPriorityExtraction:YES];
  [page_context_wrapper_ populatePageContextFieldsAsync];
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

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeleteContextFromStorage, webstate_unique_id,
                                storage_directory_path_));
}

void PersistTabContextBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  web_state_observation_.Reset();
  if (new_active) {
    web_state_observation_.Observe(new_active);
  }
}
