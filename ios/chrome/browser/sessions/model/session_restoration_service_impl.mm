// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_service_impl.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/ranges/algorithm.h"
#import "ios/chrome/browser/sessions/model/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/session_internal_util.h"
#import "ios/chrome/browser/sessions/model/session_io_request.h"
#import "ios/chrome/browser/sessions/model/session_loading.h"
#import "ios/chrome/browser/sessions/model/session_restoration_web_state_list_observer.h"
#import "ios/chrome/browser/sessions/model/web_state_list_serialization.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state.h"

namespace {

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
  std::ignore = ios::sessions::ParseProto(path, storage);
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
    ProfileIOS* profile,
    web::WebStateID web_state_id,
    web::proto::WebStateMetadataStorage metadata) {
  const base::FilePath web_state_dir =
      ios::sessions::WebStateDirectory(session_dir, web_state_id);

  const base::FilePath web_state_storage_path =
      web_state_dir.Append(kWebStateStorageFilename);

  const base::FilePath web_state_session_path =
      web_state_dir.Append(kWebStateSessionFilename);

  auto web_state = web::WebState::CreateWithStorage(
      profile, web_state_id, std::move(metadata),
      base::BindOnce(&LoadWebStateStorage, web_state_storage_path),
      base::BindOnce(&LoadWebStateSession, web_state_session_path));

  return web_state;
}

// Delete data for discarded sessions with `identifiers` in `storage_path`
// on a background thread.
void DeleteDataForSessions(const base::FilePath& storage_path,
                           const std::set<std::string>& identifiers) {
  for (const std::string& identifier : identifiers) {
    const base::FilePath path = storage_path.Append(identifier);
    std::ignore = ios::sessions::DeleteRecursively(path);
  }
}

// An output iterator that counts how many time it has been incremented.
// Allows to check if sets has non-empty intersection without allocating.
template <typename T1, typename T2>
struct CountingOutputIterator {
  CountingOutputIterator& operator++() {
    ++count;
    return *this;
  }
  CountingOutputIterator& operator++(int) {
    ++count;
    return *this;
  }

  CountingOutputIterator& operator*() { return *this; }
  CountingOutputIterator& operator=(const T1&) { return *this; }
  CountingOutputIterator& operator=(const T2&) { return *this; }

  uint32_t count = 0;
};

// Override of CountingOutputIterator<T1, T2> when types are identical.
template <typename T>
struct CountingOutputIterator<T, T> {
  CountingOutputIterator& operator++() {
    ++count;
    return *this;
  }
  CountingOutputIterator& operator++(int) {
    ++count;
    return *this;
  }

  CountingOutputIterator& operator*() { return *this; }
  CountingOutputIterator& operator=(const T&) { return *this; }

  uint32_t count = 0;
};

// Returns whether the two sets have non-empty intersection.
template <typename Range1, typename Range2>
constexpr bool HasIntersection(Range1&& range1, Range2&& range2) {
  auto result = base::ranges::set_intersection(
      std::forward<Range1>(range1), std::forward<Range2>(range2),
      CountingOutputIterator<decltype(*range1.begin()),
                             decltype(*range2.begin())>{});
  return result.count != 0;
}

// Returns a WebStateMetadataMap from `storage`.
WebStateMetadataMap MetadataMapFromStorage(
    const ios::proto::WebStateListStorage& storage) {
  WebStateMetadataMap result;
  for (const auto& item : storage.items()) {
    DCHECK(web::WebStateID::IsValidValue(item.identifier()));
    const web::WebStateID web_state_id =
        web::WebStateID::FromSerializedValue(item.identifier());

    result.insert(std::make_pair(web_state_id, item.metadata()));
  }
  return result;
}

// Updates `metadata_map` to contains data for all items in `web_state_list`
// and only those items. It will remove irrelevant mappings and add missing
// ones.
void UpdateMetadataMap(WebStateMetadataMap& metadata_map,
                       const WebStateList* web_state_list) {
  WebStateMetadataMap old_metadata_map;
  std::swap(metadata_map, old_metadata_map);

  const int count = web_state_list->count();
  for (int index = 0; index < count; ++index) {
    const web::WebState* web_state = web_state_list->GetWebStateAt(index);
    const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
    auto iter = old_metadata_map.find(web_state_id);

    web::proto::WebStateMetadataStorage storage;
    if (iter != old_metadata_map.end()) {
      storage = std::move(iter->second);
    } else {
      web_state->SerializeMetadataToProto(storage);
    }

    metadata_map.insert(std::make_pair(web_state_id, std::move(storage)));
  }

  DCHECK_EQ(metadata_map.size(), static_cast<size_t>(count));
}

