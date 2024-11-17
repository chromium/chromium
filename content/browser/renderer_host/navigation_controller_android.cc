// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_controller_android.h"

#include <stdint.h>

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/flat_map.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/android/additional_navigation_params_utils.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_request_body_android.h"
#include "content/public/common/url_constants.h"
#include "net/base/data_url.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/NavigationControllerImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {

const char kMapDataKey[] = "map_data_key";

// static
static base::android::ScopedJavaLocalRef<jobject>
JNI_NavigationControllerImpl_CreateJavaNavigationEntry(
    JNIEnv* env,
    content::NavigationEntry* entry,
    int index) {
  DCHECK(entry);

  // Get the details of the current entry
  ScopedJavaLocalRef<jobject> j_url(
      url::GURLAndroid::FromNativeGURL(env, entry->GetURL()));
  ScopedJavaLocalRef<jobject> j_virtual_url(
      url::GURLAndroid::FromNativeGURL(env, entry->GetVirtualURL()));
  ScopedJavaLocalRef<jobject> j_original_url(
      url::GURLAndroid::FromNativeGURL(env, entry->GetOriginalRequestURL()));
  ScopedJavaLocalRef<jstring> j_title(
      ConvertUTF16ToJavaString(env, entry->GetTitle()));
  ScopedJavaLocalRef<jobject> j_bitmap;
  const content::FaviconStatus& status = entry->GetFavicon();
  if (status.valid && status.image.ToSkBitmap()->computeByteSize() > 0) {
    j_bitmap = gfx::ConvertToJavaBitmap(*status.image.ToSkBitmap(),
                                        gfx::OomBehavior::kReturnNullOnOom);
  }
  jlong j_timestamp = entry->GetTimestamp().InMillisecondsSinceUnixEpoch();

  return content::Java_NavigationControllerImpl_createNavigationEntry(
      env, index, j_url, j_virtual_url, j_original_url, j_title, j_bitmap,
      entry->GetTransitionType(), j_timestamp, entry->IsInitialEntry());
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

class MapData : public base::SupportsUserData::Data {
 public:
  MapData() = default;

  MapData(const MapData&) = delete;
  MapData& operator=(const MapData&) = delete;

  ~MapData() override = default;

  static MapData* Get(content::NavigationEntry* entry) {
    MapData* map_data = static_cast<MapData*>(entry->GetUserData(kMapDataKey));
    if (map_data)
      return map_data;
    auto map_data_ptr = std::make_unique<MapData>();
    map_data = map_data_ptr.get();
    entry->SetUserData(kMapDataKey, std::move(map_data_ptr));
    return map_data;
  }

  base::flat_map<std::string, std::u16string>& map() { return map_; }

  // base::SupportsUserData::Data:
  std::unique_ptr<Data> Clone() override {
    std::unique_ptr<MapData> clone = std::make_unique<MapData>();
    clone->map_ = map_;
    return clone;
  }

 private:
  base::flat_map<std::string, std::u16string> map_;
};

}  // namespace

