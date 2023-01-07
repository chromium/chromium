// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ARCORE_GL_THREAD_H_
#define DEVICE_VR_ANDROID_ARCORE_ARCORE_GL_THREAD_H_

#include <memory>
#include "base/android/java_handler_thread.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "device/vr/android/mailbox_to_surface_bridge.h"

namespace device {

class ArCoreGl;
class ArImageTransportFactory;
class MailboxToSurfaceBridge;

class ArCoreGlThread : public base::android::JavaHandlerThread {
 public:
  ArCoreGlThread(
      std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory,
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge,
      base::OnceCallback<void()> initialized_callback);

  ArCoreGlThread(const ArCoreGlThread&) = delete;
  ArCoreGlThread& operator=(const ArCoreGlThread&) = delete;

  ~ArCoreGlThread() override;
  ArCoreGl* GetArCoreGl();

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory_;
  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;
  base::OnceCallback<void()> initialized_callback_;

  // Created on GL thread.
  std::unique_ptr<ArCoreGl> arcore_gl_;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ARCORE_GL_THREAD_H_