// Callback invoked for each WebState by `IterateDataForSessionAtPath`.
using SessionDataIterator =
    base::RepeatingCallback<void(web::WebStateID, web::proto::WebStateStorage)>;

// Loads data for session at `session_dir` invoking `iterator` for each
// WebState.
void IterateDataForSessionAtPath(const base::FilePath& session_dir,
                                 const SessionDataIterator& iterator) {
  const ios::proto::WebStateListStorage session =
      ios::sessions::LoadSessionStorage(session_dir);

  for (const auto& item : session.items()) {
    DCHECK(web::WebStateID::IsValidValue(item.identifier()));
    web::WebStateID web_state_id =
        web::WebStateID::FromSerializedValue(item.identifier());

    const base::FilePath web_state_storage_path =
        ios::sessions::WebStateDirectory(session_dir, web_state_id)
            .Append(kWebStateStorageFilename);

    iterator.Run(web_state_id, LoadWebStateStorage(web_state_storage_path));
  }
}

}  // anonymous namespace

// Information about an orphaned WebState.
struct SessionRestorationServiceImpl::OrphanInfo {
  std::string session_id;
  web::proto::WebStateMetadataStorage metadata;
};

// Class storing information about a WebStateList tracked by the
// SessionRestorationServiceImpl.
class SessionRestorationServiceImpl::WebStateListInfo {
 public:
  using WebStateListDirtyCallback =
      SessionRestorationWebStateListObserver::WebStateListDirtyCallback;

  // Constructor taking the `identifier` used to derive the path to the
  // storage on disk, the `web_state_list` to observe and a `callback`
  // invoked when the list or its content is considered dirty.
  //
  // If `original_info` is not null, then this objects corresponds to a
  // backup Browser (see SessionRestorationService::AttachBackup(...) for
  // more details). The pointer is used to represents the `has_backup` and
  // to ensure the objects are destroyed in the correct order.
  WebStateListInfo(const std::string& identifier,
                   WebStateList* web_state_list,
                   WebStateListInfo* original_info,
                   WebStateListDirtyCallback callback);
  ~WebStateListInfo();

  // Getter and setter for the bool storing whether it is possible to
  // load session synchronously for this WebStateList.
  bool can_load_synchronously() const { return can_load_synchronously_; }
  void set_cannot_load_synchronously() { can_load_synchronously_ = false; }

  // Returns the `identifier` used to derive the path to the storage.
  const std::string& identifier() const { return identifier_; }

  // Returns whether the Browser is registered as backup for another Browser.
  bool is_backup() const { return original_info_.get() != nullptr; }

  // Returns whether the Browser has an attached backup.
  bool has_backup() const { return backup_info_.get() != nullptr; }

  // Returns the `observer`.
  SessionRestorationWebStateListObserver& observer() { return observer_; }

  // Returns the WebStateMetadataMap.
  WebStateMetadataMap& metadata_map() { return metadata_map_; }

 private:
  const std::string identifier_;
  WebStateMetadataMap metadata_map_;
  SessionRestorationWebStateListObserver observer_;
  bool can_load_synchronously_ = true;
  raw_ptr<WebStateListInfo> original_info_;
  raw_ptr<WebStateListInfo> backup_info_;
};

SessionRestorationServiceImpl::WebStateListInfo::WebStateListInfo(
    const std::string& identifier,
    WebStateList* web_state_list,
    WebStateListInfo* original_info,
    WebStateListDirtyCallback callback)
    : identifier_(identifier),
      observer_(web_state_list, std::move(callback)),
      original_info_(original_info) {
  DCHECK(!identifier_.empty());
  if (original_info_) {
    DCHECK(!original_info_->has_backup());
    original_info_->backup_info_ = this;
  }
}

SessionRestorationServiceImpl::WebStateListInfo::~WebStateListInfo() {
  DCHECK(!backup_info_);
  if (original_info_) {
    DCHECK_EQ(original_info_->backup_info_.get(), this);
    original_info_->backup_info_ = nullptr;
    original_info_ = nullptr;
  }
}

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
    bool enable_tab_groups,
    const base::FilePath& storage_path,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : save_delay_(save_delay),
      enable_pinned_web_states_(enable_pinned_web_states),
      enable_tab_groups_(enable_tab_groups),
      storage_path_(storage_path.Append(kSessionRestorationDirname)),
      task_runner_(task_runner) {
  DCHECK(storage_path_.IsAbsolute());
  DCHECK(task_runner_);
}

