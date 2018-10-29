// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_controller_android.h"

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/common/resource_request_body_android.h"
#include "jni/NavigationControllerImpl_jni.h"
#include "net/base/data_url.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {

// static
static base::android::ScopedJavaLocalRef<jobject>
JNI_NavigationControllerImpl_CreateJavaNavigationEntry(
    JNIEnv* env,
    content::NavigationEntry* entry,
    int index) {
  DCHECK(entry);

  // Get the details of the current entry
  ScopedJavaLocalRef<jstring> j_url(
      ConvertUTF8ToJavaString(env, entry->GetURL().spec()));
  ScopedJavaLocalRef<jstring> j_virtual_url(
      ConvertUTF8ToJavaString(env, entry->GetVirtualURL().spec()));
  ScopedJavaLocalRef<jstring> j_original_url(
      ConvertUTF8ToJavaString(env, entry->GetOriginalRequestURL().spec()));
  ScopedJavaLocalRef<jstring> j_title(
      ConvertUTF16ToJavaString(env, entry->GetTitle()));
  ScopedJavaLocalRef<jstring> j_referrer_url(
      ConvertUTF8ToJavaString(env, entry->GetReferrer().url.spec()));
  ScopedJavaLocalRef<jobject> j_bitmap;
  const content::FaviconStatus& status = entry->GetFavicon();
  if (status.valid && status.image.ToSkBitmap()->computeByteSize() > 0)
    j_bitmap = gfx::ConvertToJavaBitmap(status.image.ToSkBitmap());

  return content::Java_NavigationControllerImpl_createNavigationEntry(
      env, index, j_url, j_virtual_url, j_original_url, j_referrer_url, j_title,
      j_bitmap, entry->GetTransitionType());
}

static void JNI_NavigationControllerImpl_AddNavigationEntryToHistory(
    JNIEnv* env,
    const JavaRef<jobject>& history,
    content::NavigationEntry* entry,
    int index) {
  content::Java_NavigationControllerImpl_addToNavigationHistory(
      env, history,
      JNI_NavigationControllerImpl_CreateJavaNavigationEntry(env, entry,
                                                             index));
}

}  // namespace

namespace content {

NavigationControllerAndroid::NavigationControllerAndroid(
    NavigationControllerImpl* navigation_controller)
    : navigation_controller_(navigation_controller) {
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(env,
             Java_NavigationControllerImpl_create(
                 env, reinterpret_cast<intptr_t>(this)).obj());
}

NavigationControllerAndroid::~NavigationControllerAndroid() {
  Java_NavigationControllerImpl_destroy(AttachCurrentThread(), obj_);
}

base::android::ScopedJavaLocalRef<jobject>
NavigationControllerAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

jboolean NavigationControllerAndroid::CanGoBack(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return navigation_controller_->CanGoBack();
}

jboolean NavigationControllerAndroid::CanGoForward(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return navigation_controller_->CanGoForward();
}

jboolean NavigationControllerAndroid::CanGoToOffset(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint offset) {
  return navigation_controller_->CanGoToOffset(offset);
}

void NavigationControllerAndroid::GoBack(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  navigation_controller_->GoBack();
}

void NavigationControllerAndroid::GoForward(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  navigation_controller_->GoForward();
}

void NavigationControllerAndroid::GoToOffset(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             jint offset) {
  navigation_controller_->GoToOffset(offset);
}

jboolean NavigationControllerAndroid::IsInitialNavigation(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return navigation_controller_->IsInitialNavigation();
}

void NavigationControllerAndroid::LoadIfNecessary(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  navigation_controller_->LoadIfNecessary();
}

void NavigationControllerAndroid::ContinuePendingReload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  navigation_controller_->ContinuePendingReload();
}

void NavigationControllerAndroid::Reload(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         jboolean check_for_repost) {
  navigation_controller_->Reload(ReloadType::NORMAL, check_for_repost);
}

void NavigationControllerAndroid::ReloadBypassingCache(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean check_for_repost) {
  navigation_controller_->Reload(ReloadType::BYPASSING_CACHE, check_for_repost);
}

jboolean NavigationControllerAndroid::NeedsReload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return navigation_controller_->NeedsReload();
}

void NavigationControllerAndroid::SetNeedsReload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  navigation_controller_->SetNeedsReload();
}

void NavigationControllerAndroid::CancelPendingReload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  navigation_controller_->CancelPendingReload();
}

void NavigationControllerAndroid::GoToNavigationIndex(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint index) {
  navigation_controller_->GoToIndex(index);
}

