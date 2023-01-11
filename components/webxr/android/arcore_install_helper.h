// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_ARCORE_INSTALL_HELPER_H_
#define COMPONENTS_WEBXR_ANDROID_ARCORE_INSTALL_HELPER_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/xr_install_helper.h"

namespace webxr {

// Equivalent of ArCoreApk.Availability enum.
// For detailed description, please see:
// https://developers.google.com/ar/reference/java/arcore/reference/com/google/ar/core/ArCoreApk.Availability
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webxr
enum class ArCoreAvailability : int {
  kSupportedApkTooOld = 0,
  kSupportedInstalled = 1,
  kSupportedNotInstalled = 2,
  kUnknownChecking = 3,
  kUnknownError = 4,
  kUnknownTimedOut = 5,
  kUnsupportedDeviceNotCapable = 6,
};

// Helper class to ensure that ArCore has been properly installed from the
// store, per the ArCore Apk's installation implementation. Inherits from
// content::XrInstallHelper so that it may be returned by the
// XrIntegrationClient.
class ArCoreInstallHelper : public content::XrInstallHelper {
 public:
  explicit ArCoreInstallHelper();
  ~ArCoreInstallHelper() override;

  ArCoreInstallHelper(const ArCoreInstallHelper&) = delete;
  ArCoreInstallHelper& operator=(const ArCoreInstallHelper&) = delete;

  // content::XrInstallHelper implementation.
  void EnsureInstalled(
      int render_process_id,
      int render_frame_id,
      base::OnceCallback<void(bool)> install_callback) override;

  // Called from Java end.
  void OnRequestInstallSupportedArCoreResult(JNIEnv* env, bool success);

 private:
  void ShowMessage(int render_process_id, int render_frame_id);
  void HandleMessagePrimaryAction(int render_process_id, int render_frame_id);
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);
  void RunInstallFinishedCallback(bool succeeded);

  base::OnceCallback<void(bool)> install_finished_callback_;
  base::android::ScopedJavaGlobalRef<jobject> java_install_utils_;
  std::unique_ptr<messages::MessageWrapper> message_;

  // Must be last.
  base::WeakPtrFactory<ArCoreInstallHelper> weak_ptr_factory_{this};
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_ARCORE_INSTALL_HELPER_H_
