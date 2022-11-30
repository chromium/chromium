// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <EGL/egl.h>
#include <stdint.h>

#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/gles2_conform_support/egl/config.h"
#include "gpu/gles2_conform_support/egl/context.h"
#include "gpu/gles2_conform_support/egl/display.h"
#include "gpu/gles2_conform_support/egl/surface.h"
#include "gpu/gles2_conform_support/egl/thread_state.h"

using gles2_conform_support::egl::Display;
using gles2_conform_support::egl::ThreadState;

extern "C" {
EGLAPI EGLint EGLAPIENTRY eglGetError() {
  return ThreadState::Get()->ConsumeErrorCode();
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id) {
  if (display_id != EGL_DEFAULT_DISPLAY)
    return EGL_NO_DISPLAY;
  return ThreadState::Get()->GetDefaultDisplay();
}

EGLAPI EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy,
                                            EGLint* major,
                                            EGLint* minor) {
  ThreadState* ts = ThreadState::Get();
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_FALSE);
  return display->Initialize(ts, major, minor);
}

EGLAPI EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy) {
  ThreadState* ts = ThreadState::Get();
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_FALSE);
  return display->Terminate(ts);
}

EGLAPI const char* EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name) {
  ThreadState* ts = ThreadState::Get();
  if (dpy == EGL_NO_DISPLAY) {
    switch (name) {
      case EGL_EXTENSIONS:
        return ts->ReturnSuccess("");
      case EGL_VERSION:
        return ts->ReturnSuccess("1.4");
      default:
        break;
    }
  }
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError<const char*>(EGL_BAD_DISPLAY, nullptr);
  return display->QueryString(ts, name);
}

EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy,
                                              const EGLint* attrib_list,
                                              EGLConfig* configs,
                                              EGLint config_size,
                                              EGLint* num_config) {
  ThreadState* ts = ThreadState::Get();
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_FALSE);
  return display->ChooseConfig(ts, attrib_list, configs, config_size,
                               num_config);
}

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigs(EGLDisplay dpy,
                                            EGLConfig* configs,
                                            EGLint config_size,
                                            EGLint* num_config) {
  ThreadState* ts = ThreadState::Get();
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_FALSE);
  return display->GetConfigs(ts, configs, config_size, num_config);
}

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy,
                                                 EGLConfig cfg,
                                                 EGLint attribute,
                                                 EGLint* value) {
  ThreadState* ts = ThreadState::Get();
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_FALSE);
  return display->GetConfigAttrib(ts, cfg, attribute, value);
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreateWindowSurface(EGLDisplay dpy,
                       EGLConfig cfg,
                       EGLNativeWindowType win,
                       const EGLint* attrib_list) {
  ThreadState* ts = ThreadState::Get();
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_NO_SURFACE);
  return display->CreateWindowSurface(ts, cfg, win, attrib_list);
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreatePbufferSurface(EGLDisplay dpy,
                        EGLConfig cfg,
                        const EGLint* attrib_list) {
  ThreadState* ts = ThreadState::Get();
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_NO_SURFACE);
  return display->CreatePbufferSurface(ts, cfg, attrib_list);
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreatePixmapSurface(EGLDisplay dpy,
                       EGLConfig config,
                       EGLNativePixmapType pixmap,
                       const EGLint* attrib_list) {
  return EGL_NO_SURFACE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay dpy,
                                                EGLSurface sfe) {
  ThreadState* ts = ThreadState::Get();
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_FALSE);
  return display->DestroySurface(ts, sfe);
}

EGLAPI EGLBoolean EGLAPIENTRY eglQuerySurface(EGLDisplay dpy,
                                              EGLSurface surface,
                                              EGLint attribute,
                                              EGLint* value) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglBindAPI(EGLenum api) {
  return EGL_FALSE;
}

EGLAPI EGLenum EGLAPIENTRY eglQueryAPI() {
  return EGL_OPENGL_ES_API;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitClient(void) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglReleaseThread(void) {
  ThreadState::ReleaseThread();
  return EGL_TRUE;
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreatePbufferFromClientBuffer(EGLDisplay dpy,
                                 EGLenum buftype,
                                 EGLClientBuffer buffer,
                                 EGLConfig config,
                                 const EGLint* attrib_list) {
  return EGL_NO_SURFACE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSurfaceAttrib(EGLDisplay dpy,
                                               EGLSurface surface,
                                               EGLint attribute,
                                               EGLint value) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglBindTexImage(EGLDisplay dpy,
                                              EGLSurface surface,
                                              EGLint buffer) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglReleaseTexImage(EGLDisplay dpy,
                                                 EGLSurface surface,
                                                 EGLint buffer) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval) {
  return EGL_FALSE;
}

EGLAPI EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy,
                                               EGLConfig cfg,
                                               EGLContext share_ctx,
                                               const EGLint* attrib_list) {
  ThreadState* ts = ThreadState::Get();
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_NO_CONTEXT);
  return display->CreateContext(ts, cfg, share_ctx, attrib_list);
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy,
                                                EGLContext ctx) {
  ThreadState* ts = ThreadState::Get();
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_FALSE);
  return display->DestroyContext(ts, ctx);
}

EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy,
                                             EGLSurface draw,
                                             EGLSurface read,
                                             EGLContext ctx) {
  ThreadState* ts = ThreadState::Get();
  if (draw == EGL_NO_SURFACE && read == EGL_NO_SURFACE &&
      ctx == EGL_NO_CONTEXT) {
    Display* display =
        dpy == EGL_NO_DISPLAY ? ts->GetDefaultDisplay() : ts->GetDisplay(dpy);
    if (!display)
      return ts->ReturnError(EGL_BAD_DISPLAY, EGL_FALSE);
    return display->ReleaseCurrent(ts);
  }
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_FALSE);
  return display->MakeCurrent(ts, draw, read, ctx);
}

EGLAPI EGLContext EGLAPIENTRY eglGetCurrentContext() {
  return EGL_NO_CONTEXT;
}

EGLAPI EGLSurface EGLAPIENTRY eglGetCurrentSurface(EGLint readdraw) {
  return EGL_NO_SURFACE;
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetCurrentDisplay() {
  return EGL_NO_DISPLAY;
}

EGLAPI EGLBoolean EGLAPIENTRY eglQueryContext(EGLDisplay dpy,
                                              EGLContext ctx,
                                              EGLint attribute,
                                              EGLint* value) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitGL() {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitNative(EGLint engine) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface sfe) {
  ThreadState* ts = ThreadState::Get();
  Display* display = ts->GetDisplay(dpy);
  if (!display)
    return ts->ReturnError(EGL_BAD_DISPLAY, EGL_FALSE);
  return display->SwapBuffers(ts, sfe);
}

EGLAPI EGLBoolean EGLAPIENTRY eglCopyBuffers(EGLDisplay dpy,
                                             EGLSurface surface,
                                             EGLNativePixmapType target) {
  return EGL_FALSE;
}

/* Now, define eglGetProcAddress using the generic function ptr. type */
EGLAPI __eglMustCastToProperFunctionPointerType EGLAPIENTRY
eglGetProcAddress(const char* procname) {
  return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(
      gles2::GetGLFunctionPointer(procname));
}
}  // extern "C"
