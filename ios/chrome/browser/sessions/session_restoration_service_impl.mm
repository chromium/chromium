// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_service_impl.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/sessions/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_internal_util.h"
#import "ios/chrome/browser/sessions/session_io_request.h"
#import "ios/chrome/browser/sessions/session_loading.h"
#import "ios/chrome/browser/sessions/session_restoration_web_state_list_observer.h"
#import "ios/chrome/browser/sessions/web_state_list_serialization.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state.h"

namespace {

using WebStateMetadataStorageMap =
    ios::sessions::SessionStorage::WebStateMetadataStorageMap;

// Maximum size of session state NSData objects.
const int kMaxSessionState = 5 * 1024 * 1024;

// Deletes all files and directory in `path` not present in `items_to_keep`.
void DeleteUnknownContent(const base::FilePath& path,
                          const std::set<base::FilePath>& items_to_keep) {
  std::vector<base::FilePath> items_to_remove;
  base::FileEnumerator e(path, false, base::FileEnumerator::NAMES_ONLY);
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    if (!base::Contains(items_to_keep, name)) {
      items_to_remove.push_back(name);
    }
  }

  for (const auto& item : items_to_remove) {
    std::ignore = base::DeletePathRecursively(item);
  }
}

// Loads WebState storage from `web_state_dir` into `storage`.
web::proto::WebStateStorage LoadWebStateStorage(const base::FilePath& path) {
  web::proto::WebStateStorage storage;
  bool success = ios::sessions::ParseProto(path, storage);
  DCHECK(success);
  return storage;
}

// Loads Webstate native session from `web_state_dir`. It is okay if the file
// is missing, in that case the function return `nil`.
NSData* LoadWebStateSession(const base::FilePath& path) {
  return ios::sessions::ReadFile(path);
}

// Helper function used to construct a WebStateFactory callback for use
// with DeserializeWebStateList() function.
std::unique_ptr<web::WebState> CreateWebState(
    const base::FilePath& session_dir,
    ChromeBrowserState* browser_state,
    const WebStateMetadataStorageMap& mapping,
    web::WebStateID web_state_id) {
  auto iter = mapping.find(web_state_id);
  DCHECK(iter != mapping.end());

  const base::FilePath web_state_dir =
      ios::sessions::WebStateDirectory(session_dir, web_state_id);

  const base::FilePath web_state_storage_path =
      web_state_dir.Append(kWebStateStorageFilename);

  const base::FilePath web_state_session_path =
      web_state_dir.Append(kWebStateSessionFilename);

  auto web_state = web::WebState::CreateWithStorage(
      browser_state, web_state_id, iter->second,
      base::BindOnce(&LoadWebStateStorage, web_state_storage_path),
      base::BindOnce(&LoadWebStateSession, web_state_session_path));

  return web_state;
}

}  // anonymous namespace

// Class storing information about a WebStateList tracked by the
// SessionRestorationServiceImpl.
class SessionRestorationServiceImpl::WebStateListInfo {
 public:
  using WebStateListDirtyCallback =
      SessionRestorationWebStateListObserver::WebStateListDirtyCallback;

  // Constructor taking the `identifier` used to derive the path to the
  // storage on disk, the `web_state_list` to observe and a `callback`
  // invoked when the list or its content is considered dirty.
  WebStateListInfo(const std::string& identifier,
                   WebStateList* web_state_list,
                   WebStateListDirtyCallback callback);
  ~WebStateListInfo();

  // Getter and setter for the bool storing whether it is possible to
  // load session synchronously for this WebStateList.
  bool can_load_synchronously() const { return can_load_synchronously_; }
  void set_cannot_load_synchronously() { can_load_synchronously_ = false; }

  // Returns the `identifier` used to derive the path to the storage.
  const std::string& identifier() const { return identifier_; }