void NavigationControllerAndroid::LoadUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url,
    jint load_url_type,
    jint transition_type,
    const JavaParamRef<jstring>& j_referrer_url,
    jint referrer_policy,
    jint ua_override_option,
    const JavaParamRef<jstring>& extra_headers,
    const JavaParamRef<jobject>& j_post_data,
    const JavaParamRef<jstring>& base_url_for_data_url,
    const JavaParamRef<jstring>& virtual_url_for_data_url,
    const JavaParamRef<jstring>& data_url_as_string,
    jboolean can_load_local_resources,
    jboolean is_renderer_initiated,
    jboolean should_replace_current_entry) {
  DCHECK(url);
  NavigationController::LoadURLParams params(
      GURL(ConvertJavaStringToUTF8(env, url)));

  params.load_type =
      static_cast<NavigationController::LoadURLType>(load_url_type);
  params.transition_type = ui::PageTransitionFromInt(transition_type);
  params.override_user_agent =
      static_cast<NavigationController::UserAgentOverrideOption>(
          ua_override_option);
  params.can_load_local_resources = can_load_local_resources;
  params.is_renderer_initiated = is_renderer_initiated;
  params.should_replace_current_entry = should_replace_current_entry;

  if (extra_headers)
    params.extra_headers = ConvertJavaStringToUTF8(env, extra_headers);

  params.post_data = ExtractResourceRequestBodyFromJavaObject(env, j_post_data);

  if (base_url_for_data_url) {
    params.base_url_for_data_url =
        GURL(ConvertJavaStringToUTF8(env, base_url_for_data_url));
  }

  if (virtual_url_for_data_url) {
    params.virtual_url_for_data_url =
        GURL(ConvertJavaStringToUTF8(env, virtual_url_for_data_url));
  }

  if (data_url_as_string) {
    // Treat |data_url_as_string| as if we were intending to put it into a GURL
    // field. Note that kMaxURLChars is only enforced when serializing URLs
    // for IPC.
    GURL data_url = GURL(ConvertJavaStringToUTF8(env, data_url_as_string));
    DCHECK(data_url.SchemeIs(url::kDataScheme));
    DCHECK(params.url.SchemeIs(url::kDataScheme));
#if DCHECK_IS_ON()
    {
      std::string mime_type, charset, data;
      DCHECK(net::DataURL::Parse(params.url, &mime_type, &charset, &data));
      DCHECK(data.empty());
    }
#endif
    std::string s = data_url.spec();
    params.data_url_as_string = base::RefCountedString::TakeString(&s);
  }

  if (j_referrer_url) {
    params.referrer = content::Referrer(
        GURL(ConvertJavaStringToUTF8(env, j_referrer_url)),
        static_cast<network::mojom::ReferrerPolicy>(referrer_policy));
  }

  navigation_controller_->LoadURLWithParams(params);
}

void NavigationControllerAndroid::ClearHistory(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  // TODO(creis): Do callers of this need to know if it fails?
  if (navigation_controller_->CanPruneAllButLastCommitted())
    navigation_controller_->PruneAllButLastCommitted();
}

jint NavigationControllerAndroid::GetNavigationHistory(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& history) {
  // Iterate through navigation entries to populate the list
  int count = navigation_controller_->GetEntryCount();
  for (int i = 0; i < count; ++i) {
    JNI_NavigationControllerImpl_AddNavigationEntryToHistory(
        env, history, navigation_controller_->GetEntryAtIndex(i), i);
  }

  return navigation_controller_->GetCurrentEntryIndex();
}

void NavigationControllerAndroid::GetDirectedNavigationHistory(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& history,
    jboolean is_forward,
    jint max_entries) {
  // Iterate through navigation entries to populate the list
  int count = navigation_controller_->GetEntryCount();
  int num_added = 0;
  int increment_value = is_forward ? 1 : -1;
  for (int i = navigation_controller_->GetCurrentEntryIndex() + increment_value;
       i >= 0 && i < count;
       i += increment_value) {
    if (num_added >= max_entries)
      break;

    JNI_NavigationControllerImpl_AddNavigationEntryToHistory(
        env, history, navigation_controller_->GetEntryAtIndex(i), i);
    num_added++;
  }
}

ScopedJavaLocalRef<jstring>
NavigationControllerAndroid::GetOriginalUrlForVisibleNavigationEntry(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  NavigationEntry* entry = navigation_controller_->GetVisibleEntry();
  if (entry == NULL)
    return ScopedJavaLocalRef<jstring>(env, NULL);
  return ConvertUTF8ToJavaString(env, entry->GetOriginalRequestURL().spec());
}