SessionRestorationServiceImpl::~SessionRestorationServiceImpl() {}

void SessionRestorationServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(infos_.empty()) << "Disconnect() must be called for all Browser";
}

#pragma mark - SessionRestorationService

void SessionRestorationServiceImpl::AddObserver(
    SessionRestorationObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void SessionRestorationServiceImpl::RemoveObserver(
    SessionRestorationObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void SessionRestorationServiceImpl::SaveSessions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SaveDirtySessions();
}

void SessionRestorationServiceImpl::ScheduleSaveSessions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nothing to do, the service automatically schedule a save as soon
  // as changes are detected.
}

void SessionRestorationServiceImpl::SetSessionID(
    Browser* browser,
    const std::string& identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WebStateList* web_state_list = browser->GetWebStateList();

  DCHECK(!base::Contains(infos_, web_state_list));
  DCHECK(!base::Contains(identifiers_, identifier));
  identifiers_.insert(identifier);

  // It is safe to use base::Unretained(this) as the callback is never called
  // after SessionRestorationWebStateListObserver is destroyed. Those objects
  // are owned by the current instance, and destroyed before `this`.
  infos_.insert(std::make_pair(
      web_state_list,
      std::make_unique<WebStateListInfo>(
          identifier, web_state_list, /*original_info=*/nullptr,
          base::BindRepeating(
              &SessionRestorationServiceImpl::MarkWebStateListDirty,
              base::Unretained(this)))));
}

void SessionRestorationServiceImpl::LoadSession(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(infos_, browser->GetWebStateList()));
  WebStateListInfo& info = *infos_[browser->GetWebStateList()];
  DCHECK(!info.is_backup());

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
  ios::proto::WebStateListStorage session =
      ios::sessions::LoadSessionStorage(session_dir);

  // Updates `info`'s WebStateMetadataMap from `session`.
  WebStateMetadataMap& metadata_map = info.metadata_map();
  metadata_map = MetadataMapFromStorage(session);

  // Since this is the first session load, it is safe to delete any
  // unreferenced files from the Browser's storage path.
  std::set<base::FilePath> files_to_keep;
  files_to_keep.insert(session_dir.Append(kSessionMetadataFilename));
  for (const auto& item : session.items()) {
    files_to_keep.insert(ios::sessions::WebStateDirectory(
        session_dir, web::WebStateID::FromSerializedValue(item.identifier())));
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DeleteUnknownContent, session_dir,
                                        std::move(files_to_keep)));

  // Deserialize the session from storage.
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(browser->GetWebStateList(), std::move(session),
                              enable_pinned_web_states_, enable_tab_groups_,
                              base::BindRepeating(&CreateWebState, session_dir,
                                                  browser->GetProfile()));

  // Loading the session may have dropped some items, so clean the metadata map.
  UpdateMetadataMap(metadata_map, browser->GetWebStateList());

  // Loading the session may have marked the Browser as dirty (unless the
  // session was empty). There is no need to serialize the WebStates that
  // have just been restored (and it is not possible for most of them as
  // they are still unrealized), so clear the observer.
  info.observer().ClearDirty();
  dirty_web_state_lists_.erase(browser->GetWebStateList());

  // If multiple windows are open, it is possible for some other Browsers
  // to be dirty. Check if this is the case or not. If there are no dirty
  // Browsers, cancel the timer.
  if (dirty_web_state_lists_.empty()) {
    if (timer_.IsRunning()) {
      timer_.Stop();
    }
  }

  for (auto& observer : observers_) {
    observer.SessionRestorationFinished(browser, restored_web_states);
  }

  // Record the time spent blocking the main thread to load the session.
  base::UmaHistogramTimes(kSessionHistogramLoadingTime,
                          base::TimeTicks::Now() - start_time);
}