  // Adds `web_state_id` to the list of expected unrealized WebState. This
  // correspond to a WebState created via `CreateUnrealizedWebState()`.
  void add_expected_id(web::WebStateID web_state_id) {
    expected_ids_.insert(web_state_id);
  }

  // Removes `web_state_id` from the list of expected unrealized WebState.
  void remove_expected_id(web::WebStateID web_state_id) {
    expected_ids_.erase(web_state_id);
  }

  // Returns whether `web_state_id` is in the list of expected unrealized
  // WebState or not. This is used to determine whether the WebState should
  // be adopted (i.e. its storage copied from another Browser) or not.
  bool is_id_expected(web::WebStateID web_state_id) const {
    return base::Contains(expected_ids_, web_state_id);
  }

  // Returns the `observer`.
  SessionRestorationWebStateListObserver& observer() { return observer_; }

 private:
  const std::string identifier_;
  SessionRestorationWebStateListObserver observer_;
  std::set<web::WebStateID> expected_ids_;
  bool can_load_synchronously_ = true;
};

SessionRestorationServiceImpl::WebStateListInfo::WebStateListInfo(
    const std::string& identifier,
    WebStateList* web_state_list,
    WebStateListDirtyCallback callback)
    : identifier_(identifier), observer_(web_state_list, std::move(callback)) {
  DCHECK(!identifier_.empty());
}

SessionRestorationServiceImpl::WebStateListInfo::~WebStateListInfo() = default;

// Safety considerations of SessionRestorationServiceImpl:
//
// As can be seen from the API, SessionRestorationServiceImpl allow to load the
// data from disk synchronously but save all data on a background sequence. To
// ensure this is safe, WebStateListInfo store a boolean that records whether
// any file modification have been scheduled on the background sequence for the
// Browser or whether the session has already been loaded.
//
// As each Browser data is saved in a separate directory, it is safe to load
// synchronously from disk before any operation has been scheduled on the
// background sequence.
//
// There are two other operation that can still execute on the main sequence
// while task are in flight:
//  - LoadWebStateStorage(...)
//  - LoadWebStateSession(...)
//
// Those functions load the WebState's state and native session data from the
// disk. Since they both load a single file, it is safe to do it on the main
// sequence while operation are in flight on the background sequence as long
// as the write operations are atomic. This is the case of IORequest methods
// and of the Posix semantic.
//
// When closing a tab, the data is not deleted immediately as it would prevent
// re-opening the tab (e.g. to "undo" a close operation). Instead the data is
// only deleted when the session is loaded (since no WebState can reference
// the path to the file at this point).

SessionRestorationServiceImpl::SessionRestorationServiceImpl(
    base::TimeDelta save_delay,
    bool enable_pinned_web_states,
    const base::FilePath& storage_path,
    const scoped_refptr<base::SequencedTaskRunner> task_runner)
    : save_delay_(save_delay),
      enable_pinned_web_states_(enable_pinned_web_states),
      storage_path_(storage_path.Append(kSessionRestorationDirname)),
      task_runner_(task_runner) {
  DCHECK(storage_path_.IsAbsolute());
  DCHECK(task_runner_);
}

SessionRestorationServiceImpl::~SessionRestorationServiceImpl() {}

void SessionRestorationServiceImpl::Shutdown() {
  DCHECK(infos_.empty()) << "Disconnect() must be called for all Browser";
}

#pragma mark - SessionRestorationService

void SessionRestorationServiceImpl::AddObserver(
    SessionRestorationObserver* observer) {
  observers_.AddObserver(observer);
}

