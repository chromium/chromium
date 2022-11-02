// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gles2_conform_support/egl/thread_state.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/lazy_instance.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/common/thread_local.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_util.h"
#include "gpu/gles2_conform_support/egl/context.h"
#include "gpu/gles2_conform_support/egl/display.h"
#include "gpu/gles2_conform_support/egl/surface.h"
#include "gpu/gles2_conform_support/egl/test_support.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace gles2_conform_support {
// Thread local key for ThreadState instance. Accessed when holding g_egl_lock
// only, since the initialization can not be Guaranteed otherwise.  Not in
// anonymous namespace due to Mac OS X 10.6 linker. See gles2_lib.cc.
static gpu::ThreadLocalKey g_egl_thread_state_key;

namespace {
base::LazyInstance<base::Lock>::Leaky g_egl_lock;
int g_egl_active_thread_count;

egl::Display* g_egl_default_display;

#if defined(COMMAND_BUFFER_GLES_LIB_SUPPORT_ONLY)
// egl::Display is used for comformance tests and command_buffer_gles.  We only
// need the exit manager for the command_buffer_gles library.
base::AtExitManager* g_exit_manager;
#endif
}  // namespace

namespace egl {

egl::ThreadState* ThreadState::Get() {
  base::AutoLock lock(g_egl_lock.Get());
  if (g_egl_active_thread_count == 0) {
#if defined(COMMAND_BUFFER_GLES_LIB_SUPPORT_ONLY)
#if defined(COMPONENT_BUILD)
    if (!g_command_buffer_gles_has_atexit_manager)
      g_exit_manager = new base::AtExitManager;
#else
    g_exit_manager = new base::AtExitManager;
#endif
#endif
    gles2::Initialize();

    if (gl::GetGLImplementation() == gl::kGLImplementationNone) {
      base::CommandLine::StringVector argv;
      std::unique_ptr<base::Environment> env(base::Environment::Create());
      std::string env_string;
      env->GetVar("CHROME_COMMAND_BUFFER_GLES2_ARGS", &env_string);
#if BUILDFLAG(IS_WIN)
      argv =
          base::SplitString(base::UTF8ToWide(env_string), base::kWhitespaceWide,
                            base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      argv.insert(argv.begin(), base::UTF8ToWide("dummy"));
#else
      argv =
          base::SplitString(env_string, base::kWhitespaceASCII,
                            base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      argv.insert(argv.begin(), "dummy");
#endif
      base::CommandLine::Init(0, nullptr);
      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
      // Need to call both Init and InitFromArgv, since Windows does not use
      // argc, argv in CommandLine::Init(argc, argv).
      command_line->InitFromArgv(argv);
#if BUILDFLAG(IS_OZONE)
      ui::OzonePlatform::InitializeForGPU(ui::OzonePlatform::InitParams());
#endif
      gl::GLDisplay* display =
          gl::init::InitializeGLNoExtensionsOneOff(/*init_bindings=*/true,
                                                   /*system_device_id=*/0);
      gpu::GpuFeatureInfo gpu_feature_info;
      if (!command_line->HasSwitch(switches::kDisableGpuDriverBugWorkarounds)) {
        gpu::GPUInfo gpu_info;
        gpu::CollectGraphicsInfoForTesting(&gpu_info);
        gpu_feature_info = gpu::ComputeGpuFeatureInfo(
            gpu_info, gpu::GpuPreferences(), command_line, nullptr);
        Context::SetPlatformGpuFeatureInfo(gpu_feature_info);
      }

      gl::init::SetDisabledExtensionsPlatform(
          gpu_feature_info.disabled_extensions);
      gl::init::InitializeExtensionSettingsOneOffPlatform(display);
    }

    g_egl_default_display = new egl::Display();
    g_egl_thread_state_key = gpu::ThreadLocalAlloc();
  }
  egl::ThreadState* thread_state = static_cast<egl::ThreadState*>(
      gpu::ThreadLocalGetValue(g_egl_thread_state_key));
  if (!thread_state) {
    thread_state = new egl::ThreadState;
    gpu::ThreadLocalSetValue(g_egl_thread_state_key, thread_state);
    ++g_egl_active_thread_count;
  }
  return thread_state;
}

void ThreadState::ReleaseThread() {
  base::AutoLock lock(g_egl_lock.Get());
  if (g_egl_active_thread_count == 0)
    return;

  egl::ThreadState* thread_state = static_cast<egl::ThreadState*>(
      gpu::ThreadLocalGetValue(g_egl_thread_state_key));
  if (!thread_state)
    return;

  --g_egl_active_thread_count;
  if (g_egl_active_thread_count > 0) {
    g_egl_default_display->ReleaseCurrent(thread_state);
    delete thread_state;
  } else {
    gpu::ThreadLocalFree(g_egl_thread_state_key);

    // First delete the display object, so that it drops the possible refs to
    // current context.
    delete g_egl_default_display;
    g_egl_default_display = nullptr;

    // We can use Surface and Context without lock, since there's no threads
    // left anymore. Destroy the current context explicitly, in an attempt to
    // reduce the number of error messages abandoned context would produce.
    if (thread_state->current_context()) {
      Context::MakeCurrent(thread_state->current_context(),
                           thread_state->current_surface(), nullptr, nullptr);
    }
    delete thread_state;

    gles2::Terminate();
#if defined(COMMAND_BUFFER_GLES_LIB_SUPPORT_ONLY)
#if defined(COMPONENT_BUILD)
    if (g_command_buffer_gles_has_atexit_manager)
      delete g_exit_manager;
#else
    delete g_exit_manager;
#endif
    g_exit_manager = nullptr;
#endif
  }
}

ThreadState::ThreadState() : error_code_(EGL_SUCCESS) {}

ThreadState::~ThreadState() = default;

EGLint ThreadState::ConsumeErrorCode() {
  EGLint current_error_code = error_code_;
  error_code_ = EGL_SUCCESS;
  return current_error_code;
}

Display* ThreadState::GetDisplay(EGLDisplay dpy) {
  if (dpy == g_egl_default_display)
    return g_egl_default_display;
  return nullptr;
}

Display* ThreadState::GetDefaultDisplay() {
  return g_egl_default_display;
}

void ThreadState::SetCurrent(Surface* surface, Context* context) {
  DCHECK((surface == nullptr) == (context == nullptr));
  if (current_context_) {
    current_context_->set_is_current_in_some_thread(false);
    current_surface_->set_is_current_in_some_thread(false);
  }
  current_surface_ = surface;
  current_context_ = context;
  if (current_context_) {
    current_context_->set_is_current_in_some_thread(true);
    current_surface_->set_is_current_in_some_thread(true);
  }
}

ThreadState::AutoCurrentContextRestore::AutoCurrentContextRestore(
    ThreadState* thread_state)
    : thread_state_(thread_state) {}

ThreadState::AutoCurrentContextRestore::~AutoCurrentContextRestore() {
  if (Context* current_context = thread_state_->current_context()) {
    current_context->ApplyCurrentContext(
        thread_state_->current_surface()->gl_surface());
  } else {
    Context::ApplyContextReleased();
  }
}

void ThreadState::AutoCurrentContextRestore::SetCurrent(Surface* surface,
                                                        Context* context) {
  thread_state_->SetCurrent(surface, context);
}

}  // namespace egl
}  // namespace gles2_conform_support