void SessionRestorationServiceImpl::LoadWebStateStorage(
    Browser* browser,
    web::WebState* web_state,
    WebStateStorageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!web_state->IsRealized());
  auto iterator = infos_.find(browser->GetWebStateList());
  if (iterator == infos_.end()) {
    return;
  }

  // If there is any pending requests, schedule them immediately, as they may
  // be pending requests to save the data for the WebState if it has just been
  // created with CreateUnrealizedWebState(...).
  if (!pending_requests_.empty()) {
    ios::sessions::IORequestList requests;
    std::swap(requests, pending_requests_);

    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ios::sessions::ExecuteIORequests, std::move(requests)));
  }

  // Updates the orphan map.
  UpdateOrphanInfoMap();

  // This method is usually called after a WebState has been detached. It may
  // be called before the pending changes have been committed to disk thus it
  // needs to partially replicate the logic from SaveDirtySessions(...) to
  // track the location of the WebState data.
  std::string session_id;

  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  auto iter = orphaned_map_.find(web_state_id);
  if (iter != orphaned_map_.end()) {
    session_id = iter->second.session_id;
  } else {
    // If the WebState is not up for adoption, assume its data is available in
    // `browser`. We cannot check whether it is in the Browser's WebStateList
    // as this method is usually called after the WebState has been detached.
    WebStateListInfo& info = *iterator->second;
    const auto& inserted_web_states = info.observer().inserted_web_states();
    DCHECK(!base::Contains(inserted_web_states, web_state_id));
    session_id = info.identifier();
  }

  DCHECK(!session_id.empty());
  const base::FilePath web_state_dir = ios::sessions::WebStateDirectory(
      storage_path_.Append(session_id), web_state_id);
  const base::FilePath storage_path =
      web_state_dir.Append(kWebStateStorageFilename);

  // Post the task to read the data from disk. It will execute after all the
  // pending requests, so if the WebState has recently been created by calling
  // CreateUnrealizedWebState(...), the data should be available.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&::LoadWebStateStorage, storage_path),
      std::move(callback));
}

void SessionRestorationServiceImpl::AttachBackup(Browser* browser,
                                                 Browser* backup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WebStateList* web_state_list = backup->GetWebStateList();

  DCHECK(!base::Contains(infos_, web_state_list));

  auto iterator = infos_.find(browser->GetWebStateList());
  DCHECK(iterator != infos_.end());
  WebStateListInfo* info = iterator->second.get();

  DCHECK(!info->is_backup());
  DCHECK(!info->has_backup());

  // It is safe to use base::Unretained(this) as the callback is never called
  // after SessionRestorationWebStateListObserver is destroyed. Those objects
  // are owned by the current instance, and destroyed before `this`.
  infos_.insert(std::make_pair(
      web_state_list,
      std::make_unique<WebStateListInfo>(
          info->identifier(), web_state_list, info,
          base::BindRepeating(
              &SessionRestorationServiceImpl::MarkWebStateListDirty,
              base::Unretained(this)))));
}

void SessionRestorationServiceImpl::Disconnect(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SaveDirtySessions();
  DCHECK(dirty_web_state_lists_.empty());

  auto iterator = infos_.find(browser->GetWebStateList());
  DCHECK(iterator != infos_.end());

  WebStateListInfo& info = *iterator->second;
  DCHECK(!info.has_backup());

  if (!info.is_backup()) {
    DCHECK(base::Contains(identifiers_, info.identifier()));
    identifiers_.erase(info.identifier());
  }

  infos_.erase(iterator);
}

std::unique_ptr<web::WebState>
SessionRestorationServiceImpl::CreateUnrealizedWebState(
    Browser* browser,
    web::proto::WebStateStorage storage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iterator = infos_.find(browser->GetWebStateList());
  DCHECK(iterator != infos_.end());

  // Create the unique identifier for the new WebState and mark it as
  // expected with the SessionRestorationWebStateListObserver (since it
  // cannot be adopted).
  const web::WebStateID web_state_id = web::WebStateID::NewUnique();

  WebStateListInfo& info = *iterator->second;
  info.observer().AddExpectedWebState(web_state_id);

  // Schedule saving the storage and metadata for the created WebState
  // to disk before creating it, to ensure the data is available after
  // the next application restart even if the WebState never transition
  // to the realised state.
  const base::FilePath web_state_dir = ios::sessions::WebStateDirectory(
      storage_path_.Append(info.identifier()), web_state_id);

  // Add the metadata to `info`'s WebStateMetadataMap. It will be saved when
  // the WebState is inserted in the WebStateList.
  DCHECK(storage.has_metadata());
  web::proto::WebStateMetadataStorage metadata;
  metadata.Swap(storage.mutable_metadata());

  DCHECK(!base::Contains(info.metadata_map(), web_state_id));
  info.metadata_map().insert(std::make_pair(web_state_id, metadata));

  // Create the request to serialize WebState storage and add it to the
  // list of pending requests (they will be scheduled once the WebState
  // is inserted in the Browser's WebStateList).
  pending_requests_.push_back(
      std::make_unique<ios::sessions::WriteProtoIORequest>(
          web_state_dir.Append(kWebStateStorageFilename),
          std::make_unique<web::proto::WebStateStorage>(storage)));

  // Create the WebState with callback that return the data from memory. This
  // ensure there is no race condition while trying to read the data from the
  // main thread while it is being written to disk on a background thread.
  return web::WebState::CreateWithStorage(
      browser->GetProfile(), web_state_id, std::move(metadata),
      base::ReturnValueOnce(std::move(storage)),
      base::ReturnValueOnce<NSData*>(nil));
}

