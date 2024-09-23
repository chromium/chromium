// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_os_level_manager_android.h"

#include <stddef.h>

#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/registrar.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/content_browser_client.h"
#include "services/network/public/cpp/attribution_utils.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/AttributionOsLevelManager_jni.h"

using jni_zero::AttachCurrentThread;
using jni_zero::ScopedJavaLocalRef;

namespace content {

namespace {

using ::attribution_reporting::Registrar;

using ApiState = ContentBrowserClient::AttributionReportingOsApiState;

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

void GetMeasurementApiStatus() {
  base::ElapsedThreadTimer timer;
  Java_AttributionOsLevelManager_getMeasurementApiStatus(AttachCurrentThread());
  if (timer.is_supported()) {
    base::UmaHistogramTimes("Conversions.GetMeasurementStatusTime",
                            timer.Elapsed());
  }
}

}  // namespace

static void JNI_AttributionOsLevelManager_OnMeasurementStateReturned(
    JNIEnv* env,
    jint state) {
  ApiState api_state = ConvertToApiState(state);

  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    AttributionOsLevelManager::SetApiState(api_state);
    return;
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&AttributionOsLevelManager::SetApiState, api_state));
}

AttributionOsLevelManagerAndroid::AttributionOsLevelManagerAndroid() {
  jobj_ = Java_AttributionOsLevelManager_Constructor(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this));

  if (AttributionOsLevelManager::ShouldInitializeApiState()) {
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTask(FROM_HERE, base::BindOnce(&GetMeasurementApiStatus));
  }
}

AttributionOsLevelManagerAndroid::~AttributionOsLevelManagerAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Java_AttributionOsLevelManager_nativeDestroyed(AttachCurrentThread(), jobj_);
}

namespace {

// 3/4 of the Android API calls below have atomic success/failure, while the
// fourth has success/failure per item.

std::vector<bool> AtomicSuccess(size_t num_items, bool success) {
  return std::vector<bool>(num_items, success);
}

void MergeIndividualSuccessAndInvokeCallback(
    std::vector<bool>& successes,
    size_t& remaining,
    base::OnceCallback<void(const std::vector<bool>&)>& callback,
    size_t i,
    bool success) {
  CHECK_GT(remaining, 0u);
  --remaining;

  successes.at(i) = success;

  if (remaining == 0) {
    std::move(callback).Run(successes);
  }
}

}  // namespace

void AttributionOsLevelManagerAndroid::Register(
    OsRegistration registration,
    const std::vector<bool>& is_debug_key_allowed,
    RegisterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const size_t num_items = registration.registration_items.size();
  CHECK_EQ(num_items, is_debug_key_allowed.size());

  JNIEnv* env = AttachCurrentThread();

  Registrar registrar = registration.registrar;
  attribution_reporting::mojom::RegistrationType type = registration.GetType();
  std::vector<ScopedJavaLocalRef<jobject>> registration_urls;
  base::ranges::transform(
      registration.registration_items, std::back_inserter(registration_urls),
      [env](const attribution_reporting::OsRegistrationItem& item) {
        return url::GURLAndroid::FromNativeGURL(env, item.url);
      });
  auto top_level_origin = url::GURLAndroid::FromNativeGURL(
      env, registration.top_level_origin.GetURL());
  std::optional<AttributionInputEvent> input_event = registration.input_event;

  auto bound_callback =
      base::BindOnce(std::move(callback), std::move(registration));

  switch (type) {
    case attribution_reporting::mojom::RegistrationType::kSource: {
      DCHECK(input_event.has_value());

      int request_id = next_callback_id_++;
      pending_registration_callbacks_.emplace(
          request_id, base::BindOnce(&AtomicSuccess, num_items)
                          .Then(std::move(bound_callback)));

      switch (registrar) {
        case Registrar::kWeb: {
          auto sources =
              Java_AttributionOsLevelManager_createWebSourceParamsList(
                  env, num_items);
          for (size_t i = 0; i < num_items; ++i) {
            Java_AttributionOsLevelManager_addWebSourceParams(
                env, sources, registration_urls[i], is_debug_key_allowed[i]);
          }
          Java_AttributionOsLevelManager_registerWebAttributionSource(
              env, jobj_, request_id, sources, top_level_origin,
              input_event->input_event);
          break;
        }
        case Registrar::kOs: {
          Java_AttributionOsLevelManager_registerAttributionSource(
              env, jobj_, request_id, registration_urls,
              input_event->input_event);
          break;
        }
      }
      break;
    }
    case attribution_reporting::mojom::RegistrationType::kTrigger: {
      switch (registrar) {
        case Registrar::kWeb: {
          int request_id = next_callback_id_++;
          pending_registration_callbacks_.emplace(
              request_id, base::BindOnce(&AtomicSuccess, num_items)
                              .Then(std::move(bound_callback)));

          auto triggers =
              Java_AttributionOsLevelManager_createWebTriggerParamsList(
                  env, num_items);
          for (size_t i = 0; i < num_items; ++i) {
            Java_AttributionOsLevelManager_addWebTriggerParams(
                env, triggers, registration_urls[i], is_debug_key_allowed[i]);
          }
          Java_AttributionOsLevelManager_registerWebAttributionTrigger(
              env, jobj_, request_id, triggers, top_level_origin);
          break;
        }
        case Registrar::kOs: {
          auto merge_results =
              base::BindRepeating(&MergeIndividualSuccessAndInvokeCallback,
                                  base::OwnedRef(std::vector<bool>(num_items)),
                                  base::OwnedRef(num_items),
                                  base::OwnedRef(std::move(bound_callback)));

          for (size_t i = 0; const auto& registration_url : registration_urls) {
            int request_id = next_callback_id_++;
            pending_registration_callbacks_.emplace(
                request_id, base::BindOnce(merge_results, i));
            ++i;

            Java_AttributionOsLevelManager_registerAttributionTrigger(
                env, jobj_, request_id, registration_url);
          }
          break;
        }
      }
      break;
    }
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

  JNIEnv* env = AttachCurrentThread();

  std::vector<ScopedJavaLocalRef<jobject>> j_origins;
  base::ranges::transform(
      origins, std::back_inserter(j_origins), [env](const url::Origin& origin) {
        return url::GURLAndroid::FromNativeGURL(env, origin.GetURL());
      });

  int request_id = next_callback_id_++;
  pending_data_deletion_callbacks_.emplace(request_id, std::move(done));

  Java_AttributionOsLevelManager_deleteRegistrations(
      env, jobj_, request_id, delete_begin.InMillisecondsSinceUnixEpoch(),
      delete_end.InMillisecondsSinceUnixEpoch(), j_origins,
      std::vector<std::string>(domains.begin(), domains.end()),
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
