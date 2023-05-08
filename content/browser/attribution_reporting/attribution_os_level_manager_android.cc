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
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/public/android/content_jni_headers/AttributionOsLevelManager_jni.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ScopedApiStateForTesting =
    ::content::AttributionOsLevelManagerAndroid::ScopedApiStateForTesting;

using ApiState = ::content::AttributionOsLevelManagerAndroid::ApiState;

#if DCHECK_IS_ON()
const base::SequenceChecker& GetSequenceChecker() {
  static base::NoDestructor<base::SequenceChecker> checker;
  return *checker;
}
#endif

// This flag is per device and can only be changed by the OS. Currently we don't
// observe setting changes on the device and the flag is only initialized once
// on startup. The value may vary in tests.
absl::optional<ApiState> g_state GUARDED_BY_CONTEXT(GetSequenceChecker());

void SetApiState(absl::optional<ApiState> state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetSequenceChecker());

  ApiState previous = AttributionOsLevelManagerAndroid::GetApiState();

  g_state = state;

  if (previous == AttributionOsLevelManagerAndroid::GetApiState()) {
    return;
  }

  AttributionManagerImpl::UpdateSupportForRenderProcessHosts();
}

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
  SetApiState(ConvertToApiState(state));

  base::UmaHistogramEnumeration("Conversions.AttributionSupport",
                                AttributionManagerImpl::GetSupport());
}

ScopedApiStateForTesting::ScopedApiStateForTesting(ApiState state)
    : previous_(g_state) {
  SetApiState(state);
}

ScopedApiStateForTesting::~ScopedApiStateForTesting() {
  SetApiState(previous_);
}

// static
ApiState AttributionOsLevelManagerAndroid::GetApiState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetSequenceChecker());
  return g_state.value_or(ApiState::kDisabled);
}

AttributionOsLevelManagerAndroid::AttributionOsLevelManagerAndroid() {
  jobj_ = Java_AttributionOsLevelManager_Constructor(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));

  InitializeOsSupport();
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
      Java_AttributionOsLevelManager_registerAttributionSource(
          env, jobj_, request_id, registration_url, top_level_origin,
          is_debug_key_allowed, registration.input_event->input_event);
      break;
    case attribution_reporting::mojom::OsRegistrationType::kTrigger:
      Java_AttributionOsLevelManager_registerAttributionTrigger(
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

  // Currently Android and Chromium have different matching behaviors when both
  // `origins` and `domains` are empty.
  // Chromium: Delete -> Delete nothing; Preserve -> Delete all.
  // Android: Delete -> Delete all; Preserve -> Delete nothing.
  // Android may fix the behavior in the future. As a workaround, Chromium will
  // not call Android if it's to delete nothing (no-op), and call Android with
  // both Delete and Preserve modes if it's to delete all. These two modes will
  // be one no-op and one delete all in Android releases with and without the
  // fix. See crbug.com/1442967.
  if (origins.empty() && domains.empty()) {
    switch (mode) {
      case BrowsingDataFilterBuilder::Mode::kDelete:
        // No-op.
        std::move(done).Run();
        return;
      case BrowsingDataFilterBuilder::Mode::kPreserve: {
        // Delete everything.
        auto barrier = base::BarrierClosure(2, std::move(done));

        auto empty_origins = url::GURLAndroid::ToJavaArrayOfGURLs(env, {});
        auto empty_domains = base::android::ToJavaArrayOfStrings(
            env, std::vector<std::string>());

        for (auto match_mode : {BrowsingDataFilterBuilder::Mode::kDelete,
                                BrowsingDataFilterBuilder::Mode::kPreserve}) {
          int request_id = next_callback_id_++;
          pending_data_deletion_callbacks_.emplace(request_id, barrier);
          Java_AttributionOsLevelManager_deleteRegistrations(
              env, jobj_, request_id, delete_begin.ToJavaTime(),
              delete_end.ToJavaTime(), empty_origins, empty_domains,
              GetDeletionMode(delete_rate_limit_data),
              GetMatchBehavior(match_mode));
        }

        return;
      }
    }
  }

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

void AttributionOsLevelManagerAndroid::InitializeOsSupport() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetSequenceChecker());

  if (g_state.has_value()) {
    return;
  }

  // Only make the async call once.
  g_state.emplace(ApiState::kDisabled);

  Java_AttributionOsLevelManager_getMeasurementApiStatus(
      base::android::AttachCurrentThread(), jobj_);
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