void SessionRestorationServiceImpl::RemoveObserver(
    SessionRestorationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SessionRestorationServiceImpl::SaveSessions() {
  SaveDirtySessions();
}

void SessionRestorationServiceImpl::SetSessionID(
    Browser* browser,
    const std::string& identifier) {
  WebStateList* web_state_list = browser->GetWebStateList();

  auto iterator = infos_.find(web_state_list);
  DCHECK(iterator == infos_.end());

  DCHECK(!base::Contains(identifiers_, identifier));
  identifiers_.insert(identifier);

  // It is safe to use base::Unretained(this) as the callback is never called
  // after SessionRestorationWebStateListObserver is destroyed. Those objects
  // are owned by the current instance, and destroyed before `this`.
  infos_.insert(std::make_pair(
      web_state_list,
      std::make_unique<WebStateListInfo>(
          identifier, web_state_list,
          base::BindRepeating(
              &SessionRestorationServiceImpl::MarkWebStateListDirty,
              base::Unretained(this)))));
}

void SessionRestorationServiceImpl::LoadSession(Browser* browser) {
  DCHECK(base::Contains(infos_, browser->GetWebStateList()));
  WebStateListInfo& info = *infos_[browser->GetWebStateList()];

  const base::TimeTicks start_time = base::TimeTicks::Now();

  // Check that LoadSession is only called once, and before any asynchronous
  // operation where started on that Browser. Then mark the Browser as no
  // longer safe for synchrounous operations.
  DCHECK(info.can_load_synchronously())
      << "LoadSession() must only be called on startup.";
  info.set_cannot_load_synchronously();

  for (auto& observer : observers_) {
    observer.WillStartSessionRestoration(browser);
  }

  // Load the session for the Browser.
  const base::FilePath session_dir = storage_path_.Append(info.identifier());
  ios::sessions::SessionStorage session =
      ios::sessions::LoadSessionStorage(session_dir);

  // Since this is the first session load, it is safe to delete any
  // unreferenced files from the Browser's storage path.
  std::set<base::FilePath> files_to_keep;
  files_to_keep.insert(session_dir.Append(kSessionMetadataFilename));
  for (const auto& item : session.session_metadata.items()) {
    files_to_keep.insert(ios::sessions::WebStateDirectory(
        session_dir, web::WebStateID::FromSerializedValue(item.identifier())));
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DeleteUnknownContent, session_dir,
                                        std::move(files_to_keep)));

  // Deserialize the session from storage.
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          browser->GetWebStateList(), std::move(session.session_metadata),
          SessionRestorationScope::kAll, enable_pinned_web_states_,
          base::BindRepeating(&CreateWebState, session_dir,
                              browser->GetBrowserState(),
                              std::move(session.web_state_storage_map)));

  // Loading the session may have marked the Browser as dirty (unless the
  // session was empty). There is no need to serialize the WebStates that
  // have just been restored (and it is not possible for most of them as
  // they are still unrealized), so clear the observer.
  info.observer().ClearDirty();
  dirty_web_state_lists_.erase(browser->GetWebStateList());

  DCHECK(dirty_web_state_lists_.empty());
  if (timer_.IsRunning()) {
    timer_.Stop();
  }

  for (auto& observer : observers_) {
    observer.SessionRestorationFinished(browser, restored_web_states);
  }

  // Record the time spent blocking the main thread to load the session.
  base::UmaHistogramTimes("Session.WebStates.LoadingTimeOnMainThread",
                          base::TimeTicks::Now() - start_time);
}

void SessionRestorationServiceImpl::Disconnect(Browser* browser) {
  SaveDirtySessions();
  DCHECK(dirty_web_state_lists_.empty());

  auto iterator = infos_.find(browser->GetWebStateList());
  DCHECK(iterator != infos_.end());

  WebStateListInfo& info = *iterator->second;
  DCHECK(base::Contains(identifiers_, info.identifier()));
  identifiers_.erase(info.identifier());

  infos_.erase(iterator);
}

