// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_DRM_STORAGE_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_DRM_STORAGE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_drm_storage.h"
#include "media/mojo/mojom/media_drm_storage.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// A MediaDrmStorage that proxies to a Remote<mojom::MediaDrmStorage>.
class MEDIA_MOJO_EXPORT MojoMediaDrmStorage final : public MediaDrmStorage {
 public:
  explicit MojoMediaDrmStorage(
      mojo::PendingRemote<mojom::MediaDrmStorage> media_drm_storage);

  MojoMediaDrmStorage(const MojoMediaDrmStorage&) = delete;
  MojoMediaDrmStorage& operator=(const MojoMediaDrmStorage&) = delete;

  ~MojoMediaDrmStorage() override;

  // MediaDrmStorage implementation:
  void Initialize(InitCB init_cb) override;
  void OnProvisioned(ResultCB result_cb) override;
  void SavePersistentSession(const std::string& session_id,
                             const SessionData& session_data,
                             ResultCB result_cb) override;
  void LoadPersistentSession(
      const std::string& session_id,
      LoadPersistentSessionCB load_persistent_session_cb) override;
  void RemovePersistentSession(const std::string& session_id,
                               ResultCB result_cb) override;
  base::WeakPtr<MediaDrmStorage> AsWeakPtr() override;

 private:
  void OnPersistentSessionLoaded(
      LoadPersistentSessionCB load_persistent_session_cb,
      mojom::SessionDataPtr session_data);

  mojo::Remote<mojom::MediaDrmStorage> media_drm_storage_;
  base::WeakPtrFactory<MojoMediaDrmStorage> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_DRM_STORAGE_H_
