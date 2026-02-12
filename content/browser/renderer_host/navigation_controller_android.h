// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
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

  bool CanGoBack(JNIEnv* env);
  bool CanGoForward(JNIEnv* env);
  bool CanGoToOffset(JNIEnv* env, int32_t offset);
  void GoBack(JNIEnv* env);
  void GoForward(JNIEnv* env);
  void GoToOffset(JNIEnv* env, int32_t offset);
  bool IsInitialNavigation(JNIEnv* env);
  void LoadIfNecessary(JNIEnv* env);
  void ContinuePendingReload(JNIEnv* env);
  void Reload(JNIEnv* env, bool check_for_repost);
  void ReloadBypassingCache(JNIEnv* env, bool check_for_repost);
  bool NeedsReload(JNIEnv* env);
  void SetNeedsReload(JNIEnv* env);
  void CancelPendingReload(JNIEnv* env);
  void GoToNavigationIndex(JNIEnv* env, int32_t index);
  base::android::ScopedJavaLocalRef<jobject> LoadUrl(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& url,
      int32_t load_url_type,
      int32_t transition_type,
      const base::android::JavaRef<jstring>& j_referrer_url,
      int32_t referrer_policy,
      int32_t ua_override_option,
      const base::android::JavaRef<jstring>& extra_headers,
      const base::android::JavaRef<jobject>& j_post_data,
      const base::android::JavaRef<jstring>& base_url_for_data_url,
      const base::android::JavaRef<jstring>& virtual_url_for_special_cases,
      const base::android::JavaRef<jstring>& data_url_as_string,
      bool can_load_local_resources,
      bool is_renderer_initiated,
      bool should_replace_current_entry,
      const base::android::JavaRef<jobject>& j_initiator_origin,
      bool has_user_gesture,
      bool should_clear_history_list,
      const base::android::JavaRef<jobject>& j_additional_navigation_params,
      int64_t input_start,
      int64_t navigation_ui_data_ptr,
      bool is_pdf,
      bool remove_extra_headers_on_cross_origin_redirect);
  void ClearSslPreferences(JNIEnv* env);
  bool GetUseDesktopUserAgent(JNIEnv* env);
  void SetUseDesktopUserAgent(JNIEnv* env,
                              bool state,
                              bool reload_on_state_change,
                              bool skip_on_initial_navigation);
  base::android::ScopedJavaLocalRef<jobject> GetEntryAtIndex(JNIEnv* env,
                                                             int index);
  base::android::ScopedJavaLocalRef<jobject> GetVisibleEntry(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetPendingEntry(JNIEnv* env);
  int GetNavigationHistory(JNIEnv* env,
                           const base::android::JavaRef<jobject>& history);
  void GetDirectedNavigationHistory(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& history,
      bool is_forward,
      int32_t max_entries);
  void ClearHistory(JNIEnv* env);
  int GetLastCommittedEntryIndex(JNIEnv* env);
  bool CanViewSource(JNIEnv* env);
  bool RemoveEntryAtIndex(JNIEnv* env, int32_t index);
  void PruneForwardEntries(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> GetEntryExtraData(
      JNIEnv* env,
      int32_t index,
      const base::android::JavaRef<jstring>& jkey);
  void SetEntryExtraData(JNIEnv* env,
                         int32_t index,
                         const base::android::JavaRef<jstring>& jkey,
                         const base::android::JavaRef<jstring>& jvalue);
  void CopyStateFrom(JNIEnv* env,
                     int64_t source_navigation_controller_ptr,
                     bool needs_reload);

 private:
  void SetUseDesktopUserAgentInternal(bool enabled,
                                      bool reload_on_state_change,
                                      bool skip_on_initial_navigation);

  raw_ptr<NavigationControllerImpl> navigation_controller_;
  // A weak reference to the Java object. The Java object is kept alive by a
  // static map in the Java code. ScopedJavaGlobalRef would scale poorly with a
  // large number of WebContents as it consumes an entry in the finite global
  // ref table.
  JavaObjectWeakGlobalRef obj_;
  base::WeakPtrFactory<NavigationControllerAndroid> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_ANDROID_H_
