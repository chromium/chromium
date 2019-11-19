// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/shell/android/content_shell_jni_headers/Shell_jni.h"
#include "content/shell/android/shell_manager.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

void Shell::PlatformInitialize(const gfx::Size& default_window_size) {
}

void Shell::PlatformExit() {
  DestroyShellManager();
}

void Shell::PlatformCleanUp() {
  JNIEnv* env = AttachCurrentThread();
  if (java_object_.is_null())
    return;
  Java_Shell_onNativeDestroyed(env, java_object_);
}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {
  JNIEnv* env = AttachCurrentThread();
  if (java_object_.is_null())
    return;
  Java_Shell_enableUiControl(env, java_object_, control, is_enabled);
}

void Shell::PlatformSetAddressBarURL(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env, url.spec());
  Java_Shell_onUpdateUrl(env, java_object_, j_url);
}

void Shell::PlatformSetIsLoading(bool loading) {
  JNIEnv* env = AttachCurrentThread();
  Java_Shell_setIsLoading(env, java_object_, loading);
}

void Shell::PlatformCreateWindow(int width, int height) {
  java_object_.Reset(CreateShellView(this));
}

void Shell::PlatformSetContents() {
  JNIEnv* env = AttachCurrentThread();
  Java_Shell_initFromNativeTabContents(env, java_object_,
                                       web_contents()->GetJavaWebContents());
}

void Shell::PlatformResizeSubViews() {
  // Not needed; subviews are bound.
}

void Shell::SizeTo(const gfx::Size& content_size) {
  JNIEnv* env = AttachCurrentThread();
  Java_Shell_sizeTo(env, java_object_, content_size.width(),
                    content_size.height());
}

void Shell::PlatformSetTitle(const base::string16& title) {
  NOTIMPLEMENTED() << ": " << title;
}

void Shell::SetOverlayMode(bool use_overlay_mode) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_Shell_setOverlayMode(env, java_object_, use_overlay_mode);
}

void Shell::PlatformToggleFullscreenModeForTab(WebContents* web_contents,
                                               bool enter_fullscreen) {
  JNIEnv* env = AttachCurrentThread();
  Java_Shell_toggleFullscreenModeForTab(env, java_object_, enter_fullscreen);
}

bool Shell::PlatformIsFullscreenForTabOrPending(
    const WebContents* web_contents) const {
  JNIEnv* env = AttachCurrentThread();
  return Java_Shell_isFullscreenForTabOrPending(env, java_object_);
}

void Shell::LoadProgressChanged(double progress) {
  JNIEnv* env = AttachCurrentThread();
  Java_Shell_onLoadProgressChanged(env, java_object_, progress);
}

void Shell::Close() {
  RemoveShellView(java_object_);
  delete this;
}

// static
void JNI_Shell_CloseShell(JNIEnv* env,
                          jlong shellPtr) {
  Shell* shell = reinterpret_cast<Shell*>(shellPtr);
  shell->Close();
}

}  // namespace content
