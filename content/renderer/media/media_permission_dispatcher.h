// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_H_

#include <stdint.h>

#include <map>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/render_frame_impl.h"
#include "media/base/media_permission.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "media/mojo/mojom/media_foundation_preferences.mojom.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// MediaPermission implementation using content PermissionService.
class MediaPermissionDispatcher : public media::MediaPermission {
 public:
  explicit MediaPermissionDispatcher(RenderFrameImpl* render_frame);

  MediaPermissionDispatcher(const MediaPermissionDispatcher&) = delete;
  MediaPermissionDispatcher& operator=(const MediaPermissionDispatcher&) =
      delete;

  ~MediaPermissionDispatcher() override;

  // Called when the frame owning this MediaPermissionDispatcher is navigated.
  void OnNavigation();

  // media::MediaPermission implementation.
  // Note: Can be called on any thread. The |permission_status_cb| will always
  // be fired on the thread where these methods are called.
  void HasPermission(Type type,
                     PermissionStatusCB permission_status_cb) override;
  void RequestPermission(Type type,
                         PermissionStatusCB permission_status_cb) override;
  bool IsEncryptedMediaEnabled() override;

#if BUILDFLAG(IS_WIN)
  void IsHardwareSecureDecryptionAllowed(
      IsHardwareSecureDecryptionAllowedCB cb) override;
#endif  // BUILDFLAG(IS_WIN)

 private:
  // Map of request IDs and pending PermissionStatusCBs.
  typedef std::map<uint32_t, PermissionStatusCB> RequestMap;

  // Register PermissionStatusCBs. Returns |request_id| that can be used to make
  // PermissionService calls.
  uint32_t RegisterCallback(PermissionStatusCB permission_status_cb);

  // Ensure there is a connection to the permission service and return it.
  blink::mojom::PermissionService* GetPermissionService();

#if BUILDFLAG(IS_WIN)
  // Ensure there is a connection to the media foundation preferences and
  // return it.
  media::mojom::MediaFoundationPreferences* GetMediaFoundationPreferences();

  // Callback for |mf_preferences_| connection errors.
  void OnMediaFoundationPreferencesConnectionError();

  // Callback for |mf_preferences_|
  void OnIsHardwareSecureDecryptionAllowed(bool allowed);
#endif

  // Callback for |permission_service_| calls.
  void OnPermissionStatus(uint32_t request_id,
                          blink::mojom::PermissionStatus status);

  // Callback for |permission_service_| connection errors.
  void OnPermissionServiceConnectionError();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  uint32_t next_request_id_;
  RequestMap requests_;
  mojo::Remote<blink::mojom::PermissionService> permission_service_;

#if BUILDFLAG(IS_WIN)
  mojo::Remote<media::mojom::MediaFoundationPreferences> mf_preferences_;
#endif

  // The |RenderFrameImpl| that owns this MediaPermissionDispatcher.  It's okay
  // to hold a raw pointer here because the lifetime of this object is bounded
  // by the render frame's life (the latter holds a unique pointer to this).
  const raw_ptr<RenderFrameImpl> render_frame_;

  // Used to safely post MediaPermission calls for execution on |task_runner_|.
  base::WeakPtr<MediaPermissionDispatcher> weak_ptr_;

  base::WeakPtrFactory<MediaPermissionDispatcher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_H_
