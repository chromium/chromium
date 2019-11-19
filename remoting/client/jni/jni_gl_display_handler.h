// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_GL_DISPLAY_HANDLER_H_
#define REMOTING_CLIENT_JNI_JNI_GL_DISPLAY_HANDLER_H_

#include <EGL/egl.h>
#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/display/gl_renderer.h"
#include "remoting/client/display/gl_renderer_delegate.h"
#include "remoting/client/queued_task_poster.h"
#include "remoting/protocol/cursor_shape_stub.h"

namespace remoting {

namespace protocol {
class VideoRenderer;
}  // namespace protocol

class ChromotingClientRuntime;

// Handles OpenGL display operations. Draws desktop and cursor on the OpenGL
// surface. The handler should be used and destroyed on the UI thread. It also
// has a core that works on the display thread.
class JniGlDisplayHandler {
 public:
  JniGlDisplayHandler(const base::android::JavaRef<jobject>& java_client);
  ~JniGlDisplayHandler();

  std::unique_ptr<protocol::CursorShapeStub> CreateCursorShapeStub();
  std::unique_ptr<protocol::VideoRenderer> CreateVideoRenderer();

  void OnSurfaceCreated(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jobject>& surface);

  void OnSurfaceChanged(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& caller,
                        int width,
                        int height);

  void OnSurfaceDestroyed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  void OnPixelTransformationChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jfloatArray>&  matrix
      );

  void OnCursorPixelPositionChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      float x,
      float y);

  void OnCursorVisibilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      bool visible);

  void OnCursorInputFeedback(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      float x,
      float y,
      float diameter);

 private:
  class Core;

  // Callbacks from the core.
  void OnRenderDone();
  void OnCanvasSizeChanged(int width, int height);

  ChromotingClientRuntime* runtime_;

  QueuedTaskPoster ui_task_poster_;

  std::unique_ptr<Core> core_;

  base::android::ScopedJavaGlobalRef<jobject> java_display_;

  // Used on UI thread.
  base::WeakPtrFactory<JniGlDisplayHandler> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(JniGlDisplayHandler);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_GL_DISPLAY_HANDLER_H_
