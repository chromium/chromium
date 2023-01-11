// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPABILITIES_WEBRTC_VIDEO_STATS_DB_PROVIDER_H_
#define MEDIA_CAPABILITIES_WEBRTC_VIDEO_STATS_DB_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "media/base/media_export.h"

namespace media {

class WebrtcVideoStatsDB;

// Interface for extracting a pointer to the DB from its owner. DB lifetime is
// assumed to match that of the provider. Callers must not use DB after provider
// has been destroyed. This allows sharing a "seed" DB instance between an
// Incognito profile and the original profile, which re-uses the in-memory
// cache for that DB and avoids race conditions of instantiating a second DB
// that reads the same files.
class MEDIA_EXPORT WebrtcVideoStatsDBProvider {
 public:
  // Request a pointer to the *initialized* DB owned by this provider. Call
  // lazily to avoid triggering unnecessary DB initialization. `db` is null in
  // the event of an error. Callback may be run immediately if `db` is already
  // initialized by provider.
  using GetCB = base::OnceCallback<void(WebrtcVideoStatsDB* db)>;
  virtual void GetWebrtcVideoStatsDB(GetCB get_db_b) = 0;

 protected:
  virtual ~WebrtcVideoStatsDBProvider();
};

}  // namespace media

#endif  // MEDIA_CAPABILITIES_WEBRTC_VIDEO_STATS_DB_PROVIDER_H_