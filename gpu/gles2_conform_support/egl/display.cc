// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gles2_conform_support/egl/display.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "gpu/gles2_conform_support/egl/config.h"
#include "gpu/gles2_conform_support/egl/context.h"
#include "gpu/gles2_conform_support/egl/surface.h"
#include "gpu/gles2_conform_support/egl/thread_state.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace gles2_conform_support {
namespace egl {

Display::Display()
    : is_initialized_(false),
      next_create_window_surface_creates_pbuffer_(false),
      window_surface_pbuffer_width_(0),
      window_surface_pbuffer_height_(0) {}

Display::~Display() {
  surfaces_.clear();
  contexts_.clear();
}
void Display::SetNextCreateWindowSurfaceCreatesPBuffer(EGLint width,
                                                       EGLint height) {
  next_create_window_surface_creates_pbuffer_ = true;
  window_surface_pbuffer_width_ = width;
  window_surface_pbuffer_height_ = height;
}

EGLBoolean Display::Initialize(ThreadState* ts, EGLint* major, EGLint* minor) {
  base::AutoLock auto_lock(lock_);
  is_initialized_ = true;

  if (major)
    *major = 1;
  if (minor)
    *minor = 4;
  return ts->ReturnSuccess(EGL_TRUE);
}

EGLBoolean Display::Terminate(ThreadState* ts) {
  base::AutoLock auto_lock(lock_);
  is_initialized_ = false;
  surfaces_.clear();
  for (const auto& context : contexts_)
    context->MarkDestroyed();
  contexts_.clear();
  return ts->ReturnSuccess(EGL_TRUE);
}

const char* Display::QueryString(ThreadState* ts, EGLint name) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnError<const char*>(EGL_NOT_INITIALIZED, nullptr);
  switch (name) {
    case EGL_CLIENT_APIS:
      return ts->ReturnSuccess("OpenGL_ES");
    case EGL_EXTENSIONS:
      return ts->ReturnSuccess("");
    case EGL_VENDOR:
      return ts->ReturnSuccess("Google LLC");
    case EGL_VERSION:
      return ts->ReturnSuccess("1.4");
    default:
      return ts->ReturnError<const char*>(EGL_BAD_PARAMETER, nullptr);
  }
}

EGLBoolean Display::ChooseConfig(ThreadState* ts,
                                 const EGLint* attrib_list,
                                 EGLConfig* configs,
                                 EGLint config_size,
                                 EGLint* num_config) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnError(EGL_NOT_INITIALIZED, EGL_FALSE);
  if (num_config == nullptr)
    return ts->ReturnError(EGL_BAD_PARAMETER, EGL_FALSE);
  if (!Config::ValidateAttributeList(attrib_list))
    return ts->ReturnError(EGL_BAD_ATTRIBUTE, EGL_FALSE);
  InitializeConfigsIfNeeded();
  if (!configs)
    config_size = 0;
  *num_config = 0;
  for (size_t i = 0; i < std::size(configs_); ++i) {
    if (configs_[i]->Matches(attrib_list)) {
      if (*num_config < config_size) {
        configs[*num_config] = configs_[i].get();
      }
      ++*num_config;
    }
  }
  return ts->ReturnSuccess(EGL_TRUE);
}

EGLBoolean Display::GetConfigs(ThreadState* ts,
                               EGLConfig* configs,
                               EGLint config_size,
                               EGLint* num_config) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnError(EGL_NOT_INITIALIZED, EGL_FALSE);
  if (num_config == nullptr)
    return ts->ReturnError(EGL_BAD_PARAMETER, EGL_FALSE);
  InitializeConfigsIfNeeded();
  if (!configs)
    config_size = 0;
  *num_config = std::size(configs_);
  size_t count =
      std::min(std::size(configs_), static_cast<size_t>(config_size));
  for (size_t i = 0; i < count; ++i)
    configs[i] = configs_[i].get();
  return ts->ReturnSuccess(EGL_TRUE);
}

