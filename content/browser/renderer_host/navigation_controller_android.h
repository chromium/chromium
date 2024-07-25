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

  jboolean CanGoBack(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  jboolean CanGoForward(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  jboolean CanGoToOffset(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint offset);
  void GoBack(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void GoForward(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void GoToOffset(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jint offset);
  jboolean IsInitialNavigation(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  void LoadIfNecessary(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);
  void ContinuePendingReload(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  void Reload(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              jboolean check_for_repost);
  void ReloadBypassingCache(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            jboolean check_for_repost);
  jboolean NeedsReload(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);
  void SetNeedsReload(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void CancelPendingReload(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);
  void GoToNavigationIndex(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jint index);
  base::android::ScopedJavaLocalRef<jobject> LoadUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
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
  void ClearSslPreferences(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& /* obj */);
  bool GetUseDesktopUserAgent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& /* obj */);
  void SetUseDesktopUserAgent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& /* obj */,
      jboolean state,
      jboolean reload_on_state_change,
      jint source);
  base::android::ScopedJavaLocalRef<jobject> GetEntryAtIndex(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      int index);
  base::android::ScopedJavaLocalRef<jobject> GetVisibleEntry(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& /* obj */);
  base::android::ScopedJavaLocalRef<jobject> GetPendingEntry(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& /* obj */);
  int GetNavigationHistory(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           const base::android::JavaParamRef<jobject>& history);
  void GetDirectedNavigationHistory(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& history,
      jboolean is_forward,
      jint max_entries);
  void ClearHistory(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
  int GetLastCommittedEntryIndex(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean RemoveEntryAtIndex(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj,
                              jint index);
  void PruneForwardEntries(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetEntryExtraData(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint index,
      const base::android::JavaParamRef<jstring>& jkey);
  void SetEntryExtraData(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint index,
                         const base::android::JavaParamRef<jstring>& jkey,
                         const base::android::JavaParamRef<jstring>& jvalue);

 private:
  void SetUseDesktopUserAgentInternal(bool enabled,
                                      bool reload_on_state_change);

  raw_ptr<NavigationControllerImpl> navigation_controller_;
  base::android::ScopedJavaGlobalRef<jobject> obj_;
  base::WeakPtrFactory<NavigationControllerAndroid> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_ANDROID_H_
