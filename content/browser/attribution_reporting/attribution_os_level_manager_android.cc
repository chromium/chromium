// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_os_level_manager_android.h"

#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/atomic_sequence_num.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/attribution_reporting/os_support.mojom-shared.h"
#include "content/public/android/content_jni_headers/AttributionOsLevelManager_jni.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

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

attribution_reporting::mojom::OsSupport ConvertToOsSupport(int value) {
  // See
  // https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/measurement/MeasurementManager
  // for constant values.
  static constexpr int kMeasurementApiStateDisabled = 0;
  static constexpr int kMeasurementApiStateEnabled = 1;

  switch (value) {
    case kMeasurementApiStateDisabled:
      return attribution_reporting::mojom::OsSupport::kDisabled;
    case kMeasurementApiStateEnabled:
      return attribution_reporting::mojom::OsSupport::kEnabled;
    default:
      return attribution_reporting::mojom::OsSupport::kDisabled;
  }
}

}  // namespace

AttributionOsLevelManagerAndroid::AttributionOsLevelManagerAndroid() {
  jobj_ = Java_AttributionOsLevelManager_Constructor(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AttributionOsLevelManagerAndroid::~AttributionOsLevelManagerAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Java_AttributionOsLevelManager_nativeDestroyed(
      base::android::AttachCurrentThread(), jobj_);
}

void AttributionOsLevelManagerAndroid::RegisterAttributionSource(
    const GURL& registration_url,
    const url::Origin& top_level_origin,
    bool is_debug_key_allowed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AttributionOsLevelManager_registerAttributionSource(
      env, jobj_, url::GURLAndroid::FromNativeGURL(env, registration_url),
      url::GURLAndroid::FromNativeGURL(env, top_level_origin.GetURL()),
      is_debug_key_allowed);
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

  static base::AtomicSequenceNumber g_request_id_counter;
  int request_id = g_request_id_counter.GetNext();
  pending_data_deletion_callbacks_.emplace(request_id, std::move(done));

  Java_AttributionOsLevelManager_deleteRegistrations(
      env, jobj_, request_id, delete_begin.ToJavaTime(),
      delete_end.ToJavaTime(),
      url::GURLAndroid::ToJavaArrayOfGURLs(env, j_origins),
      base::android::ToJavaArrayOfStrings(
          env, std::vector<std::string>(domains.begin(), domains.end())),
      GetDeletionMode(delete_rate_limit_data), GetMatchBehavior(mode));
}

attribution_reporting::mojom::OsSupport
AttributionOsLevelManagerAndroid::GetOsSupport() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return ConvertToOsSupport(
      Java_AttributionOsLevelManager_getMeasurementApiStatus(
          base::android::AttachCurrentThread(), jobj_));
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
