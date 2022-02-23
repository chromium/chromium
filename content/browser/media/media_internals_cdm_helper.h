// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_CDM_HELPER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_CDM_HELPER_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "media/mojo/mojom/key_system_support.mojom-forward.h"

namespace content {

// Manages the "CDMs" tab on about://media-internals.
class MediaInternalsCdmHelper {
 public:
  MediaInternalsCdmHelper();
  MediaInternalsCdmHelper(const MediaInternalsCdmHelper&) = delete;
  MediaInternalsCdmHelper& operator=(const MediaInternalsCdmHelper&) = delete;
  ~MediaInternalsCdmHelper();

  // Get information on all registered CDMs and update the "CDMs" tab on
  // about://media-internals.
  void GetRegisteredCdms();

 private:
  void OnCdmInfoFinalized(const std::string& key_system,
                          bool success,
                          media::mojom::KeySystemCapabilityPtr capability);
  void SendCdmUpdate();

  std::set<std::string> pending_key_systems_;

  base::WeakPtrFactory<MediaInternalsCdmHelper> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_CDM_HELPER_H_
