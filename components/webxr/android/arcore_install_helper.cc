// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/arcore_install_helper.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/webxr/android/webxr_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webxr/android/xr_jni_headers/ArCoreInstallUtils_jni.h"

using base::android::AttachCurrentThread;

namespace webxr {

namespace {
// Increase the timeout for the message to 60s from the default 10s.
constexpr base::TimeDelta kMessageTimeout = base::Seconds(60);
}  // namespace

ArCoreInstallHelper::ArCoreInstallHelper() {
  // As per documentation, it's recommended to issue a call to
  // ArCoreApk.checkAvailability() early in application lifecycle & ignore the
  // result so that subsequent calls can return cached result:
  // https://developers.google.com/ar/develop/java/enable-arcore
  // In the event that a remote call is required, it will not block on that
  // remote call per:
  // https://developers.google.com/ar/reference/java/arcore/reference/com/google/ar/core/ArCoreApk#checkAvailability
  Java_ArCoreInstallUtils_shouldRequestInstallSupportedArCore(
      AttachCurrentThread());

  java_install_utils_ = Java_ArCoreInstallUtils_create(
      AttachCurrentThread(), reinterpret_cast<jlong>(this));
}

ArCoreInstallHelper::~ArCoreInstallHelper() {
  if (!java_install_utils_.is_null()) {
    Java_ArCoreInstallUtils_onNativeDestroy(AttachCurrentThread(),
                                            java_install_utils_);
  }

  RunInstallFinishedCallback(false);
}

void ArCoreInstallHelper::EnsureInstalled(
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> install_callback) {
  DVLOG(1) << __func__ << ": java_install_utils_.is_null()="
           << java_install_utils_.is_null();

  DCHECK(!install_finished_callback_);
  install_finished_callback_ = std::move(install_callback);

  if (java_install_utils_.is_null()) {
    RunInstallFinishedCallback(false);
    return;
  }

  // ARCore is not installed or requires an update.
  if (Java_ArCoreInstallUtils_shouldRequestInstallSupportedArCore(
          AttachCurrentThread())) {
    ShowMessage(render_process_id, render_frame_id);
    return;
  }

  // ARCore did not need to be installed/updated so mock out that its
  // installation succeeded.
  OnRequestInstallSupportedArCoreResult(nullptr, true);
}

void ArCoreInstallHelper::ShowMessage(int render_process_id,
                                      int render_frame_id) {
  DVLOG(1) << __func__;

  ArCoreAvailability availability = static_cast<ArCoreAvailability>(
      Java_ArCoreInstallUtils_getArCoreInstallStatus(AttachCurrentThread()));
  int message_title = -1;
  int button_text = -1;
  switch (availability) {
    case ArCoreAvailability::kUnsupportedDeviceNotCapable: {
      RunInstallFinishedCallback(false);
      return;  // No need to process further
    }
    case ArCoreAvailability::kUnknownChecking:
    case ArCoreAvailability::kUnknownError:
    case ArCoreAvailability::kUnknownTimedOut:
    case ArCoreAvailability::kSupportedNotInstalled: {
      message_title = IDS_AR_CORE_CHECK_MESSAGE_INSTALL_TITLE;
      button_text = IDS_INSTALL;
      break;
    }
    case ArCoreAvailability::kSupportedApkTooOld: {
      message_title = IDS_AR_CORE_CHECK_MESSAGE_UPDATE_TITLE;
      button_text = IDS_UPDATE;
      break;
    }
    case ArCoreAvailability::kSupportedInstalled:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  DCHECK_NE(-1, message_title);
  DCHECK_NE(-1, button_text);

  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::AR_CORE_UPGRADE,
      base::BindOnce(&ArCoreInstallHelper::HandleMessagePrimaryAction,
                     base::Unretained(this), render_process_id,
                     render_frame_id),
      base::BindOnce(&ArCoreInstallHelper::HandleMessageDismissed,
                     base::Unretained(this)));

  message_->SetTitle(l10n_util::GetStringUTF16(message_title));
  message_->SetDescription(
      l10n_util::GetStringUTF16(IDS_AR_CORE_CHECK_MESSAGE_DESCRIPTION));
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(button_text));
  messages::MessageDispatcherBridge* message_dispatcher_bridge =
      messages::MessageDispatcherBridge::Get();
  message_->SetIconResourceId(message_dispatcher_bridge->MapToJavaDrawableId(
      IDR_ANDROID_AR_CORE_INSALL_ICON));
  message_->SetDuration(kMessageTimeout.InMilliseconds());

  message_dispatcher_bridge->EnqueueMessage(
      message_.get(), GetWebContents(render_process_id, render_frame_id),
      messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}

void ArCoreInstallHelper::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  // If the message is dismissed for any reason other than the primary action
  // button click to install/update ARCore, indicate to the deferred callback
  // that no installation/update was facilitated.
  if (dismiss_reason != messages::DismissReason::PRIMARY_ACTION) {
    OnRequestInstallSupportedArCoreResult(nullptr, false);
  }
  DCHECK(message_);
  message_.reset();
}

void ArCoreInstallHelper::HandleMessagePrimaryAction(int render_process_id,
                                                     int render_frame_id) {
  // When completed, java will call: OnRequestInstallSupportedArCoreResult
  Java_ArCoreInstallUtils_requestInstallSupportedArCore(
      AttachCurrentThread(), java_install_utils_,
      GetJavaWebContents(render_process_id, render_frame_id));
}

void ArCoreInstallHelper::OnRequestInstallSupportedArCoreResult(JNIEnv* env,
                                                                bool success) {
  DVLOG(1) << __func__;

  // Nothing else to do, simply call the deferred callback.
  RunInstallFinishedCallback(success);
}

void ArCoreInstallHelper::RunInstallFinishedCallback(bool succeeded) {
  if (install_finished_callback_) {
    std::move(install_finished_callback_).Run(succeeded);
  }
}

}  // namespace webxr
