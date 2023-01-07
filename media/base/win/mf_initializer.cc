// Copyright 2015 The Chromium Authors
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
// MFShutdown() will generally not be called. However, we use singleton traits
// that register an AtExit handler for tests and remoting.
class MediaFoundationSession {
 public:
  static MediaFoundationSession* GetInstance() {
    // StaticMemorySingletonTraits are preferred over DefaultSingletonTraits to
    // allow access from CONTINUE_ON_SHUTDOWN tasks. This means we don't mind a
    // task reading the value of `has_media_foundation_` even after the AtExit
    // hook has run the destructor. StaticMemorySingletonTraits actually make
    // this safe by allocating the singleton with placement new into a static
    // buffer: The destructor doesn't free the memory occupied by the object
    // and it also leaves the object state intact.
    return base::Singleton<
        MediaFoundationSession,
        base::StaticMemorySingletonTraits<MediaFoundationSession>>::get();
  }

  ~MediaFoundationSession() {
    // The public documentation stating that it needs to have a corresponding
    // shutdown for all startups (even failed ones) is wrong.
    if (has_media_foundation_)
      MFShutdown();
  }

  bool has_media_foundation() const { return has_media_foundation_; }

 private:
  friend struct base::StaticMemorySingletonTraits<MediaFoundationSession>;

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
