// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_deleter_ios.h"

#import <optional>
#import <string>

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/ios_util.h"
#import "base/json/json_file_value_serializer.h"
#import "base/memory/scoped_refptr.h"
#import "base/sequence_checker.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/uuid.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/web/public/browser_state_utils.h"

namespace {

using DeletionResult = ProfileDeleterIOS::Result;
using DeletionResultCallback = ProfileDeleterIOS::DeletionResultCallback;
using TaskRunnerPtr = scoped_refptr<base::SequencedTaskRunner>;

// Converts a boolean to a ProfileDeleterIOS::Result.
DeletionResult ResultFromBool(bool success) {
  return success ? DeletionResult::kSuccess : DeletionResult::kFailure;
}

// Deletes the Profile storage for a given profile. This is the last step
// when deleting a profile as once this is deleted, there is no way to
// get any information back from the profile.
void DeleteProfileStorage(const TaskRunnerPtr& task_runner,
                          const base::FilePath& profile_dir,
                          DeletionResultCallback callback) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::DeletePathRecursively, profile_dir),
      base::BindOnce(&ResultFromBool).Then(std::move(callback)));
}

// Invoked when the WebKit storage has been deleted, or with a non-nil
// error if the deletion failed. If the operation was a success, move
// to the next stage.
void WebkitStorageDeleted(const TaskRunnerPtr& task_runner,
                          const base::FilePath& profile_dir,
                          DeletionResultCallback callback,
                          NSError* error) {
  if (error != nil) {
    // If the deletion of the WebKit storage failed, abort the deletion.
    // The ProfileManagerIOS should retry on the next application run.
    std::move(callback).Run(DeletionResult::kFailure);
    return;
  }

  DeleteProfileStorage(task_runner, profile_dir, std::move(callback));
}

// Deletes the WebKit storage for a given profile. This is the first step
// when deleting a profile (as the identifier is stored in the profile's
// PrefService for old profiles, and thus can't be accessed after the
// profile's data on disk has been deleted).
void DeleteWebkitStorage(const TaskRunnerPtr& task_runner,
                         const base::FilePath& profile_dir,
                         DeletionResultCallback callback,
                         const base::Uuid& webkit_storage_id) {
  if (!base::ios::IsRunningOnIOS17OrLater() || !webkit_storage_id.is_valid()) {
    DeleteProfileStorage(task_runner, profile_dir, std::move(callback));
    return;
  }

  web::RemoveDataStorageForIdentifier(
      webkit_storage_id, base::BindOnce(&WebkitStorageDeleted, task_runner,
                                        profile_dir, std::move(callback)));
}

// Determines the WebKit storage identifier by loading the preferences,
// returning `default_uuid` if they cannot be loaded, or the value is
// not present.
base::Uuid DetermineWebkitStorageId(const base::FilePath& profile_dir,
                                    const base::Uuid& default_uuid) {
  std::unique_ptr<base::Value> value =
      JSONFileValueDeserializer(profile_dir.Append("Preferences"))
          .Deserialize(/*error_code=*/nullptr, /*error_message=*/nullptr);
  if (!value || !value->is_dict()) {
    return default_uuid;
  }

  const std::string* string =
      value->GetDict().FindString(prefs::kBrowserStateStorageIdentifier);
  if (!string) {
    return default_uuid;
  }

  const base::Uuid uuid = base::Uuid::ParseLowercase(*string);
  if (!uuid.is_valid()) {
    return default_uuid;
  }

  return uuid;
}

// Invoked after checking whether the profile storage exists on disk.
void DeleteProfileIfExists(const TaskRunnerPtr& task_runner,
                           const base::FilePath& profile_dir,
                           const base::Uuid& default_uuid,
                           DeletionResultCallback callback,
                           bool profile_storage_exists) {
  if (!profile_storage_exists) {
    // If the profile storage does not exists on disk, there is nothing to
    // do (as the WebKit storage is created after the profile directory).
    std::move(callback).Run(DeletionResult::kSuccess);
    return;
  }

  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DetermineWebkitStorageId, profile_dir, default_uuid),
      base::BindOnce(&DeleteWebkitStorage, task_runner, profile_dir,
                     std::move(callback)));
}

}  // namespace

ProfileDeleterIOS::ProfileDeleterIOS() {
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({
      base::MayBlock(),
      base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
  });
}

ProfileDeleterIOS::~ProfileDeleterIOS() = default;

void ProfileDeleterIOS::DeleteProfile(std::string_view profile_name,
                                      const base::FilePath& storage_dir,
                                      DeletionResultCallback callback) {
  base::FilePath profile_dir = storage_dir.Append(profile_name);
  base::Uuid default_uuid = base::Uuid::ParseCaseInsensitive(profile_name);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::DirectoryExists, profile_dir),
      base::BindOnce(&DeleteProfileIfExists, task_runner_, profile_dir,
                     default_uuid, std::move(callback)));
}
