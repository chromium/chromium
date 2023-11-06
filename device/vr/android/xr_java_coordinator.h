// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_XR_JAVA_COORDINATOR_H_
#define DEVICE_VR_ANDROID_XR_JAVA_COORDINATOR_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace device {

class CompositorDelegateProvider;

// Immersive AR sessions use callbacks in the following sequence:
//
// RequestArSession
//   SurfaceReadyCallback
//   SurfaceTouchCallback (repeated for each touch)
//   [exit session via "back" button, or via JS session exit]
//   DestroyedCallback
//
using SurfaceReadyCallback =
    base::RepeatingCallback<void(gfx::AcceleratedWidget window,
                                 gpu::SurfaceHandle handle,
                                 ui::WindowAndroid* root_window,
                                 display::Display::Rotation rotation,
                                 const gfx::Size& size)>;
using SurfaceTouchCallback =
    base::RepeatingCallback<void(bool is_primary,
                                 bool touching,
                                 int32_t pointer_id,
                                 const gfx::PointF& location)>;
using JavaShutdownCallback = base::OnceClosure;

using XrSessionButtonTouchedCallback = base::OnceClosure;

// The purpose of this interface is to allow for dependency injection of code
// that needs to talk Java Code in the WebXR component, which otherwise cannot
// be directly talked to in //device. Unfortunately, the implementation in
// //components must have the same name as the java class for the jni headers
// to build properly; so we're left to come up with a slightly different name
// here.
class XrJavaCoordinator {
 public:
  virtual ~XrJavaCoordinator() = default;
  virtual bool EnsureARCoreLoaded() = 0;
  virtual base::android::ScopedJavaLocalRef<jobject>
  GetCurrentActivityContext() = 0;
  virtual base::android::ScopedJavaLocalRef<jobject> GetActivityFrom(
      int render_process_id,
      int render_frame_id) = 0;
  virtual void RequestArSession(
      int render_process_id,
      int render_frame_id,
      bool use_overlay,
      bool can_render_dom_content,
      const CompositorDelegateProvider& compositor_delegate_provider,
      SurfaceReadyCallback ready_callback,
      SurfaceTouchCallback touch_callback,
      JavaShutdownCallback destroyed_callback) = 0;
  virtual void RequestVrSession(
      int render_process_id,
      int render_frame_id,
      const CompositorDelegateProvider& compositor_delegate_provider,
      SurfaceReadyCallback ready_callback,
      SurfaceTouchCallback touch_callback,
      JavaShutdownCallback destroyed_callback,
      XrSessionButtonTouchedCallback button_touched_callback) = 0;
  virtual void EndSession() = 0;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_XR_JAVA_COORDINATOR_H_