void SessionRestorationServiceImpl::DeleteDataForDiscardedSessions(
    const std::set<std::string>& identifiers,
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!HasIntersection(identifiers, identifiers_));
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DeleteDataForSessions, storage_path_, identifiers),
      std::move(closure));
}

void SessionRestorationServiceImpl::InvokeClosureWhenBackgroundProcessingDone(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTask(FROM_HERE, std::move(closure));
}

void SessionRestorationServiceImpl::PurgeUnassociatedData(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(closure));
}

void SessionRestorationServiceImpl::ParseDataForBrowserAsync(
    Browser* browser,
    WebStateStorageIterationCallback iter_callback,
    WebStateStorageIterationCompleteCallback done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = infos_.find(browser->GetWebStateList());
  DCHECK(iter != infos_.end());

  const WebStateListInfo& info = *iter->second;
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&IterateDataForSessionAtPath,
                     storage_path_.Append(info.identifier()),
                     std::move(iter_callback)),
      std::move(done));
}

#pragma mark - Private

void SessionRestorationServiceImpl::MarkWebStateListDirty(
    WebStateList* web_state_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dirty_web_state_lists_.insert(web_state_list);
  if (!timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE, save_delay_,
        base::BindRepeating(&SessionRestorationServiceImpl::SaveDirtySessions,
                            base::Unretained(this)));
  }
}

void SessionRestorationServiceImpl::UpdateOrphanInfoMap() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (WebStateList* web_state_list : dirty_web_state_lists_) {
    DCHECK(base::Contains(infos_, web_state_list));
    WebStateListInfo& info = *infos_[web_state_list];

    const auto& detached_web_states = info.observer().detached_web_states();
    if (!detached_web_states.empty()) {
      WebStateMetadataMap& metadata_map = info.metadata_map();
      const std::string& identifier = info.identifier();

      for (const auto web_state_id : detached_web_states) {
        // It is possible for this method to be called multiple times before
        // the dirty flag is called on the observer (as it is called by both
        // SaveDirySession() and LoadWebStateStorage() methods).
        if (base::Contains(orphaned_map_, web_state_id)) {
          continue;
        }

        // If a realized WebState is created, inserted into a Browser and
        // then moved to another Browser before its state could be saved,
        // the metadata will not be present in the metadata_map. See the
        // bug https://crbug.com/329219388 for more details.
        auto iter = metadata_map.find(web_state_id);
        if (iter == metadata_map.end()) {
          continue;
        }

        OrphanInfo orphan_info{
            .session_id = identifier,
            .metadata = std::move(iter->second),
        };

        orphaned_map_.insert(
            std::make_pair(web_state_id, std::move(orphan_info)));

        metadata_map.erase(iter);
      }
    }
  }
}

