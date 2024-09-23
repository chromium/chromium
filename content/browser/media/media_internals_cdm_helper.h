// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_CDM_HELPER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_CDM_HELPER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/media/cdm_registry_impl.h"
#include "content/public/common/cdm_info.h"

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
  void OnKeySystemCapabilitiesUpdated(KeySystemCapabilities capabilities);

  // Callback subscription to keep the callback alive in the CdmRegistry.
  base::CallbackListSubscription cb_subscription_;

  base::WeakPtrFactory<MediaInternalsCdmHelper> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_CDM_HELPER_H_