namespace content {

NavigationControllerAndroid::NavigationControllerAndroid(
    NavigationControllerImpl* navigation_controller)
    : navigation_controller_(navigation_controller) {
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(env, Java_NavigationControllerImpl_create(
                      env, reinterpret_cast<intptr_t>(this))
                      .obj());
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
  return navigation_controller_->CanGoToOffsetWithSkipping(offset);
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
  navigation_controller_->GoToOffsetWithSkipping(offset);
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
  SCOPED_CRASH_KEY_BOOL("nav_reentrancy_caller2", "Reload_check",
                        (bool)check_for_repost);
  navigation_controller_->Reload(ReloadType::NORMAL, check_for_repost);
}

void NavigationControllerAndroid::ReloadBypassingCache(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean check_for_repost) {
  SCOPED_CRASH_KEY_BOOL("nav_reentrancy_caller2", "ReloadB_check",
                        (bool)check_for_repost);
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

base::android::ScopedJavaLocalRef<jobject> NavigationControllerAndroid::LoadUrl(
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
    const JavaParamRef<jstring>& virtual_url_for_special_cases,
    const JavaParamRef<jstring>& data_url_as_string,
    jboolean can_load_local_resources,
    jboolean is_renderer_initiated,
    jboolean should_replace_current_entry,
    const JavaParamRef<jobject>& j_initiator_origin,
    jboolean has_user_gesture,
    jboolean should_clear_history_list,
    const base::android::JavaParamRef<jobject>& j_additional_navigation_params,
    jlong input_start,
    jlong navigation_ui_data_ptr,
    jboolean is_pdf) {
  DCHECK(url);
  NavigationController::LoadURLParams params(
      GURL(ConvertJavaStringToUTF8(env, url)));
  // Wrap the raw pointer in case on an early return.
  std::unique_ptr<NavigationUIData> navigation_ui_data = base::WrapUnique(
      reinterpret_cast<NavigationUIData*>(navigation_ui_data_ptr));
  params.load_type =
      static_cast<NavigationController::LoadURLType>(load_url_type);
  params.transition_type = ui::PageTransitionFromInt(transition_type);
  params.override_user_agent =
      static_cast<NavigationController::UserAgentOverrideOption>(
          ua_override_option);
  params.can_load_local_resources = can_load_local_resources;
  params.is_renderer_initiated = is_renderer_initiated;
  params.should_replace_current_entry = should_replace_current_entry;
  params.has_user_gesture = has_user_gesture;
  params.should_clear_history_list = should_clear_history_list;
  params.is_pdf = is_pdf;

  if (j_additional_navigation_params) {
    params.initiator_frame_token =
        GetInitiatorFrameTokenFromJavaAdditionalNavigationParams(
            env, j_additional_navigation_params);
    params.initiator_process_id =
        GetInitiatorProcessIdFromJavaAdditionalNavigationParams(
            env, j_additional_navigation_params);

    // If the attribution src token exists, then an impression exists with this
    // navigation.
    if (GetAttributionSrcTokenFromJavaAdditionalNavigationParams(
            env, j_additional_navigation_params)
            .has_value()) {
      blink::Impression impression;
      impression.attribution_src_token =
          GetAttributionSrcTokenFromJavaAdditionalNavigationParams(
              env, j_additional_navigation_params)
              .value();
      params.impression = impression;
    }
  }

  if (extra_headers)
    params.extra_headers = ConvertJavaStringToUTF8(env, extra_headers);

  params.post_data = ExtractResourceRequestBodyFromJavaObject(env, j_post_data);

  if (base_url_for_data_url) {
    params.base_url_for_data_url =
        GURL(ConvertJavaStringToUTF8(env, base_url_for_data_url));
  }

  if (virtual_url_for_special_cases) {
    params.virtual_url_for_special_cases =
        GURL(ConvertJavaStringToUTF8(env, virtual_url_for_special_cases));
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
    params.data_url_as_string =
        base::MakeRefCounted<base::RefCountedString>(std::move(s));
  }

  if (j_referrer_url) {
    params.referrer =
        Referrer(GURL(ConvertJavaStringToUTF8(env, j_referrer_url)),
                 Referrer::ConvertToPolicy(referrer_policy));
  }

  if (j_initiator_origin) {
    params.initiator_origin =
        url::Origin::FromJavaObject(env, j_initiator_origin);
  }

  if (input_start != 0)
    params.input_start = base::TimeTicks::FromUptimeMillis(input_start);

  params.navigation_ui_data = std::move(navigation_ui_data);

  base::WeakPtr<NavigationHandle> handle =
      navigation_controller_->LoadURLWithParams(params);

  if (!handle) {
    return nullptr;
  }

  return base::android::ScopedJavaLocalRef<jobject>(
      handle->GetJavaNavigationHandle());
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
       i >= 0 && i < count; i += increment_value) {
    if (num_added >= max_entries)
      break;

    JNI_NavigationControllerImpl_AddNavigationEntryToHistory(
        env, history, navigation_controller_->GetEntryAtIndex(i), i);
    num_added++;
  }
}

void NavigationControllerAndroid::ClearSslPreferences(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  SSLHostStateDelegate* delegate =
      navigation_controller_->GetBrowserContext()->GetSSLHostStateDelegate();
  if (delegate)
    delegate->Clear(base::NullCallback());
}

bool NavigationControllerAndroid::GetUseDesktopUserAgent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  NavigationEntry* entry = navigation_controller_->GetLastCommittedEntry();
  return entry && entry->GetIsOverridingUserAgent();
}

