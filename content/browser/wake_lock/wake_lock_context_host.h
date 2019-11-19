// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_CONTEXT_HOST_H_
#define CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_CONTEXT_HOST_H_

#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// On Android, WakeLockContext requires the NativeView associated with the
// context in order to lock the screen. WakeLockContextHost provides this
// functionality by mapping WakeLockContext IDs to the WebContents associated
// with those IDs.
class WakeLockContextHost {
 public:
  explicit WakeLockContextHost(WebContents* web_contents);
  ~WakeLockContextHost();

  // This callback is passed into the DeviceService constructor in order to
  // enable WakeLockContext to map a context ID to a Native View as necessary.
  static gfx::NativeView GetNativeViewForContext(int context_id);

  // Returns the WakeLockContext* to which this instance is connected.
  device::mojom::WakeLockContext* GetWakeLockContext() {
    return wake_lock_context_ ? wake_lock_context_.get() : nullptr;
  }

 private:
  // This instance's ID.
  int id_;

  // The WebContents that owns this instance.
  WebContents* web_contents_;

  // The WakeLockContext instance that is connected to this instance.
  mojo::Remote<device::mojom::WakeLockContext> wake_lock_context_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockContextHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_CONTEXT_HOST_H_
