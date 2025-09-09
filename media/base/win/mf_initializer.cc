// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_initializer.h"

#include <windows.h>

#include <mfapi.h>
#include <synchapi.h>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/delayload_helpers.h"
#include "base/win/scoped_handle.h"
#include "media/base/win/media_foundation_package_runtime_locator.h"

namespace {

static const char kMediaFoundationLoadFailedMessage[] =
    "Failed to start Media Foundation, accelerated media functionality "
    "may be disabled. If you're using Windows N, see "
    "https://support.microsoft.com/en-us/topic/"
    "media-feature-pack-for-windows-10-n-may-2020-ebbdf559-b84c-0fc2-"
    "bd51-e23c9f6a4439 for information on how to install the Media "
    "Feature Pack. Error: ";

// Attempts to load the required Media Foundation libraries once. Returns the
// status of that attempt on subsequent calls. Must be called once prior to
// sandbox initialization or it will always fail.
bool LoadMediaFoundationLibraries() {
  static const bool kDidLoadSucceed = []() {
    for (const wchar_t* mfdll : {L"mf.dll", L"mfplat.dll"}) {
      base::NativeLibraryLoadError error;
      if (!base::LoadSystemLibrary(mfdll, &error)) {
        ::SetLastError(error.code);
        PLOG(ERROR) << kMediaFoundationLoadFailedMessage << "Could not load "
                    << mfdll;
        return false;
      }
    }

#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    if (media::LoadMediaFoundationPackageDecoder(media::AudioCodec::kEAC3)) {
      DVLOG(2)
          << __func__
          << ": EAC3(AC3) decoder loaded from MediaFoundation codec package";
    }
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
    if (media::LoadMediaFoundationPackageDecoder(media::AudioCodec::kAC4)) {
      DVLOG(2) << __func__
               << ": AC4 decoder loaded from MediaFoundation codec package";
    }
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)

    return true;
  }();
  return kDidLoadSucceed;
}

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
    DCHECK(LoadMediaFoundationLibraries());
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
    HRESULT hr = E_FAIL;
    if (ResolveMediaFoundationDelayloads()) {
      hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
      has_media_foundation_ = hr == S_OK;
    }

    LOG_IF(ERROR, !has_media_foundation_)
        << kMediaFoundationLoadFailedMessage
        << logging::SystemErrorCodeToString(hr);
  }

  bool ResolveMediaFoundationDelayloads() {
    // Mitigate the issues caused by loading DLLs on a background thread; see
    // https://crbug.com/41464781. The DLLs should already be loaded on account
    // of `LoadMediaFoundationLibraries()`, but it is possible that resolving
    // the imports could lead to hard faults to page in the DLLs.
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

    // LoadAllImportsForDll() makes a case-sensitive comparison to the module
    // names in the dll's delayloaded dependent module list.
    // LINT.IfChange
    static constexpr base::cstring_view kDlls[] = {"MF.dll", "MFPlat.DLL"};
    // LINT.ThenChange(//chrome/common/win/delay_load_failure_hook.cc)
    for (const auto& mfdll : kDlls) {
      // In tests the DLLs might not be direct deps of this module, so only
      // report errors encountered when the DLLs are found but not loaded.
      auto loaded = base::win::LoadAllImportsForDll(mfdll);
      if (!loaded.has_value()) {
        // Set error for PLOG.
        ::SetLastError(HRESULT_CODE(loaded.error()));
        PLOG(ERROR) << kMediaFoundationLoadFailedMessage
                    << "Could not resolve imports for " << mfdll;
        return false;
      }
    }
    return true;
  }

  bool has_media_foundation_ = false;
};

}  // namespace

namespace media {

bool InitializeMediaFoundation() {
  return LoadMediaFoundationLibraries() &&
         MediaFoundationSession::GetInstance()->has_media_foundation();
}

bool PreSandboxMediaFoundationInitialization() {
  // Create the global D3D mutex prior to sandbox startup, thereby ensuring
  // Intel hardware encoding MFTs will successfully reuse the existing mutex
  // instead of getting denied by the system, which leads to a failure to
  // activate encoders; see https://crbug.com/1491893.
  static const base::NoDestructor<base::win::ScopedHandle> mutex_handle(
      ::CreateMutex(/*lpMutexAttributes=*/nullptr, /*bInitialOwner=*/false,
                    L"mfx_d3d_mutex"));

  return LoadMediaFoundationLibraries();
}

}  // namespace media