bool Display::IsValidNativeWindow(EGLNativeWindowType win) {
#if BUILDFLAG(IS_WIN)
  return ::IsWindow(win) != FALSE;
#else
  // TODO(alokp): Validate window handle.
  return true;
#endif  // BUILDFLAG(IS_WIN)
}

EGLBoolean Display::GetConfigAttrib(ThreadState* ts,
                                    EGLConfig cfg,
                                    EGLint attribute,
                                    EGLint* value) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnError(EGL_NOT_INITIALIZED, EGL_FALSE);
  const egl::Config* config = GetConfig(cfg);
  if (!config)
    return ts->ReturnError(EGL_BAD_CONFIG, EGL_FALSE);
  if (!config->GetAttrib(attribute, value))
    return ts->ReturnError(EGL_BAD_ATTRIBUTE, EGL_FALSE);
  return ts->ReturnSuccess(EGL_TRUE);
}

EGLSurface Display::CreatePbufferSurface(ThreadState* ts,
                                         EGLConfig cfg,
                                         const EGLint* attrib_list) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnError(EGL_NOT_INITIALIZED, EGL_NO_SURFACE);
  const egl::Config* config = GetConfig(cfg);
  if (!config)
    return ts->ReturnError(EGL_BAD_CONFIG, EGL_NO_SURFACE);
  EGLint value = EGL_NONE;
  config->GetAttrib(EGL_SURFACE_TYPE, &value);
  if ((value & EGL_PBUFFER_BIT) == 0)
    return ts->ReturnError(EGL_BAD_MATCH, EGL_NO_SURFACE);
  if (!egl::Surface::ValidatePbufferAttributeList(attrib_list))
    return ts->ReturnError(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);

  int width = 1;
  int height = 1;
  if (attrib_list) {
    for (const int32_t* attr = attrib_list; attr[0] != EGL_NONE; attr += 2) {
      switch (attr[0]) {
        case EGL_WIDTH:
          width = attr[1];
          break;
        case EGL_HEIGHT:
          height = attr[1];
          break;
      }
    }
  }
  return DoCreatePbufferSurface(ts, config, width, height);
}

EGLSurface Display::DoCreatePbufferSurface(ThreadState* ts,
                                           const Config* config,
                                           EGLint width,
                                           EGLint height) {
  lock_.AssertAcquired();
  scoped_refptr<gl::GLSurface> gl_surface;
  gl_surface = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(),
                                                  gfx::Size(width, height));
  if (!gl_surface)
    return ts->ReturnError(EGL_BAD_ALLOC, nullptr);
  surfaces_.emplace_back(new Surface(gl_surface.get(), config));
  return ts->ReturnSuccess<EGLSurface>(surfaces_.back().get());
}

EGLSurface Display::CreateWindowSurface(ThreadState* ts,
                                        EGLConfig cfg,
                                        EGLNativeWindowType win,
                                        const EGLint* attrib_list) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnError(EGL_NOT_INITIALIZED, EGL_NO_SURFACE);
  const egl::Config* config = GetConfig(cfg);
  if (!config)
    return ts->ReturnError(EGL_BAD_CONFIG, EGL_NO_SURFACE);
  EGLint value = EGL_NONE;
  config->GetAttrib(EGL_SURFACE_TYPE, &value);
  if ((value & EGL_WINDOW_BIT) == 0)
    return ts->ReturnError(EGL_BAD_CONFIG, EGL_NO_SURFACE);
  if (!next_create_window_surface_creates_pbuffer_ && !IsValidNativeWindow(win))
    return ts->ReturnError(EGL_BAD_NATIVE_WINDOW, EGL_NO_SURFACE);
  if (!Surface::ValidateWindowAttributeList(attrib_list))
    return ts->ReturnError(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
  if (next_create_window_surface_creates_pbuffer_) {
    EGLSurface result = DoCreatePbufferSurface(ts, config,
                                               window_surface_pbuffer_width_,
                                               window_surface_pbuffer_height_);
    next_create_window_surface_creates_pbuffer_ = false;
    window_surface_pbuffer_width_ = 0;
    window_surface_pbuffer_height_ = 0;
    return result;
  }
  scoped_refptr<gl::GLSurface> gl_surface;
#if BUILDFLAG(IS_MAC)
  gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;
#else
  gfx::AcceleratedWidget widget = static_cast<gfx::AcceleratedWidget>(win);
#endif
  gl_surface = gl::init::CreateViewGLSurface(gl::GetDefaultDisplay(), widget);
  if (!gl_surface)
    return ts->ReturnError(EGL_BAD_ALLOC, EGL_NO_SURFACE);
  surfaces_.emplace_back(new Surface(gl_surface.get(), config));
  return ts->ReturnSuccess(surfaces_.back().get());
}