void NavigationControllerAndroid::ClearSslPreferences(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  content::SSLHostStateDelegate* delegate =
      navigation_controller_->GetBrowserContext()->GetSSLHostStateDelegate();
  if (delegate)
    delegate->Clear(base::Callback<bool(const std::string&)>());
}

bool NavigationControllerAndroid::GetUseDesktopUserAgent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  NavigationEntry* entry = navigation_controller_->GetVisibleEntry();
  return entry && entry->GetIsOverridingUserAgent();
}

void NavigationControllerAndroid::SetUseDesktopUserAgent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled,
    jboolean reload_on_state_change) {
  if (GetUseDesktopUserAgent(env, obj) == enabled)
    return;

  // Make sure the navigation entry actually exists.
  NavigationEntry* entry = navigation_controller_->GetVisibleEntry();
  if (!entry)
    return;

  // Set the flag in the NavigationEntry.
  entry->SetIsOverridingUserAgent(enabled);
  navigation_controller_->delegate()->UpdateOverridingUserAgent();

  // Send the override to the renderer.
  if (reload_on_state_change) {
    // Reloading the page will send the override down as part of the
    // navigation IPC message.
    navigation_controller_->Reload(ReloadType::ORIGINAL_REQUEST_URL, true);
  }
}

base::android::ScopedJavaLocalRef<jobject>
NavigationControllerAndroid::GetEntryAtIndex(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             int index) {
  if (index < 0 || index >= navigation_controller_->GetEntryCount())
    return base::android::ScopedJavaLocalRef<jobject>();

  content::NavigationEntry* entry =
      navigation_controller_->GetEntryAtIndex(index);
  return JNI_NavigationControllerImpl_CreateJavaNavigationEntry(env, entry,
                                                                index);
}

base::android::ScopedJavaLocalRef<jobject>
NavigationControllerAndroid::GetPendingEntry(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  content::NavigationEntry* entry = navigation_controller_->GetPendingEntry();

  if (!entry)
    return base::android::ScopedJavaLocalRef<jobject>();

  return JNI_NavigationControllerImpl_CreateJavaNavigationEntry(
      env, entry, navigation_controller_->GetPendingEntryIndex());
}

jint NavigationControllerAndroid::GetLastCommittedEntryIndex(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return navigation_controller_->GetLastCommittedEntryIndex();
}

jboolean NavigationControllerAndroid::RemoveEntryAtIndex(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint index) {
  return navigation_controller_->RemoveEntryAtIndex(index);
}

jboolean NavigationControllerAndroid::CanCopyStateOver(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return navigation_controller_->GetEntryCount() == 0 &&
      !navigation_controller_->GetPendingEntry();
}

jboolean NavigationControllerAndroid::CanPruneAllButLastCommitted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return navigation_controller_->CanPruneAllButLastCommitted();
}

void NavigationControllerAndroid::CopyStateFrom(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong source_navigation_controller_android,
    jboolean needs_reload) {
  navigation_controller_->CopyStateFrom(
      *(reinterpret_cast<NavigationControllerAndroid*>(
            source_navigation_controller_android)
            ->navigation_controller_),
      needs_reload);
}

void NavigationControllerAndroid::CopyStateFromAndPrune(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong source_navigation_controller_android,
    jboolean replace_entry) {
  navigation_controller_->CopyStateFromAndPrune(
      reinterpret_cast<NavigationControllerAndroid*>(
          source_navigation_controller_android)->navigation_controller_,
      replace_entry);
}

ScopedJavaLocalRef<jstring> NavigationControllerAndroid::GetEntryExtraData(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint index,
    const JavaParamRef<jstring>& jkey) {
  if (index < 0 || index >= navigation_controller_->GetEntryCount())
    return ScopedJavaLocalRef<jstring>();

  std::string key = base::android::ConvertJavaStringToUTF8(env, jkey);
  base::string16 value;
  navigation_controller_->GetEntryAtIndex(index)->GetExtraData(key, &value);
  return ConvertUTF16ToJavaString(env, value);
}

void NavigationControllerAndroid::SetEntryExtraData(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint index,
    const JavaParamRef<jstring>& jkey,
    const JavaParamRef<jstring>& jvalue) {
  if (index < 0 || index >= navigation_controller_->GetEntryCount())
    return;

  std::string key = base::android::ConvertJavaStringToUTF8(env, jkey);
  base::string16 value = base::android::ConvertJavaStringToUTF16(env, jvalue);
  navigation_controller_->GetEntryAtIndex(index)->SetExtraData(key, value);
}

}  // namespace content