std::unique_ptr<web::WebState>
SessionRestorationServiceImpl::CreateUnrealizedWebState(
    Browser* browser,
    web::proto::WebStateStorage storage) {
  auto iterator = infos_.find(browser->GetWebStateList());
  DCHECK(iterator != infos_.end());

  // Create the unique identifier for the new WebState and mark it as
  // expected with the WebStateListInfo (since it cannot be adopted).
  const web::WebStateID web_state_id = web::WebStateID::NewUnique();

  WebStateListInfo& info = *iterator->second;
  info.add_expected_id(web_state_id);

  // Schedule saving the storage and metadata for the created WebState
  // to disk before creating it, to ensure the data is available after
  // the next application restart even if the WebState never transition
  // to the realised state.
  const base::FilePath web_state_dir = ios::sessions::WebStateDirectory(
      storage_path_.Append(info.identifier()), web_state_id);

  // Create requests to serialize WebState storage and metadata storage,
  // and then post them to the background sequence.
  web::proto::WebStateMetadataStorage metadata;
  metadata.Swap(storage.mutable_metadata());

  ios::sessions::IORequestList requests;
  requests.push_back(std::make_unique<ios::sessions::WriteProtoIORequest>(
      web_state_dir.Append(kWebStateMetadataStorageFilename),
      std::make_unique<web::proto::WebStateMetadataStorage>(metadata)));
  requests.push_back(std::make_unique<ios::sessions::WriteProtoIORequest>(
      web_state_dir.Append(kWebStateStorageFilename),
      std::make_unique<web::proto::WebStateStorage>(storage)));

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ios::sessions::ExecuteIORequests, std::move(requests)));

  // Create the WebState with callback that return the data from memory. This
  // ensure there is no race condition while trying to read the data from the
  // main thread while it is being written to disk on a background thread.
  return web::WebState::CreateWithStorage(
      browser->GetBrowserState(), web_state_id, std::move(metadata),
      base::ReturnValueOnce(std::move(storage)),
      base::ReturnValueOnce<NSData*>(nil));
}

#pragma mark - Private

void SessionRestorationServiceImpl::MarkWebStateListDirty(
    WebStateList* web_state_list) {
  dirty_web_state_lists_.insert(web_state_list);
  if (!timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE, save_delay_,
        base::BindRepeating(&SessionRestorationServiceImpl::SaveDirtySessions,
                            base::Unretained(this)));
  }
}

