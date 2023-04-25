// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// This class is responsible for communicating with java code to handle
// registering events received on the web with Android.
class CONTENT_EXPORT AttributionOsLevelManagerAndroid
    : public AttributionOsLevelManager {
 public:
  enum class ApiState {
    kDisabled,
    kEnabled,
  };

  class CONTENT_EXPORT ScopedApiStateForTesting {
   public:
    explicit ScopedApiStateForTesting(ApiState);
    ~ScopedApiStateForTesting();

    ScopedApiStateForTesting(const ScopedApiStateForTesting&) = delete;
    ScopedApiStateForTesting& operator=(const ScopedApiStateForTesting&) =
        delete;

    ScopedApiStateForTesting(ScopedApiStateForTesting&&) = delete;
    ScopedApiStateForTesting& operator=(ScopedApiStateForTesting&&) = delete;

   private:
    const absl::optional<ApiState> previous_;
  };

  // Returns whether OS-level attribution is enabled. `kDisabled` is returned
  // before the result is returned from JNI.
  static ApiState GetApiState();

  AttributionOsLevelManagerAndroid();
  ~AttributionOsLevelManagerAndroid() override;

  AttributionOsLevelManagerAndroid(const AttributionOsLevelManagerAndroid&) =
      delete;
  AttributionOsLevelManagerAndroid& operator=(
      const AttributionOsLevelManagerAndroid&) = delete;

  AttributionOsLevelManagerAndroid(AttributionOsLevelManagerAndroid&&) = delete;
  AttributionOsLevelManagerAndroid& operator=(
      AttributionOsLevelManagerAndroid&&) = delete;

  void Register(const OsRegistration&,
                bool is_debug_key_allowed,
                base::OnceCallback<void(bool success)>) override;

  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 const std::set<url::Origin>& origins,
                 const std::set<std::string>& domains,
                 BrowsingDataFilterBuilder::Mode mode,
                 bool delete_rate_limit_data,
                 base::OnceClosure done) override;

  // These are exposed to JNI and therefore have to be public.
  void OnDataDeletionCompleted(JNIEnv* env, jint request_id);
  void OnRegistrationCompleted(JNIEnv* env, jint request_id, bool success);

 private:
  void InitializeOsSupport() VALID_CONTEXT_REQUIRED(sequence_checker_);

  base::flat_map<int, base::OnceClosure> pending_data_deletion_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::flat_map<int, base::OnceCallback<void(bool success)>>
      pending_registration_callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);

  int next_callback_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::android::ScopedJavaGlobalRef<jobject> jobj_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_ANDROID_H_
