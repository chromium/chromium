// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_PREF_SERVICE_H_
#define MEDIA_CDM_CDM_PREF_SERVICE_H_

#include "base/callback.h"
#include "base/unguessable_token.h"
#include "media/base/media_export.h"
#include "url/origin.h"

namespace media {

// Manages reads and writes to the user prefs service related to CDM usage.
// The service itself is a per-frame interface that lives in the browser process
// and will be called by a client living in the utility process hosting the CDM.
class MEDIA_EXPORT CdmPrefService {
 public:
  using GetCdmOriginIdCB =
      base::OnceCallback<void(const base::UnguessableToken&)>;

  CdmPrefService() = default;
  virtual ~CdmPrefService() = default;

  CdmPrefService(const CdmPrefService&) = delete;
  CdmPrefService& operator=(const CdmPrefService&) = delete;

  // Gets the origin ID associated with the origin of the CDM. The origin ID is
  // used in place of the origin when hiding the concrete origin is needed. The
  // origin ID is also user resettable by clearing the browsing data.
  virtual void GetCdmOriginId(GetCdmOriginIdCB callback) = 0;
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_PREF_SERVICE_H_
