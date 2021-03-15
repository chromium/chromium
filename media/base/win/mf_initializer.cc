// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_initializer.h"

#include <mfapi.h>

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace {

// MFShutdown() is sometimes very expensive if it's the last instance and
// shouldn't result in excessive memory usage to leave around, so only start it
// once and only shut it down at process exit. See https://crbug.com/1069603#c90
// for details.
//
// Note: Most Chrome process exits will not invoke the AtExit handler, so
// MFShutdown() will generally not be called. However, we use the default
// singleton traits (which register an AtExit handler) for tests and remoting.
class MediaFoundationSession {
 public:
  static MediaFoundationSession* GetInstance() {
    return base::Singleton<MediaFoundationSession>::get();
  }

  ~MediaFoundationSession() {
    // The public documentation stating that it needs to have a corresponding
    // shutdown for all startups (even failed ones) is wrong.
    if (has_media_foundation_)
      MFShutdown();
  }

  bool has_media_foundation() const { return has_media_foundation_; }

 private:
  friend struct base::DefaultSingletonTraits<MediaFoundationSession>;

  MediaFoundationSession() {
    const auto hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    has_media_foundation_ = hr == S_OK;

    LOG_IF(ERROR, !has_media_foundation_)
        << "Failed to start Media Foundation, accelerated media functionality "
           "may be disabled. If you're using Windows N, see "
           "https://support.microsoft.com/en-us/topic/"
           "media-feature-pack-for-windows-10-n-may-2020-ebbdf559-b84c-0fc2-"
           "bd51-e23c9f6a4439 for information on how to install the Media "
           "Feature Pack. Error: "
        << logging::SystemErrorCodeToString(hr);
  }

  bool has_media_foundation_ = false;
};

}  // namespace

namespace media {

bool InitializeMediaFoundation() {
  return MediaFoundationSession::GetInstance()->has_media_foundation();
}

}  // namespace media
