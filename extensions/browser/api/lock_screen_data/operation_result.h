// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_OPERATION_RESULT_H_
#define EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_OPERATION_RESULT_H_

namespace extensions {
namespace lock_screen_data {

// Enum containing possible results of data item operations exposed by
// ItemStorage and DataItem.
// IMPORTANT: Used to report metrics. Should be kept in sync with
// LockScreenDataItemOperationResult histogram enum. The assigned values should
// not be changed.
enum class OperationResult {
  kSuccess = 0,
  kFailed = 1,
  kNotFound = 2,
  kUnknownExtension = 3,
  kAlreadyRegistered = 4,
  kInvalidKey = 5,
  kWrongKey = 6,
  kCount,
};

}  // namespace lock_screen_data
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_OPERATION_RESULT_H_
