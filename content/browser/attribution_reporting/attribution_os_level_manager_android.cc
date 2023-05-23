// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_os_level_manager_android.h"

#include <jni.h>

#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/public/android/content_jni_headers/AttributionOsLevelManager_jni.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ApiState = ::content::AttributionOsLevelManager::ApiState;

int GetDeletionMode(bool delete_rate_limit_data) {
  // See
  // https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/measurement/DeletionRequest#constants
  // for constant values.
  static constexpr int kDeletionModeAll = 0;
  static constexpr int kDeletionModeExcludeInternalData = 1;

  return delete_rate_limit_data ? kDeletionModeAll
                                : kDeletionModeExcludeInternalData;
}

int GetMatchBehavior(BrowsingDataFilterBuilder::Mode mode) {
  // See
  // https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/measurement/DeletionRequest#constants
  // for constant values.
  static constexpr int kMatchBehaviorDelete = 0;
  static constexpr int kMatchBehaviorPreserve = 1;

  switch (mode) {
    case BrowsingDataFilterBuilder::Mode::kDelete:
      return kMatchBehaviorDelete;
    case BrowsingDataFilterBuilder::Mode::kPreserve:
      return kMatchBehaviorPreserve;
  }
}

ApiState ConvertToApiState(int value) {
  // See
  // https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/measurement/MeasurementManager
  // for constant values.
  static constexpr int kMeasurementApiStateDisabled = 0;
  static constexpr int kMeasurementApiStateEnabled = 1;

  switch (value) {
    case kMeasurementApiStateDisabled:
      return ApiState::kDisabled;
    case kMeasurementApiStateEnabled:
      return ApiState::kEnabled;
    default:
      return ApiState::kDisabled;
  }
}

}  // namespace

static void JNI_AttributionOsLevelManager_OnMeasurementStateReturned(
    JNIEnv* env,
    jint state) {
  AttributionOsLevelManager::SetApiState(ConvertToApiState(state));
}

AttributionOsLevelManagerAndroid::AttributionOsLevelManagerAndroid() {
  jobj_ = Java_AttributionOsLevelManager_Constructor(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));

  if (AttributionOsLevelManager::ShouldInitializeApiState()) {
    Java_AttributionOsLevelManager_getMeasurementApiStatus(
        base::android::AttachCurrentThread(), jobj_);
  }
}

AttributionOsLevelManagerAndroid::~AttributionOsLevelManagerAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Java_AttributionOsLevelManager_nativeDestroyed(
      base::android::AttachCurrentThread(), jobj_);
}

void AttributionOsLevelManagerAndroid::Register(
    const OsRegistration& registration,
    bool is_debug_key_allowed,
    base::OnceCallback<void(bool sucess)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  JNIEnv* env = base::android::AttachCurrentThread();

  int request_id = next_callback_id_++;
  pending_registration_callbacks_.emplace(request_id, std::move(callback));

  auto registration_url =
      url::GURLAndroid::FromNativeGURL(env, registration.registration_url);
  auto top_level_origin = url::GURLAndroid::FromNativeGURL(
      env, registration.top_level_origin.GetURL());

  switch (registration.GetType()) {
    case attribution_reporting::mojom::OsRegistrationType::kSource:
      DCHECK(registration.input_event.has_value());
      if (AttributionOsLevelManager::ShouldUseOsWebSource()) {
        Java_AttributionOsLevelManager_registerWebAttributionSource(
            env, jobj_, request_id, registration_url, top_level_origin,
            is_debug_key_allowed, registration.input_event->input_event);
      } else {
        Java_AttributionOsLevelManager_registerAttributionSource(
            env, jobj_, request_id, registration_url,
            registration.input_event->input_event);
      }
      break;
    case attribution_reporting::mojom::OsRegistrationType::kTrigger:
      Java_AttributionOsLevelManager_registerWebAttributionTrigger(
          env, jobj_, request_id, registration_url, top_level_origin,
          is_debug_key_allowed);
      break;
  }
}

void AttributionOsLevelManagerAndroid::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    const std::set<url::Origin>& origins,
    const std::set<std::string>& domains,
    BrowsingDataFilterBuilder::Mode mode,
    bool delete_rate_limit_data,
    base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  JNIEnv* env = base::android::AttachCurrentThread();

  std::vector<base::android::ScopedJavaLocalRef<jobject>> j_origins;
  base::ranges::transform(
      origins, std::back_inserter(j_origins), [env](const url::Origin& origin) {
        return url::GURLAndroid::FromNativeGURL(env, origin.GetURL());
      });

  int request_id = next_callback_id_++;
  pending_data_deletion_callbacks_.emplace(request_id, std::move(done));

  Java_AttributionOsLevelManager_deleteRegistrations(
      env, jobj_, request_id, delete_begin.ToJavaTime(),
      delete_end.ToJavaTime(),
      url::GURLAndroid::ToJavaArrayOfGURLs(env, j_origins),
      base::android::ToJavaArrayOfStrings(
          env, std::vector<std::string>(domains.begin(), domains.end())),
      GetDeletionMode(delete_rate_limit_data), GetMatchBehavior(mode));
}

void AttributionOsLevelManagerAndroid::OnRegistrationCompleted(JNIEnv* env,
                                                               jint request_id,
                                                               bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = pending_registration_callbacks_.find(request_id);
  if (it == pending_registration_callbacks_.end()) {
    return;
  }

  std::move(it->second).Run(success);
  pending_registration_callbacks_.erase(it);
}

void AttributionOsLevelManagerAndroid::OnDataDeletionCompleted(
    JNIEnv* env,
    jint request_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = pending_data_deletion_callbacks_.find(request_id);
  if (it == pending_data_deletion_callbacks_.end()) {
    return;
  }

  std::move(it->second).Run();
  pending_data_deletion_callbacks_.erase(it);
}

}  // namespace content