EGLBoolean Display::DestroySurface(ThreadState* ts, EGLSurface sfe) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnError(EGL_NOT_INITIALIZED, EGL_FALSE);
  auto it = base::ranges::find(surfaces_, sfe);
  if (it == surfaces_.end())
    return ts->ReturnError(EGL_BAD_SURFACE, EGL_FALSE);
  surfaces_.erase(it);
  return ts->ReturnSuccess(EGL_TRUE);
}

EGLBoolean Display::ReleaseCurrent(ThreadState* ts) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnSuccess(EGL_TRUE);
  ThreadState::AutoCurrentContextRestore accr(ts);
  if (ts->current_context()) {
    Context::MakeCurrent(ts->current_context(), ts->current_surface(), nullptr,
                         nullptr);
    accr.SetCurrent(nullptr, nullptr);
  }
  return ts->ReturnSuccess(EGL_TRUE);
}

EGLBoolean Display::MakeCurrent(ThreadState* ts,
                                EGLSurface draw,
                                EGLSurface read,
                                EGLSurface ctx) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnError(EGL_NOT_INITIALIZED, EGL_FALSE);
  ThreadState::AutoCurrentContextRestore accr(ts);
  // Client might have called use because it changed some other gl binding
  // global state. For example, the client might have called eglMakeCurrent on
  // the same EGL as what command buffer uses. The client probably knows that
  // this invalidates the internal state of command buffer, too. So reset the
  // current context with accr in any case, regardless whether context or
  // surface pointer changes.
  Surface* new_surface = GetSurface(draw);
  if (!new_surface)
    return ts->ReturnError(EGL_BAD_SURFACE, EGL_FALSE);
  new_surface = GetSurface(read);
  if (!new_surface)
    return ts->ReturnError(EGL_BAD_SURFACE, EGL_FALSE);
  egl::Context* new_context = GetContext(ctx);
  if (!new_context)
    return ts->ReturnError(EGL_BAD_CONTEXT, EGL_FALSE);
  if (draw != read)
    return ts->ReturnError(EGL_BAD_MATCH, EGL_FALSE);

  Surface* current_surface = ts->current_surface();
  Context* current_context = ts->current_context();

  if (current_context != new_context &&
      new_context->is_current_in_some_thread())
    return ts->ReturnError(EGL_BAD_ACCESS, EGL_FALSE);

  if (current_surface != new_surface &&
      new_surface->is_current_in_some_thread())
    return ts->ReturnError(EGL_BAD_ACCESS, EGL_FALSE);

  if (!Context::MakeCurrent(current_context,
                            current_context ? current_surface : nullptr,
                            new_context, new_context ? new_surface : nullptr))
    return ts->ReturnError(EGL_BAD_MATCH, EGL_FALSE);

  accr.SetCurrent(new_surface, new_context);
  return ts->ReturnSuccess(EGL_TRUE);
}

