// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_LOADING_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_LOADING_H_

#include <map>

#include "base/files/file_path.h"
#include "ios/chrome/browser/sessions/proto/storage.pb.h"
#include "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#include "ios/web/public/session/proto/storage.pb.h"
#include "ios/web/public/web_state_id.h"

namespace ios::sessions {

// Represents the metadata required to restore a session.
struct SessionStorage {
  // Maps the metadata for a WebState to its identifier.
  using WebStateMetadataStorageMap =
      std::map<web::WebStateID, web::proto::WebStateMetadataStorage>;

  // Constructs a default SessionStorage representing an empty session.
  SessionStorage();

  // Constructs a SessionStorage from data loaded from disk.
  SessionStorage(ios::proto::WebStateListStorage session_metadata,
                 WebStateMetadataStorageMap web_state_storage_map);

  SessionStorage(SessionStorage&&);
  SessionStorage& operator=(SessionStorage&&);

  ~SessionStorage();

  ios::proto::WebStateListStorage session_metadata;
  WebStateMetadataStorageMap web_state_storage_map;
};

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
SessionStorage LoadSessionStorage(const base::FilePath& directory);

}  // namespace ios::sessions

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_LOADING_H_
