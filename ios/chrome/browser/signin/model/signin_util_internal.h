// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_UTIL_INTERNAL_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_UTIL_INTERNAL_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"

namespace signin {
enum class Tribool;

// Struct returned by `LoadDeviceRestoreDataInternal()`.
struct RestoreData {
  // The value is `kTrue` if the current session is right after a device
  // restore, otherwise `kFalse`.
  // `kUnknown` if it was not possible know.
  signin::Tribool is_first_session_after_device_restore;
  // Timestamp of the latest device restore. The value is unset:
  //   - if the device was not been restored,
  //   - or if this is the first run after a device restore,
  //   - or if `LoadDeviceRestoreData()` was not called yet.
  std::optional<base::Time> last_restore_timestamp;
};

}  // namespace signin

// File name for sentinel to backup in iOS backup device.
extern const base::FilePath::CharType kSentinelThatIsBackedUp[];
// File name for sentinel to not backup in iOS backup device.
extern const base::FilePath::CharType kSentinelThatIsNotBackedUp[];

base::FilePath PathForSentinel(const base::FilePath::CharType* sentinel_name);

// Returns whether Chrome has been started after a device restore. This method
// needs to be called for the first time before IO is disallowed on UI thread.
// The value is cached. The result is cached for later calls.
// `completion` is called once all sentinel files are created.
signin::RestoreData LoadDeviceRestoreDataInternal(
    base::OnceClosure completion = base::DoNothing());

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_UTIL_INTERNAL_H_
