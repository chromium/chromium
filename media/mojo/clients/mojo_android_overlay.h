// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_ANDROID_OVERLAY_H_
#define MEDIA_MOJO_CLIENTS_MOJO_ANDROID_OVERLAY_H_

#include "base/unguessable_token.h"
#include "media/base/android/android_overlay.h"
#include "media/mojo/mojom/android_overlay.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// AndroidOverlay implementation via mojo.
class MojoAndroidOverlay : public AndroidOverlay,
                           public mojom::AndroidOverlayClient {
 public:
  MojoAndroidOverlay(
      mojo::PendingRemote<mojom::AndroidOverlayProvider> pending_provider,
      AndroidOverlayConfig config,
      const base::UnguessableToken& routing_token);

  MojoAndroidOverlay(const MojoAndroidOverlay&) = delete;
  MojoAndroidOverlay& operator=(const MojoAndroidOverlay&) = delete;

  ~MojoAndroidOverlay() override;

  // AndroidOverlay
  void ScheduleLayout(const gfx::Rect& rect) override;
  const base::android::JavaRef<jobject>& GetJavaSurface() const override;

  // mojom::AndroidOverlayClient
  void OnSurfaceReady(uint64_t surface_key) override;
  void OnDestroyed() override;
  void OnSynchronouslyDestroyed(
      OnSynchronouslyDestroyedCallback done_cb) override;
  void OnPowerEfficientState(bool is_power_efficient) override;

 private:
  AndroidOverlayConfig config_;
  mojo::Remote<mojom::AndroidOverlay> overlay_;
  mojo::Receiver<mojom::AndroidOverlayClient> receiver_{this};
  gl::ScopedJavaSurface surface_;

  // Have we received OnSurfaceReady yet?
  bool received_surface_ = false;
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_ANDROID_OVERLAY_H_
