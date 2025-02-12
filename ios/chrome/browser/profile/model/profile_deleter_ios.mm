// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_deleter_ios.h"

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/ios_util.h"
#import "base/memory/scoped_refptr.h"
#import "base/sequence_checker.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/uuid.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state_utils.h"

namespace ProfileDeleterIOS {
namespace internal {

// Callback invoked when the deletion completes, successfully or not.
using DeletionCompleteCallback = base::OnceCallback<void(bool)>;

// Deletes the Profile storage for a given profile. This is the last step
// when deleting a profile as once this is deleted, there is no way to
// get any information back from the profile.
void DeleteProfileStorage(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::FilePath storage_path,
    DeletionCompleteCallback callback) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&base::DeletePathRecursively, std::move(storage_path)),
      std::move(callback));
}

// Invoked when the WebKit storage has been deleted, or with a non-nil
// error if the deletion failed. If the operation was a success, move
// to the next stage.
void WebkitStorageDeleted(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::FilePath storage_path,
    DeletionCompleteCallback callback,
    NSError* error) {
  if (error != nil) {
    // If the deletion of the WebKit storage failed, abort the deletion.
    // The ProfileManagerIOS should retry on the next application run.
    std::move(callback).Run(false);
    return;
  }

  DeleteProfileStorage(task_runner, std::move(storage_path),
                       std::move(callback));
}

// Deletes the WebKit storage for a given profile. This is the first step
// when deleting a profile (as the identifier is stored in the profile's
// PrefService for old profiles, and thus can't be accessed after the
// profile's data on disk has been deleted).
void DeleteWebkitStorage(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::FilePath storage_path,
    base::Uuid webkit_storage_id,
    DeletionCompleteCallback callback) {
  if (!base::ios::IsRunningOnIOS17OrLater() || !webkit_storage_id.is_valid()) {
    DeleteProfileStorage(task_runner, std::move(storage_path),
                         std::move(callback));
    return;
  }

  web::RemoveDataStorageForIdentifier(
      webkit_storage_id,
      base::BindOnce(&WebkitStorageDeleted, task_runner,
                     std::move(storage_path), std::move(callback)));
}

// Initiates the deletion of a profile. This must be called after deleting
// the ProfileIOS object.
void DeleteProfile(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                   base::FilePath storage_path,
                   base::Uuid webkit_storage_id,
                   DeletionCompleteCallback callback) {
  // Start the deletion process (once the tasks on the IO task runner have
  // completed, approximate this by posting a no-op task to IO task runner
  // and calling the first step of the deletion when it completes).
  task_runner->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&internal::DeleteWebkitStorage, task_runner,
                     std::move(storage_path), std::move(webkit_storage_id),
                     std::move(callback)));
}

}  // namespace internal

void DeleteProfile(std::unique_ptr<ProfileIOS> profile,
                   ProfileDeletedCallback callback) {
  DCHECK(!profile->IsOffTheRecord());

  // Extract the information from the profile before deleting the object.
  std::string profile_name = profile->GetProfileName();
  base::FilePath storage_path = profile->GetStatePath();
  base::Uuid webkit_storage_id = profile->GetWebKitStorageID();
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      profile->GetIOTaskRunner();

  // Delete the ProfileIOS object. This should stop any KeyedService still
  // running. They may try to post task on the Profile's IO task runner so
  // post a task with reply to "wait" for any pending tasks and then start
  // the profile deletion by removing the WebKitStorage.
  profile.reset();

  internal::DeleteProfile(task_runner, std::move(storage_path),
                          std::move(webkit_storage_id),
                          base::BindOnce(std::move(callback), profile_name));
}

}  // namespace ProfileDeleterIOS
