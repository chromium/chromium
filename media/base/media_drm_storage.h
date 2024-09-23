// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_DRM_STORAGE_H_
#define MEDIA_BASE_MEDIA_DRM_STORAGE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "media/base/media_drm_key_type.h"
#include "media/base/media_export.h"
#include "url/origin.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace media {

// Allows MediaDrmBridge to store and retrieve persistent data. This is needed
// for features like per-origin provisioning and persistent license support.
class MEDIA_EXPORT MediaDrmStorage {
 public:
  // When using per-origin provisioning, this is the ID for the origin.
  // If not specified, the device specific origin ID is to be used.
  using MediaDrmOriginId = std::optional<base::UnguessableToken>;

  struct MEDIA_EXPORT SessionData {
    SessionData(std::vector<uint8_t> key_set_id,
                std::string mime_type,
                MediaDrmKeyType key_type);
    SessionData(const SessionData& other);
    ~SessionData();

    std::vector<uint8_t> key_set_id;
    std::string mime_type;
    MediaDrmKeyType key_type;
  };

  MediaDrmStorage();

  MediaDrmStorage(const MediaDrmStorage&) = delete;
  MediaDrmStorage& operator=(const MediaDrmStorage&) = delete;

  virtual ~MediaDrmStorage();

  // Callback to return whether the operation succeeded.
  using ResultCB = base::OnceCallback<void(bool)>;

  // Callback for storage initialization.
  using InitCB =
      base::OnceCallback<void(bool success, const MediaDrmOriginId& origin_id)>;

  // Callback to return the result of LoadPersistentSession. |key_set_id| and
  // |mime_type| must be non-empty if |success| is true, and vice versa.
  using LoadPersistentSessionCB =
      base::OnceCallback<void(std::unique_ptr<SessionData> session_data)>;

  // Initialize the storage for current origin. The implementation already know
  // the origin for the storage.
  // Implementation should return a random origin id in |init_cb|. The ID should
  // be unique and persisted. Origin ID must be valid. If any corruption is
  // detected, the old map should be removed in OnProvisioned.
  virtual void Initialize(InitCB init_cb) = 0;

  // Called when MediaDrm is provisioned for the origin bound to |this|.
  // The implementation should keep track of the storing time so that the
  // information can be cleared based on selected time range (e.g. for clearing
  // browsing data).
  virtual void OnProvisioned(ResultCB result_cb) = 0;

  // Saves the persistent session info for |session_id| in the storage.
  // The implementation should keep track of the storing time so that the
  // information can be cleared based on selected time range (e.g. for clearing
  // browsing data).
  virtual void SavePersistentSession(const std::string& session_id,
                                     const SessionData& session_data,
                                     ResultCB result_cb) = 0;

  // Loads the persistent session info for |session_id| from the storage.
  virtual void LoadPersistentSession(
      const std::string& session_id,
      LoadPersistentSessionCB load_persistent_session_cb) = 0;

  // Removes the persistent session info for |session_id| from the storage.
  // If the session for |session_id| exists in the storage, it is removed.
  // Otherwise, this call is a no-op. In both cases, the result will be true.
  // The result will be false on other unexpected errors, e.g. connection error
  // to the storage backend.
  virtual void RemovePersistentSession(const std::string& session_id,
                                       ResultCB result_cb) = 0;

  // Return a WeakPtr instance. This must be implemented by the deepest
  // class in the hierarchy. This is used for JNI calls in
  // `MediaDrmStorageBridge`.
  virtual base::WeakPtr<MediaDrmStorage> AsWeakPtr() = 0;
};

using CreateStorageCB =
    base::RepeatingCallback<std::unique_ptr<MediaDrmStorage>()>;

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_DRM_STORAGE_H_