void SessionRestorationServiceImpl::SaveDirtySessions() {
  if (timer_.IsRunning()) {
    timer_.Stop();
  }

  if (dirty_web_state_lists_.empty()) {
    return;
  }

  const base::TimeTicks start_time = base::TimeTicks::Now();

  ios::sessions::IORequestList requests;

  // Create a map of orphaned WebStates (i.e. "unrealized" WebStates detached
  // from a WebStateList).
  std::map<web::WebStateID, std::string> orphaned_map;
  for (WebStateList* web_state_list : dirty_web_state_lists_) {
    DCHECK(base::Contains(infos_, web_state_list));
    WebStateListInfo& info = *infos_[web_state_list];

    const auto& detached_web_states = info.observer().detached_web_states();
    if (!detached_web_states.empty()) {
      for (const auto web_state_id : detached_web_states) {
        orphaned_map.insert(std::make_pair(web_state_id, info.identifier()));
      }
    }
  }

  // Handle adopted WebStates (i.e. "unrealized" WebStates inserted into a
  // WebStateList).
  for (WebStateList* web_state_list : dirty_web_state_lists_) {
    DCHECK(base::Contains(infos_, web_state_list));
    WebStateListInfo& info = *infos_[web_state_list];

    const auto& inserted_web_states = info.observer().inserted_web_states();
    if (!inserted_web_states.empty()) {
      const base::FilePath dest_dir = storage_path_.Append(info.identifier());

      for (const auto web_state_id : inserted_web_states) {
        // Check whether the `web_state_id` is expected. If this is the case,
        // then `CreateUnrealizedWebState()` took care of scheduling tasks to
        // save its state to disk and there is nothing to do here.
        if (info.is_id_expected(web_state_id)) {
          info.remove_expected_id(web_state_id);
          continue;
        }

        // If the `web_state_id` is not expected, then it must be adopted
        // from another Browser, thus needs to be in the `orphaned_map`.
        DCHECK(base::Contains(orphaned_map, web_state_id));
        const base::FilePath from_dir =
            storage_path_.Append(orphaned_map[web_state_id]);

        requests.push_back(std::make_unique<ios::sessions::CopyPathIORequest>(
            ios::sessions::WebStateDirectory(from_dir, web_state_id),
            ios::sessions::WebStateDirectory(dest_dir, web_state_id)));
      }
    }
  }

  // Handle dirty WebStateLists and WebStates.
  for (WebStateList* web_state_list : dirty_web_state_lists_) {
    DCHECK(base::Contains(infos_, web_state_list));
    WebStateListInfo& info = *infos_[web_state_list];

    // Asynchronous operation will be scheduled for this Browser, so it is
    // no longer safe to perform synchronous operation on it anymore.
    info.set_cannot_load_synchronously();

    SessionRestorationWebStateListObserver& observer = info.observer();
    DCHECK(observer.is_web_state_list_dirty() ||
           !observer.dirty_web_states().empty());

    const base::FilePath dest_dir = storage_path_.Append(info.identifier());

    // Serialize the state of dirty WebState before serializing the metadata
    // for the WebStateList. This ensures that metadata is always referring
    // to WebStates that have been saved.
    const auto& dirty_web_states = observer.dirty_web_states();
    for (web::WebState* web_state : dirty_web_states) {
      const base::FilePath web_state_dir = ios::sessions::WebStateDirectory(
          dest_dir, web_state->GetUniqueIdentifier());

      // Serialize the WebState to protobuf message.
      auto storage = std::make_unique<web::proto::WebStateStorage>();
      web_state->SerializeToProto(*storage);

      // Extract the metadata from `storage` to save it in its own file.
      // The metadata must be non-null at this point (since at least the
      // creation time or last active time will be non-default).
      auto metadata = base::WrapUnique(storage->release_metadata());
      DCHECK(metadata);

      // Create requests to serialize both `metadata` and `storage`.
      requests.push_back(std::make_unique<ios::sessions::WriteProtoIORequest>(
          web_state_dir.Append(kWebStateMetadataStorageFilename),
          std::move(metadata)));
      requests.push_back(std::make_unique<ios::sessions::WriteProtoIORequest>(
          web_state_dir.Append(kWebStateStorageFilename), std::move(storage)));

      // Try to serialize the native session data, but abort if too large.
      // In that case, the old data is deleted (deleting a non-existing
      // file is not a failure).
      NSData* data = web_state->SessionStateData();
      if (data && data.length <= kMaxSessionState) {
        requests.push_back(std::make_unique<ios::sessions::WriteDataIORequest>(
            web_state_dir.Append(kWebStateSessionFilename), data));
      } else {
        requests.push_back(std::make_unique<ios::sessions::DeletePathIORequest>(
            web_state_dir.Append(kWebStateSessionFilename)));
      }
    }

    // Serialize the state of the WebStateList if it is considered dirty.
    if (observer.is_web_state_list_dirty()) {
      auto storage = std::make_unique<ios::proto::WebStateListStorage>();
      SerializeWebStateList(*web_state_list, *storage);

      requests.push_back(std::make_unique<ios::sessions::WriteProtoIORequest>(
          dest_dir.Append(kSessionMetadataFilename), std::move(storage)));
    }

    observer.ClearDirty();
  }

  // Post the IORequests on the background sequence as writing to disk
  // can block.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ios::sessions::ExecuteIORequests, std::move(requests)));

  dirty_web_state_lists_.clear();

  // Record the time spent blocking the main thread to save the session.
  base::UmaHistogramTimes("Session.WebStates.SavingTimeOnMainThread",
                          base::TimeTicks::Now() - start_time);
}
