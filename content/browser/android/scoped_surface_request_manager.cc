// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/scoped_surface_request_manager.h"

#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/service/texture_owner.h"

namespace content {

// static
ScopedSurfaceRequestManager* ScopedSurfaceRequestManager::GetInstance() {
  return base::Singleton<
      ScopedSurfaceRequestManager,
      base::LeakySingletonTraits<ScopedSurfaceRequestManager>>::get();
}

base::UnguessableToken
ScopedSurfaceRequestManager::RegisterScopedSurfaceRequest(
    ScopedSurfaceRequestCB request_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!request_cb.is_null());

  base::UnguessableToken request_token = base::UnguessableToken::Create();

  DCHECK(!request_callbacks_.count(request_token));
  request_callbacks_.insert(
      std::make_pair(request_token, std::move(request_cb)));

  return request_token;
}

void ScopedSurfaceRequestManager::UnregisterScopedSurfaceRequest(
    const base::UnguessableToken& request_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetAndUnregisterInternal(request_token);
}

ScopedSurfaceRequestManager::ScopedSurfaceRequestCB
ScopedSurfaceRequestManager::GetAndUnregisterInternal(
    const base::UnguessableToken& request_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!request_token.is_empty());

  ScopedSurfaceRequestManager::ScopedSurfaceRequestCB request;

  auto it = request_callbacks_.find(request_token);
  if (it != request_callbacks_.end()) {
    request = std::move(it->second);
    request_callbacks_.erase(it);
  }

  return request;
}

void ScopedSurfaceRequestManager::ForwardSurfaceOwnerForSurfaceRequest(
    const base::UnguessableToken& request_token,
    const gpu::TextureOwner* texture_owner) {
  FulfillScopedSurfaceRequest(request_token,
                              texture_owner->CreateJavaSurface());
}

void ScopedSurfaceRequestManager::FulfillScopedSurfaceRequest(
    const base::UnguessableToken& request_token,
    gl::ScopedJavaSurface surface) {
  // base::Unretained is safe because the lifetime of this object is tied to
  // the lifetime of the browser process.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ScopedSurfaceRequestManager::CompleteRequestOnUiThread,
                     base::Unretained(this), request_token,
                     std::move(surface)));
}

void ScopedSurfaceRequestManager::CompleteRequestOnUiThread(
    const base::UnguessableToken& request_token,
    gl::ScopedJavaSurface surface) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ScopedSurfaceRequestManager::ScopedSurfaceRequestCB request =
      GetAndUnregisterInternal(request_token);

  if (!request.is_null())
    std::move(request).Run(std::move(surface));
}

ScopedSurfaceRequestManager::ScopedSurfaceRequestManager() {}

ScopedSurfaceRequestManager::~ScopedSurfaceRequestManager() {}

}  // namespace content
