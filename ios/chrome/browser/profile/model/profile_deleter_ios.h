// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_DELETER_IOS_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_DELETER_IOS_H_

#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

// Helper class to delete profile's storage from disk.
class ProfileDeleterIOS {
 public:
  // Result of the deletion operation.
  enum class Result {
    kSuccess,
    kFailure,
  };

  // Callback invoked with the result of the deletion.
  using DeletionResultCallback = base::OnceCallback<void(Result)>;

  ProfileDeleterIOS();

  ProfileDeleterIOS(const ProfileDeleterIOS&) = delete;
  ProfileDeleterIOS& operator=(const ProfileDeleterIOS&) = delete;

  ~ProfileDeleterIOS();

  // Deletes a profile given its name, and the directory where the profiles
  // are stored. Invokes `callback` asynchronously on the current sequence
  // with the result of the operation on completion.
  void DeleteProfile(std::string_view profile_name,
                     const base::FilePath& storage_dir,
                     DeletionResultCallback callback);

 private:
  // The task runner used to execute background operations.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_DELETER_IOS_H_
