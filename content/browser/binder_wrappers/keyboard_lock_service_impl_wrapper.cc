// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/binder_wrappers/keyboard_lock_service_impl_wrapper.h"

#include "content/browser/keyboard_lock/keyboard_lock_service_impl.h"
#include "third_party/blink/public/mojom/keyboard_lock/keyboard_lock.mojom.h"

namespace content {

namespace internal {

void KeyboardLockServiceImplCreateMojoService(
    mojo::BinderMapWithContext<RenderFrameHost*>& map) {
  map.Add<blink::mojom::KeyboardLockService>(
      base::BindRepeating(&KeyboardLockServiceImpl::CreateMojoService));
}

}  // namespace internal

}  // namespace content
