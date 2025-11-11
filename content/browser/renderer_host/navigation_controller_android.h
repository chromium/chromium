// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

class NavigationControllerImpl;

// Android wrapper around NavigationController that provides safer passage
// from java and back to native and provides java with a means of communicating
// with its native counterpart.
class CONTENT_EXPORT NavigationControllerAndroid {
 public:
  explicit NavigationControllerAndroid(
      NavigationControllerImpl* navigation_controller);

  NavigationControllerAndroid(const NavigationControllerAndroid&) = delete;
  NavigationControllerAndroid& operator=(const NavigationControllerAndroid&) =
      delete;

  ~NavigationControllerAndroid();

  NavigationControllerImpl* navigation_controller() const {
    return navigation_controller_;
  }

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  jboolean CanGoBack(JNIEnv* env);
  jboolean CanGoForward(JNIEnv* env);
  jboolean CanGoToOffset(JNIEnv* env,
                         jint offset);
  void GoBack(JNIEnv* env);
  void GoForward(JNIEnv* env);
  void GoToOffset(JNIEnv* env,
                  jint offset);
  jboolean IsInitialNavigation(JNIEnv* env);
  void LoadIfNecessary(JNIEnv* env);
  void ContinuePendingReload(JNIEnv* env);
  void Reload(JNIEnv* env,
              jboolean check_for_repost);
  void ReloadBypassingCache(JNIEnv* env,
                            jboolean check_for_repost);
  jboolean NeedsReload(JNIEnv* env);
  void SetNeedsReload(JNIEnv* env);
  void CancelPendingReload(JNIEnv* env);
  void GoToNavigationIndex(JNIEnv* env,
                           jint index);
  base::android::ScopedJavaLocalRef<jobject> LoadUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& url,
      jint load_url_type,
      jint transition_type,
      const base::android::JavaParamRef<jstring>& j_referrer_url,
      jint referrer_policy,
      jint ua_override_option,
      const base::android::JavaParamRef<jstring>& extra_headers,
      const base::android::JavaParamRef<jobject>& j_post_data,
      const base::android::JavaParamRef<jstring>& base_url_for_data_url,
      const base::android::JavaParamRef<jstring>& virtual_url_for_special_cases,
      const base::android::JavaParamRef<jstring>& data_url_as_string,
      jboolean can_load_local_resources,
      jboolean is_renderer_initiated,
      jboolean should_replace_current_entry,
      const base::android::JavaParamRef<jobject>& j_initiator_origin,
      jboolean has_user_gesture,
      jboolean should_clear_history_list,
      const base::android::JavaParamRef<jobject>&
          j_additional_navigation_params,
      jlong input_start,
      jlong navigation_ui_data_ptr,
      jboolean is_pdf);
  void ClearSslPreferences(JNIEnv* env);
  bool GetUseDesktopUserAgent(JNIEnv* env);
  void SetUseDesktopUserAgent(JNIEnv* env,
                              jboolean state,
                              jboolean reload_on_state_change,
                              jboolean skip_on_initial_navigation);
  base::android::ScopedJavaLocalRef<jobject> GetEntryAtIndex(JNIEnv* env,
                                                             int index);
  base::android::ScopedJavaLocalRef<jobject> GetVisibleEntry(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetPendingEntry(JNIEnv* env);
  int GetNavigationHistory(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& history);
  void GetDirectedNavigationHistory(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& history,
      jboolean is_forward,
      jint max_entries);
  void ClearHistory(JNIEnv* env);
  int GetLastCommittedEntryIndex(JNIEnv* env);
  jboolean CanViewSource(JNIEnv* env);
  jboolean RemoveEntryAtIndex(JNIEnv* env,
                              jint index);
  void PruneForwardEntries(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> GetEntryExtraData(
      JNIEnv* env,
      jint index,
      const base::android::JavaParamRef<jstring>& jkey);
  void SetEntryExtraData(JNIEnv* env,
                         jint index,
                         const base::android::JavaParamRef<jstring>& jkey,
                         const base::android::JavaParamRef<jstring>& jvalue);
  void CopyStateFrom(JNIEnv* env,
                     jlong source_navigation_controller_ptr,
                     jboolean needs_reload);

 private:
  void SetUseDesktopUserAgentInternal(bool enabled,
                                      bool reload_on_state_change,
                                      bool skip_on_initial_navigation);

  raw_ptr<NavigationControllerImpl> navigation_controller_;
  base::android::ScopedJavaGlobalRef<jobject> obj_;
  base::WeakPtrFactory<NavigationControllerAndroid> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_ANDROID_H_
