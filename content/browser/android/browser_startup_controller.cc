// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_startup_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/browser/android/content_startup_flags.h"
#include "content/browser/browser_main_loop.h"
#include "ppapi/buildflags/buildflags.h"

#include "content/public/android/content_jni_headers/BrowserStartupControllerImpl_jni.h"

#include <sys/time.h>
#include <sys/resource.h>

#ifdef __aarch64__
	#define setpriorityName Java_com_byte4byte_cloudretro64_ContentShellActivity_setpriority
	#define SetCommandLineFlagsName Java_com_byte4byte_cloudretro64_ContentShellActivity_SetCommandLineFlags
	#define setEcUrlName Java_com_byte4byte_cloudretro64_ContentShellActivity_setEcUrl
#else
	#define setpriorityName Java_com_byte4byte_cloudretro32_ContentShellActivity_setpriority
	#define SetCommandLineFlagsName Java_com_byte4byte_cloudretro32_ContentShellActivity_SetCommandLineFlags
	#define setEcUrlName Java_com_byte4byte_cloudretro32_ContentShellActivity_setEcUrl
#endif

extern "C" {
JNIEXPORT void JNICALL setpriorityName(
    JNIEnv* env,
    jclass,
    jint prio) {
		setpriority(PRIO_PROCESS, 0, prio);
	}
}

extern "C" {
	JNIEXPORT void setEcUrlName(JNIEnv* env,
    jclass,
    jstring url) {
		const char *str;
		str = env->GetStringUTFChars(url, NULL); //1
		//printf("Hello %s\n", str);
		std::string url_str = str;
		env->ReleaseStringUTFChars(url, str); //
		
		content::setEcUrl(url_str);

	}
}

extern "C" {
JNIEXPORT void JNICALL SetCommandLineFlagsName(
    JNIEnv* env,
    jclass,
    jstring plugin_descriptor,
	jstring f_dir,
	jstring f_eclibdir,
	jstring f_wwwdir) {
		
		
		
		const char *str;
    str = env->GetStringUTFChars(plugin_descriptor, NULL); //1
    //printf("Hello %s\n", str);
	std::string plugin_str = str;
    env->ReleaseStringUTFChars(plugin_descriptor, str); //
	
	const char *fdirstr;
	fdirstr = env->GetStringUTFChars(f_dir, NULL); //1
    //printf("Hello %s\n", str);
	std::string files_dir = fdirstr;
    env->ReleaseStringUTFChars(f_dir, fdirstr); //
	
	
	const char *f_eclibdirstr;
	f_eclibdirstr = env->GetStringUTFChars(f_eclibdir, NULL); //1
    //printf("Hello %s\n", str);
	std::string eclib_dir = f_eclibdirstr;
    env->ReleaseStringUTFChars(f_eclibdir, f_eclibdirstr); //
	
	
	const char *f_wwwstr;
	f_wwwstr = env->GetStringUTFChars(f_wwwdir, NULL); //1
    //printf("Hello %s\n", str);
	std::string ecwww_dir = f_wwwstr;
    env->ReleaseStringUTFChars(f_wwwdir, f_wwwstr); //
		
 /* std::string plugin_str =
      (plugin_descriptor == NULL
           ? std::string()
           : base::android::ConvertJavaStringToUTF8(env, plugin_descriptor));*/
  content::SetContentCommandLineFlags(false, plugin_str, files_dir, eclib_dir, ecwww_dir);
}

}


using base::android::JavaParamRef;

namespace content {

void BrowserStartupComplete(int result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowserStartupControllerImpl_browserStartupComplete(env, result);
}

void ServiceManagerStartupComplete() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowserStartupControllerImpl_serviceManagerStartupComplete(env);
}

bool ShouldStartGpuProcessOnBrowserStartup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_BrowserStartupControllerImpl_shouldStartGpuProcessOnBrowserStartup(
      env);
}

static void JNI_BrowserStartupControllerImpl_SetCommandLineFlags(
    JNIEnv* env,
    jboolean single_process) {
  SetContentCommandLineFlags(static_cast<bool>(single_process), "", "", "", "");
}

static jboolean JNI_BrowserStartupControllerImpl_IsOfficialBuild(JNIEnv* env) {
#if defined(OFFICIAL_BUILD)
  return true;
#else
  return false;
#endif
}

static void JNI_BrowserStartupControllerImpl_FlushStartupTasks(JNIEnv* env) {
  BrowserMainLoop::GetInstance()->SynchronouslyFlushStartupTasks();
}

}  // namespace content
