// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_OPERATION_RESULT_H_
#define EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_OPERATION_RESULT_H_

namespace extensions {
namespace lock_screen_data {

// Enum containing possible results of data item operations exposed by
// ItemStorage and DataItem.
enum class OperationResult {
  kSuccess,
  kFailed,
  kNotFound,
  kUnknownExtension,
  kAlreadyRegistered,
  kInvalidKey,
  kWrongKey,
  kCount,
};

}  // namespace lock_screen_data
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_OPERATION_RESULT_H_
