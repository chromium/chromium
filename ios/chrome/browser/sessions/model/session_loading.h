// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_LOADING_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_LOADING_H_

#include "base/files/file_path.h"
#include "ios/chrome/browser/sessions/model/proto/storage.pb.h"
#include "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#include "ios/web/public/session/proto/storage.pb.h"
#include "ios/web/public/web_state_id.h"

namespace ios::sessions {

// Returns the path of the sub-directory of `directory` containing the
// files representing of the storage of WebState with `identifier`.
base::FilePath WebStateDirectory(const base::FilePath& directory,
                                 web::WebStateID identifier);

// Filters items identified by `removing_indexes` from `storage`.
ios::proto::WebStateListStorage FilterItems(
    ios::proto::WebStateListStorage storage,
    const RemovingIndexes& removing_indexes);

// Loads the session metadata storage from `directory`. Returns an empty
// session in case of failure (e.g. due to a missing or corrupted session).
ios::proto::WebStateListStorage LoadSessionStorage(
    const base::FilePath& directory);

}  // namespace ios::sessions

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_LOADING_H_
