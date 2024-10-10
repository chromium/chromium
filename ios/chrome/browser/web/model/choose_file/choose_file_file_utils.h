// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_FILE_UTILS_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_FILE_UTILS_H_

#import <optional>

#import "ios/web/public/web_state_id.h"

namespace base {
class FilePath;
}

// A utility file to handle the directory in which files chosen by the user will
// be temporary downloaded.
// The directory structure is
// TEMPORARY_DIRECTORY/choose_file/<session UUID>/<WebStateId>.
// Here, session must be understood as "Process lifetime" as the only purpose
// is to be able to delete the directory later, should Chrome Crash.

// Return the directory in which the WebState can temporary download files
// to be uploaded.
std::optional<base::FilePath> GetTabChooseFileDirectory(
    web::WebStateID web_state_id);

// Schedule the deletion of the temporary directory for the tab.
void DeleteTempChooseFileDirectoryForTab(web::WebStateID web_state_id);

// Delete all stalled Session directory (a.k.a empties
// TEMPORARY_DIRECTORY/choose_file except for the current session directory.
void DeleteTempChooseFileDirectory();

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_FILE_UTILS_H_