void SessionRestorationServiceImpl::SaveDirtySessions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timer_.IsRunning()) {
    timer_.Stop();
  }

  if (dirty_web_state_lists_.empty()) {
    return;
  }

  const base::TimeTicks start_time = base::TimeTicks::Now();

  // Initialize the list of requests with all pending request. This ensures
  // that any WebState created by CreateUnrealizedWebState(...) will have
  // its state saved.
  ios::sessions::IORequestList requests;
  std::swap(requests, pending_requests_);

  // Updates the map of orphaned WebStates.
  UpdateOrphanInfoMap();

  // Handle adopted WebStates (i.e. "unrealized" WebStates inserted into a
  // WebStateList).
  for (WebStateList* web_state_list : dirty_web_state_lists_) {
    DCHECK(base::Contains(infos_, web_state_list));
    WebStateListInfo& info = *infos_[web_state_list];

    const auto& inserted_web_states = info.observer().inserted_web_states();
    if (!inserted_web_states.empty()) {
      const base::FilePath dest_dir = storage_path_.Append(info.identifier());

      WebStateMetadataMap& metadata_map = info.metadata_map();
      for (const auto web_state_id : inserted_web_states) {
        // The `web_state_id` must be adopted from another Browser, thus needs
        // to be in the `orphaned_map_` (the case of expected WebState is dealt
        // entirely in SessionRestorationWebStateListObserver).
        DCHECK(base::Contains(orphaned_map_, web_state_id));
        auto iter = orphaned_map_.find(web_state_id);

        // The WebState is adopted, remove it from the orphan map.
        OrphanInfo orphan_info = std::move(iter->second);
        orphaned_map_.erase(iter);

        // Only unrealized WebState should be adopted, realized WebState
        // will instead be considered dirty. Thus the metadata should be
        // present in the orphaned_map_.
        DCHECK(!base::Contains(metadata_map, web_state_id));
        metadata_map.insert(
            std::make_pair(web_state_id, std::move(orphan_info.metadata)));

        // No need to copy if this is moving to/from the backup.
        if (orphan_info.session_id == info.identifier()) {
          continue;
        }

        const base::FilePath from_dir =
            storage_path_.Append(orphan_info.session_id);

        // Create a request to copy the orphaned data.
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
    WebStateMetadataMap& metadata_map = info.metadata_map();

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
      const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
      const base::FilePath web_state_dir =
          ios::sessions::WebStateDirectory(dest_dir, web_state_id);

      // Serialize the WebState to protobuf message.
      auto storage = std::make_unique<web::proto::WebStateStorage>();
      web_state->SerializeToProto(*storage);
      DCHECK(storage->has_metadata());

      // Extract the metadata from `storage` to save it in its own file.
      // The metadata must be non-null at this point (since at least the
      // creation time or last active time will be non-default).
      auto metadata = base::WrapUnique(storage->release_metadata());
      DCHECK(metadata);

      // Update the metadata in `info`'s WebStateMetadataMap. It will be
      // saved inside the WebStateListStorage.
      auto iter = metadata_map.find(web_state_id);
      if (iter == metadata_map.end()) {
        metadata_map.insert(std::make_pair(web_state_id, std::move(*metadata)));
      } else {
        iter->second = std::move(*metadata);
      }

      // The WebState is serialized, remove it from the orphan map.
      orphaned_map_.erase(web_state_id);

      // Create a request to serialize the `storage`.
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

    // Delete the storage of any WebState that has been closed (the data is
    // now unreachable, and thus can safely be deleted).
    const auto& closed_web_states = observer.closed_web_states();
    for (web::WebStateID web_state_id : closed_web_states) {
      // It is possible (though unlikely) for a WebState to be closed just
      // after being moved between Browser. Support that case by deleting
      // the data from the Browser that listed the WebState for adoption.
      base::FilePath browser_dir = dest_dir;

      auto iter = orphaned_map_.find(web_state_id);
      if (iter != orphaned_map_.end()) {
        const OrphanInfo& orphan_info = iter->second;
        browser_dir = storage_path_.Append(orphan_info.session_id);

        // The WebState is closed, remove it from the orphan map.
        orphaned_map_.erase(iter);
      } else {
        metadata_map.erase(web_state_id);
      }

      requests.push_back(std::make_unique<ios::sessions::DeletePathIORequest>(
          ios::sessions::WebStateDirectory(browser_dir, web_state_id)));
    }

    // Clear the "dirty" bit.
    observer.ClearDirty();

    // No need to serialize if this is a backup.
    if (info.is_backup()) {
      continue;
    }

    // It has been found in production that the metadata map may be missing
    // data for some items (see https://crbug.com/332533665 for such crash).
    // As a workaround, update the metadata map from the WebStateList.
    UpdateMetadataMap(metadata_map, web_state_list);

    // Always serialize the WebStateList as it includes the WebStates'
    // metadata (and thus needs to be saved either the list or one of
    // the WebState is dirty).
    auto storage = std::make_unique<ios::proto::WebStateListStorage>();
    SerializeWebStateList(*web_state_list, metadata_map, *storage);

    requests.push_back(std::make_unique<ios::sessions::WriteProtoIORequest>(
        dest_dir.Append(kSessionMetadataFilename), std::move(storage)));
  }

  // Post the IORequests on the background sequence as writing to disk
  // can block.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ios::sessions::ExecuteIORequests, std::move(requests)));

  dirty_web_state_lists_.clear();

  // Record the time spent blocking the main thread to save the session.
  base::UmaHistogramTimes(kSessionHistogramSavingTime,
                          base::TimeTicks::Now() - start_time);
}
