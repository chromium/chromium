// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_ANDROID_OVERLAY_H_
#define MEDIA_BASE_ANDROID_ANDROID_OVERLAY_H_

#include <list>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/android_overlay_config.h"
#include "media/base/media_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace media {

// Client interface to an AndroidOverlay.  Once constructed, you can expect to
// receive either a call to ReadyCB or a call to FailedCB to indicate whether
// the overlay is ready, or isn't going to be ready.  If one does get ReadyCB,
// then one may GetJavaSurface() to retrieve the java Surface object.  One
// will get DestroyedCB eventually after ReadyCB, assuming that one doesn't
// delete the overlay before that.
// When DestroyedCB arrives, you should stop using the Android Surface and
// delete the AndroidOverlay instance.  Currently, the exact contract depends
// on the concrete implementation.  Once ContentVideoView is deprecated, it will
// be: it is not guaranteed that any AndroidOverlay instances will operate until
// the destroyed instance is deleted.  This must happen on the thread that was
// used to create it.  It does not have to happen immediately, or before the
// callback returns.
// With CVV, one must still delete the overlay on the main thread, and it
// doesn't have to happen before this returns.  However, one must signal the
// CVV onSurfaceDestroyed handler on some thread before returning from the
// callback.  AVDACodecAllocator::ReleaseMediaCodec handles signaling.  The
// fundamental difference is that CVV blocks the UI thread in the browser, which
// makes it unsafe to let the gpu main thread proceed without risk of deadlock
// AndroidOverlay isn't technically supposed to do that.
class MEDIA_EXPORT AndroidOverlay {
 public:
  virtual ~AndroidOverlay();

  // Schedules a relayout of this overlay.  If called before the client is
  // notified that the surface is created, then the call will be ignored.
  virtual void ScheduleLayout(const gfx::Rect& rect) = 0;

  // May be called during / after ReadyCB and before DestroyedCB.
  virtual const base::android::JavaRef<jobject>& GetJavaSurface() const = 0;

  // Add a destruction callback that will be called if the surface is destroyed.
  // Note that this refers to the destruction of the Android Surface, caused by
  // Android.  It is not reporting the destruction of |this|.
  //
  // Destroying |this| prevents any further destroyed callbacks.  This includes
  // cases in which an earlier callback out of multiple registred ones deletes
  // |this|.  None of the later callbacks will be called.
  //
  // These will be called in the same order that they're added.
  virtual void AddSurfaceDestroyedCallback(
      AndroidOverlayConfig::DestroyedCB cb);

  // Add a callback to notify when |this| has been deleted.
  void AddOverlayDeletedCallback(AndroidOverlayConfig::DeletedCB cb);

 protected:
  AndroidOverlay();

  void RunSurfaceDestroyedCallbacks();

  std::list<AndroidOverlayConfig::DestroyedCB> destruction_cbs_;
  std::list<AndroidOverlayConfig::DeletedCB> deletion_cbs_;

  base::WeakPtrFactory<AndroidOverlay> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AndroidOverlay);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_ANDROID_OVERLAY_H_
