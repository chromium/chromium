// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PATH_UTIL_H_
#define EXTENSIONS_BROWSER_PATH_UTIL_H_

#include <cstdint>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace extensions {
namespace path_util {

// Prettifies |source_path|, by replacing the user's home directory with "~"
// (if applicable).
// For OS X, prettifies |source_path| by localizing every component of the
// path. Additionally, if the path is inside the user's home directory, then
// replace the home directory component with "~".
base::FilePath PrettifyPath(const base::FilePath& source_path);

// Calculates the size of the directory containing an extension, returning the
// size value without formatting.
void CalculateExtensionDirectorySize(
    const base::FilePath& extension_path,
    base::OnceCallback<void(const int64_t)> callback);

// Calculates the size of the directory containing an extension, and formats it
// to a localized string that can be placed directly in the UI. |message_id| is
// the ID of the string to use when the size is less than 1 MB, basically
// IDS_APPLICATION_INFO_SIZE_SMALL_LABEL.
void CalculateAndFormatExtensionDirectorySize(
    const base::FilePath& extension_path,
    int message_id,
    base::OnceCallback<void(const std::u16string&)> callback);

// Returns a new FilePath with the '~' resolved to the home directory, if
// appropriate. Otherwise, returns the original path.
base::FilePath ResolveHomeDirectory(const base::FilePath& path);

}  // namespace path_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PATH_UTIL_H_