void NavigationControllerAndroid::SetUseDesktopUserAgent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled,
    jboolean reload_on_state_change,
    jint source) {
  SCOPED_CRASH_KEY_BOOL("nav_reentrancy_caller2", "SetUA_enabled",
                        (bool)enabled);
  if (GetUseDesktopUserAgent(env, obj) == enabled)
    return;

  if (navigation_controller_->in_navigate_to_pending_entry() &&
      reload_on_state_change) {
    // Sometimes it's possible to call this function in response to a
    // navigation to a pending entry. In this case, we should avoid triggering
    // another navigation synchronously, as it will crash due to navigation
    // re-entrancy checks. To do that, post a task to update the UA and
    // reload asynchronously.
    // TODO(crbug.com/40841494): Figure out the case that leads to this
    // situation and avoid calling this function entirely in that case. For now,
    // do a do a DumpWithoutCrashing so that we can investigate.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NavigationControllerAndroid::SetUseDesktopUserAgentInternal,
            weak_factory_.GetWeakPtr(), enabled, reload_on_state_change));
    LOG(WARNING) << "NavigationControllerAndroid::SetUseDesktopUserAgent "
                 << "triggers re-entrant navigation, override: "
                 << (bool)enabled << ", source: " << (int)source;
    SCOPED_CRASH_KEY_NUMBER("SetUseDesktopUserAgent", "caller", (int)source);
    base::debug::DumpWithoutCrashing();
  } else {
    SetUseDesktopUserAgentInternal(enabled, reload_on_state_change);
  }
}

void NavigationControllerAndroid::SetUseDesktopUserAgentInternal(
    bool enabled,
    bool reload_on_state_change) {
  // Make sure the navigation entry actually exists.
  NavigationEntry* entry = navigation_controller_->GetLastCommittedEntry();
  // TODO(crbug.com/40063008): Early return for initial NavigationEntries as a
  // workaround. Currently, doing a reload while on the initial NavigationEntry
  // might result in committing an unrelated pending NavigationEntry and
  // mistakenly marking that entry as an initial NavigationEntry. That will
  // cause problems, such as the URL bar showing about:blank instead of the URL
  // of the NavigationEntry. To prevent that happening in this case, skip
  // reloading initial NavigationEntries entirely. This is a short-term fix,
  // while we work on a long-term fix to no longer mistakenly mark the unrelated
  // pending NavigationEntry as the initial NavigationEntry.
  if (!entry || entry->IsInitialEntry()) {
    return;
  }

  // Set the flag in the NavigationEntry.
  entry->SetIsOverridingUserAgent(enabled);
  navigation_controller_->delegate()->UpdateOverridingUserAgent();

  // Send the override to the renderer.
  if (reload_on_state_change) {
    // Reloading the page will send the override down as part of the
    // navigation IPC message.
    navigation_controller_->LoadOriginalRequestURL();
  }
}

base::android::ScopedJavaLocalRef<jobject>
NavigationControllerAndroid::GetEntryAtIndex(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             int index) {
  if (index < 0 || index >= navigation_controller_->GetEntryCount())
    return base::android::ScopedJavaLocalRef<jobject>();

  NavigationEntry* entry = navigation_controller_->GetEntryAtIndex(index);
  return JNI_NavigationControllerImpl_CreateJavaNavigationEntry(env, entry,
                                                                index);
}

base::android::ScopedJavaLocalRef<jobject>
NavigationControllerAndroid::GetVisibleEntry(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  NavigationEntry* entry = navigation_controller_->GetVisibleEntry();

  if (!entry)
    return base::android::ScopedJavaLocalRef<jobject>();

  return JNI_NavigationControllerImpl_CreateJavaNavigationEntry(env, entry,
                                                                /*index=*/-1);
}

base::android::ScopedJavaLocalRef<jobject>
NavigationControllerAndroid::GetPendingEntry(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  NavigationEntry* entry = navigation_controller_->GetPendingEntry();

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

void NavigationControllerAndroid::PruneForwardEntries(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return navigation_controller_->PruneForwardEntries();
}

ScopedJavaLocalRef<jstring> NavigationControllerAndroid::GetEntryExtraData(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint index,
    const JavaParamRef<jstring>& jkey) {
  if (index < 0 || index >= navigation_controller_->GetEntryCount())
    return ScopedJavaLocalRef<jstring>();

  std::string key = base::android::ConvertJavaStringToUTF8(env, jkey);
  MapData* map_data =
      MapData::Get(navigation_controller_->GetEntryAtIndex(index));
  auto iter = map_data->map().find(key);
  return ConvertUTF16ToJavaString(
      env, iter == map_data->map().end() ? std::u16string() : iter->second);
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
  std::u16string value = base::android::ConvertJavaStringToUTF16(env, jvalue);
  MapData* map_data =
      MapData::Get(navigation_controller_->GetEntryAtIndex(index));
  map_data->map()[key] = value;
}

}  // namespace content