EGLBoolean Display::SwapBuffers(ThreadState* ts, EGLSurface sfe) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnError(EGL_NOT_INITIALIZED, EGL_FALSE);
  egl::Surface* surface = GetSurface(sfe);
  if (!surface)
    return ts->ReturnError(EGL_BAD_SURFACE, EGL_FALSE);
  if (ts->current_surface() != surface)
    return ts->ReturnError(EGL_BAD_SURFACE, EGL_FALSE);
  if (!ts->current_context()->SwapBuffers(surface))
    ts->ReturnError(EGL_CONTEXT_LOST, EGL_FALSE);
  return ts->ReturnSuccess(EGL_TRUE);
}

EGLContext Display::CreateContext(ThreadState* ts,
                                  EGLConfig cfg,
                                  EGLContext share_ctx,
                                  const EGLint* attrib_list) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnError(EGL_NOT_INITIALIZED, EGL_NO_CONTEXT);
  if (share_ctx != EGL_NO_CONTEXT) {
    egl::Context* share_context = GetContext(share_ctx);
    if (!share_context)
      return ts->ReturnError(EGL_BAD_CONTEXT, EGL_NO_CONTEXT);
    // TODO(alokp): Add support for shared contexts.
    return ts->ReturnError(EGL_BAD_MATCH, EGL_NO_CONTEXT);
  }

  if (!egl::Context::ValidateAttributeList(attrib_list))
    return ts->ReturnError(EGL_BAD_ATTRIBUTE, EGL_NO_CONTEXT);
  const egl::Config* config = GetConfig(cfg);
  if (!config)
    return ts->ReturnError(EGL_BAD_CONFIG, EGL_NO_CONTEXT);
  scoped_refptr<Context> context(new Context(this, config));
  if (!context)
    return ts->ReturnError(EGL_BAD_ALLOC, EGL_NO_CONTEXT);
  contexts_.emplace_back(context.get());
  return ts->ReturnSuccess<EGLContext>(context.get());
}

EGLBoolean Display::DestroyContext(ThreadState* ts, EGLContext ctx) {
  base::AutoLock auto_lock(lock_);
  if (!is_initialized_)
    return ts->ReturnError(EGL_NOT_INITIALIZED, EGL_FALSE);
  auto it = base::ranges::find(contexts_, ctx);
  if (it == contexts_.end())
    return ts->ReturnError(EGL_BAD_CONTEXT, EGL_FALSE);
  (*it)->MarkDestroyed();
  contexts_.erase(it);
  return ts->ReturnSuccess(EGL_TRUE);
}

uint64_t Display::GenerateFenceSyncRelease() {
  base::AutoLock auto_lock(lock_);
  return next_fence_sync_release_++;
}

void Display::InitializeConfigsIfNeeded() {
  lock_.AssertAcquired();
  if (!configs_[0]) {
    // The interface offers separate configs for window and pbuffer.
    // This way we can record the client intention at context creation time.
    // The GL implementation (gl::GLContext and gl::GLSurface) needs this
    // distinction when creating a context.
    configs_[0] = std::make_unique<Config>(EGL_WINDOW_BIT);
    configs_[1] = std::make_unique<Config>(EGL_PBUFFER_BIT);
  }
}

const Config* Display::GetConfig(EGLConfig cfg) {
  lock_.AssertAcquired();
  for (const auto& config : configs_) {
    if (config.get() == cfg)
      return config.get();
  }
  return nullptr;
}

Surface* Display::GetSurface(EGLSurface surface) {
  lock_.AssertAcquired();
  auto it = base::ranges::find(surfaces_, surface);
  if (it == surfaces_.end())
    return nullptr;
  return it->get();
}

Context* Display::GetContext(EGLContext context) {
  lock_.AssertAcquired();
  auto it = base::ranges::find(contexts_, context);
  if (it == contexts_.end())
    return nullptr;
  return it->get();
}

}  // namespace egl
}  // namespace gles2_conform_support
