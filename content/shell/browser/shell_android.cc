// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/file_chooser_file_info.h"
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

#include "chrome/browser/file_select_helper.h"

#include "content/public/common/file_chooser_file_info.h"

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
  //NOTIMPLEMENTED() << ": " << title;
}

void Shell::LoadProgressChanged(WebContents* source, double progress) {
  JNIEnv* env = AttachCurrentThread();
  Java_Shell_onLoadProgressChanged(env, java_object_, progress);
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

content::RenderFrameHost* filehost;
FileChooserParams::Mode filemode;
std::unique_ptr<FileSelectListener> *_listener;

void Shell::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
	std::unique_ptr<FileSelectListener> listener,
    const FileChooserParams& params) {
		
		JNIEnv* env = AttachCurrentThread();
		if (! env) {
			return;
		}
		
		//render_frame_host->AddRef();
		
		filehost = render_frame_host;
		filemode = params.mode;
		_listener = &listener; 
		
		// Construct a String
#ifdef __aarch64__		
		jclass clazz = env->FindClass("com/byte4byte/cloudretro64/ContentShellActivity");
#else
		jclass clazz = env->FindClass("com/byte4byte/cloudretro32/ContentShellActivity");
#endif
		// Get the method that you want to call
		jmethodID rfc = env->GetStaticMethodID(clazz, "runFileChooser", "(I)V");
		// Call the method on the object
		env->CallStaticVoidMethod(clazz, rfc, (ui::SelectFileDialog::Type)params.mode == ui::SelectFileDialog::SELECT_UPLOAD_FOLDER ? 1 : 0);
		
		
	}


}  // namespace content

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
void listdir(const char *name, std::vector<FileChooserFileInfoPtr> &files)
{
    DIR *dir;
    struct dirent *entry;
	char path[1024];

    if (!(dir = opendir(name)))
        return;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
            //printf("%*s[%s]\n", indent, "", entry->d_name);
            listdir(path, files);
        } else {
			snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
			base::FilePath fpath(path);	
			/*FileChooserFileInfo file_info;
			file_info.file_path = fpath;
			//file_info.display_name = rawString;
			files.push_back(file_info);*/
			
			const size_t cSize = strlen(path)+1;
			std::wstring wc( cSize, L'#' );
			mbstowcs( &wc[0], path, cSize );
			
			base::string16 ps = (const unsigned short *)wc.c_str();			
			
			files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
					blink::mojom::NativeFileInfo::New(base::FilePath(path), ps)));
			
            //printf("%*s- %s\n", indent, "", entry->d_name);
        }
    }
    closedir(dir);
}

#ifdef __aarch64__
	#define fileChooserResultsName Java_com_byte4byte_cloudretro64_ContentShellActivity_fileChooserResults
#else
	#define fileChooserResultsName Java_com_byte4byte_cloudretro32_ContentShellActivity_fileChooserResults
#endif

extern "C" {
JNIEXPORT void JNICALL fileChooserResultsName(
    JNIEnv* env,
    jclass,
    jobjectArray stringArray) {
		
		
	int stringCount = env->GetArrayLength(stringArray);
	
	std::vector<FileChooserFileInfoPtr> files;
	
	char path[2048] = {0};

    for (int i=0; i<stringCount; i++) {
        jstring string = (jstring) (env->GetObjectArrayElement(stringArray, i));
        const char *rawString = env->GetStringUTFChars(string, 0);
		
		
		if (content::filemode == (FileChooserParams::Mode)ui::SelectFileDialog::SELECT_UPLOAD_FOLDER) {
			//char buff[2048];
			strcpy(path, rawString);
			strcat(path, "/ ");
			//base::FilePath path(buff);
			
			//FileChooserFileInfo file_info;
			//file_info.file_path = path;
			//file_info.is_directory = true;
			//files.push_back(file_info);
			listdir(rawString, files);
		}
		else {
			/*base::FilePath path(rawString);
			
			FileChooserFileInfo file_info;
			file_info.file_path = path;
			files.push_back(file_info);*/
			
			if (! path[0]) {
				const char *ptr = strrchr(rawString, '/');
				if (ptr) {
					strncpy(path, rawString, ptr-rawString);
				}
			}
			
			/*const size_t cSize = strlen(rawString)+1;
			std::wstring wc( cSize, L'#' );
			mbstowcs( &wc[0], rawString, cSize );*/
			
			const size_t cSize = strlen(rawString)+1;
			std::wstring wc( cSize, L'#' );
			mbstowcs( &wc[0], rawString, cSize );
			
			base::string16 ps = (const unsigned short *)wc.c_str();	
			
			files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
					blink::mojom::NativeFileInfo::New(base::FilePath(rawString), ps)));
			
		}
		
		
		env->ReleaseStringUTFChars(string, rawString);
    }
	
	{
		//const size_t cSize = strlen(path)+1;
		//std::wstring wcpath( cSize, L'#' );
		//mbstowcs( &wcpath[0], path, cSize );
		
		//content::filehost->FilesSelectedInChooser(files, content::filemode);
		base::FilePath base_dir(path);
		(*content::_listener)->FileSelected(std::move(files), base_dir, (ui::SelectFileDialog::Type)content::filemode == ui::SelectFileDialog::SELECT_UPLOAD_FOLDER ? FileChooserParams::Mode::kUploadFolder : FileChooserParams::Mode::kOpenMultiple);

	}

}
}