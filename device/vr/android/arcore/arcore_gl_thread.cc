// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_gl_thread.h"

#include <utility>
#include "base/version.h"
#include "device/vr/android/arcore/ar_image_transport.h"
#include "device/vr/android/arcore/arcore_gl.h"

namespace device {

ArCoreGlThread::ArCoreGlThread(
    std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory,
    std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge,
    base::OnceCallback<void()> initialized_callback)
    : base::android::JavaHandlerThread("ArCoreGL"),
      ar_image_transport_factory_(std::move(ar_image_transport_factory)),
      mailbox_bridge_(std::move(mailbox_bridge)),
      initialized_callback_(std::move(initialized_callback)) {
  DVLOG(3) << __func__;
}

ArCoreGlThread::~ArCoreGlThread() {
  DVLOG(3) << __func__;
  Stop();
}

ArCoreGl* ArCoreGlThread::GetArCoreGl() {
  return arcore_gl_.get();
}

void ArCoreGlThread::Init() {
  DCHECK(!arcore_gl_);

  arcore_gl_ = std::make_unique<ArCoreGl>(
      ar_image_transport_factory_->Create(std::move(mailbox_bridge_)));
  std::move(initialized_callback_).Run();
}

void ArCoreGlThread::CleanUp() {
  arcore_gl_.reset();
}

}  // namespace device
