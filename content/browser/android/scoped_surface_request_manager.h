// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SCOPED_SURFACE_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_ANDROID_SCOPED_SURFACE_REQUEST_MANAGER_H_

#include <unordered_map>

#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "gpu/ipc/common/android/scoped_surface_request_conduit.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace content {

class CONTENT_EXPORT ScopedSurfaceRequestManager
    : public gpu::ScopedSurfaceRequestConduit {
 public:
  static ScopedSurfaceRequestManager* GetInstance();

  ScopedSurfaceRequestManager(const ScopedSurfaceRequestManager&) = delete;
  ScopedSurfaceRequestManager& operator=(const ScopedSurfaceRequestManager&) =
      delete;

  using ScopedSurfaceRequestCB =
      base::OnceCallback<void(gl::ScopedJavaSurface)>;

  // Registers a request, and returns the |request_token| that should be used to
  // call Fulfill at a later time. The caller is responsible for unregistering
  // the request, if it is destroyed before the request is fulfilled.
  // It is the requester's responsibility to check the validity of the final
  // ScopedJavaSurface (as passing an empty surface is a valid operation).
  // Must be called on the UI thread.
  base::UnguessableToken RegisterScopedSurfaceRequest(
      ScopedSurfaceRequestCB request_cb);

  // Unregisters a request registered under |request_token| if it exists,
  // no-ops otherwise.
  // Must be called on the UI thread.
  void UnregisterScopedSurfaceRequest(
      const base::UnguessableToken& request_token);

  // Unregisters and runs the request callback identified by |request_token| if
  // one exists, no-ops otherwise.
  // Passing an empty |surface| is a valid operation that will complete the
  // request.
  // Can be called from any thread. The request will be posted to the UI thread.
  void FulfillScopedSurfaceRequest(const base::UnguessableToken& request_token,
                                   gl::ScopedJavaSurface surface);

  // Implementation of ScopedSurfaceRequestConduit.
  // To be used in the single process case.
  // Can be called from any thread.
  void ForwardSurfaceOwnerForSurfaceRequest(
      const base::UnguessableToken& request_token,
      const gpu::TextureOwner* texture_owner) override;

  void clear_requests_for_testing() { request_callbacks_.clear(); }

  int request_count_for_testing() { return request_callbacks_.size(); }

 private:
  friend struct base::DefaultSingletonTraits<ScopedSurfaceRequestManager>;

  // Unregisters and returns the request identified by |request_token|.
  ScopedSurfaceRequestCB GetAndUnregisterInternal(
      const base::UnguessableToken& request_token);

  void CompleteRequestOnUiThread(const base::UnguessableToken& request_token,
                                 gl::ScopedJavaSurface surface);

  // Map used to hold references to the registered callbacks.
  std::unordered_map<base::UnguessableToken,
                     ScopedSurfaceRequestCB,
                     base::UnguessableTokenHash>
      request_callbacks_;

  ScopedSurfaceRequestManager();
  ~ScopedSurfaceRequestManager() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SCOPED_SURFACE_REQUEST_MANAGER_H_
