// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURED_SURFACE_CONTROL_PERMISSION_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURED_SURFACE_CONTROL_PERMISSION_MANAGER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

// Encapsulates the permission state and logic associated with the Captured
// Surface Control API. Objects of this class live on the IO thread.
class CONTENT_EXPORT CapturedSurfaceControlPermissionManager {
 public:
  enum class CapturedSurfaceControlPermissionStatus {
    kGranted,
    kDenied,
    kError,
  };

  explicit CapturedSurfaceControlPermissionManager(
      GlobalRenderFrameHostId capturer_rfh_id);
  virtual ~CapturedSurfaceControlPermissionManager();

  CapturedSurfaceControlPermissionManager(
      const CapturedSurfaceControlPermissionManager&) = delete;
  CapturedSurfaceControlPermissionManager& operator=(
      const CapturedSurfaceControlPermissionManager&) = delete;

  // Checks whether the user has approved the Captured Surface Control APIs.
  // If permission has not yet been granted, attempts to prompt the user.
  //
  // Must be called on the IO thread.
  //
  // Note that if the same app is engaged in multiple concurrent captures,
  // there is no way for the user to know which of these the permission
  // is granted for. This is an extremely rare case, and would be
  // harmless if it does happen.
  virtual void CheckPermission(
      base::OnceCallback<void(CapturedSurfaceControlPermissionStatus)>
          callback);

 private:
  // This static method normally just forwards the call to `manager`, but if
  // `manager` is null, this static method invokes the callback directly,
  // passing in an error. This is done to ensure that the callback is always
  // executed, even if the user closes the captured tab while the prompt is
  // pending. This method runs on the IO thread.
  static void OnCheckResultStatic(
      base::WeakPtr<CapturedSurfaceControlPermissionManager> manager,
      base::OnceCallback<void(CapturedSurfaceControlPermissionStatus)> callback,
      CapturedSurfaceControlPermissionStatus status);

  // This method is invoked on the IO thread as a callback after the user is
  // prompted to approve the use of Captured Surface Control APIs. This method
  // receives the user's choice, updates this object's state accordingly, and
  // invokes `callback` to inform the original caller of CheckPermission() of
  // the result.
  void OnCheckResult(
      base::OnceCallback<void(CapturedSurfaceControlPermissionStatus)> callback,
      CapturedSurfaceControlPermissionStatus status);

  const GlobalRenderFrameHostId capturer_rfh_id_;

  base::WeakPtrFactory<CapturedSurfaceControlPermissionManager> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURED_SURFACE_CONTROL_PERMISSION_MANAGER_H_
