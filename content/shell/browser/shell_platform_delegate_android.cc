// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_delegate.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/shell/android/shell_manager.h"
#include "content/shell/browser/shell.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/shell/android/content_shell_jni_headers/Shell_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

struct ShellPlatformDelegate::ShellData {
  base::android::ScopedJavaGlobalRef<jobject> java_object;
};

struct ShellPlatformDelegate::PlatformData {};

ShellPlatformDelegate::ShellPlatformDelegate() = default;

void ShellPlatformDelegate::Initialize(const gfx::Size& default_window_size) {
  // |platform_| is not used on this platform.
}

ShellPlatformDelegate::~ShellPlatformDelegate() {
  DestroyShellManager();
}

void ShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  DCHECK(!base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  shell_data.java_object.Reset(CreateShellView(shell));
}

void ShellPlatformDelegate::CleanUp(Shell* shell) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  RemoveShellView(shell_data.java_object);

  if (!shell_data.java_object.is_null())
    Java_Shell_onNativeDestroyed(env, shell_data.java_object);

  shell_data_map_.erase(shell);
}

void ShellPlatformDelegate::SetContents(Shell* shell) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  Java_Shell_initFromNativeTabContents(
      env, shell_data.java_object, shell->web_contents()->GetJavaWebContents());
}

void ShellPlatformDelegate::ResizeWebContent(Shell* shell,
                                             const gfx::Size& content_size) {
  shell->web_contents()->GetRenderWidgetHostView()->SetSize(content_size);
}

void ShellPlatformDelegate::EnableUIControl(Shell* shell,
                                            UIControl control,
                                            bool is_enabled) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  if (shell_data.java_object.is_null())
    return;
  Java_Shell_enableUiControl(env, shell_data.java_object, control, is_enabled);
}

void ShellPlatformDelegate::SetAddressBarURL(Shell* shell, const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env, url.spec());
  Java_Shell_onUpdateUrl(env, shell_data.java_object, j_url);
}

void ShellPlatformDelegate::SetIsLoading(Shell* shell, bool loading) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  Java_Shell_setIsLoading(env, shell_data.java_object, loading);
}

void ShellPlatformDelegate::SetTitle(Shell* shell,
                                     const std::u16string& title) {}

void ShellPlatformDelegate::MainFrameCreated(Shell* shell) {}

bool ShellPlatformDelegate::DestroyShell(Shell* shell) {
  return false;  // Shell destroys itself.
}

void ShellPlatformDelegate::ToggleFullscreenModeForTab(
    Shell* shell,
    WebContents* web_contents,
    bool enter_fullscreen) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  Java_Shell_toggleFullscreenModeForTab(env, shell_data.java_object,
                                        enter_fullscreen);
}

bool ShellPlatformDelegate::IsFullscreenForTabOrPending(
    Shell* shell,
    const WebContents* web_contents) const {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(base::Contains(shell_data_map_, shell));
  const ShellData& shell_data = shell_data_map_.find(shell)->second;

  return Java_Shell_isFullscreenForTabOrPending(env, shell_data.java_object);
}

void ShellPlatformDelegate::SetOverlayMode(Shell* shell,
                                           bool use_overlay_mode) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  return Java_Shell_setOverlayMode(env, shell_data.java_object,
                                   use_overlay_mode);
}

void ShellPlatformDelegate::LoadProgressChanged(Shell* shell, double progress) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  Java_Shell_onLoadProgressChanged(env, shell_data.java_object, progress);
}

// static
void JNI_Shell_CloseShell(JNIEnv* env, jlong shellPtr) {
  Shell* shell = reinterpret_cast<Shell*>(shellPtr);
  shell->Close();
}

}  // namespace content
