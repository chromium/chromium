// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/arcore_install_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/infobars/android/confirm_infobar.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/webxr/android/ar_jni_headers/ArCoreInstallUtils_jni.h"
#include "components/webxr/android/webxr_utils.h"
#include "components/webxr/android/xr_install_helper_delegate.h"
#include "components/webxr/android/xr_install_infobar.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;

namespace webxr {

ArCoreInstallHelper::ArCoreInstallHelper(
    std::unique_ptr<XrInstallHelperDelegate> install_delegate)
    : install_delegate_(std::move(install_delegate)) {
  DCHECK(install_delegate_);

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
    ShowInfoBar(render_process_id, render_frame_id);
    return;
  }

  // ARCore did not need to be installed/updated so mock out that its
  // installation succeeded.
  OnRequestInstallSupportedArCoreResult(nullptr, true);
}

void ArCoreInstallHelper::ShowInfoBar(int render_process_id,
                                      int render_frame_id) {
  DVLOG(1) << __func__;

  infobars::InfoBarManager* infobar_manager =
      install_delegate_->GetInfoBarManager(
          GetWebContents(render_process_id, render_frame_id));

  // We can't show an infobar without an |infobar_manager|, so if it's null,
  // report that we are not installed and stop processing.
  if (!infobar_manager) {
    DVLOG(2) << __func__ << ": infobar_manager is null";
    RunInstallFinishedCallback(false);
    return;
  }

  ArCoreAvailability availability = static_cast<ArCoreAvailability>(
      Java_ArCoreInstallUtils_getArCoreInstallStatus(AttachCurrentThread()));
  int message_text = -1;
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
      message_text = IDS_AR_CORE_CHECK_INFOBAR_INSTALL_TEXT;
      button_text = IDS_INSTALL;
      break;
    }
    case ArCoreAvailability::kSupportedApkTooOld: {
      message_text = IDS_AR_CORE_CHECK_INFOBAR_UPDATE_TEXT;
      button_text = IDS_UPDATE;
      break;
    }
    case ArCoreAvailability::kSupportedInstalled:
      NOTREACHED();
      break;
  }

  DCHECK_NE(-1, message_text);
  DCHECK_NE(-1, button_text);

  // Binding ourself as a weak ref is okay, since our destructor will still
  // guarantee that the callback is run if we are destroyed while waiting for
  // the callback from the infobar.
  // TODO(ijamardo, https://crbug.com/838833): Add icon for AR info bar.
  auto delegate = std::make_unique<XrInstallInfoBar>(
      infobars::InfoBarDelegate::InfoBarIdentifier::AR_CORE_UPGRADE_ANDROID,
      IDR_ANDROID_AR_CORE_INSALL_ICON, message_text, button_text,
      base::BindOnce(&ArCoreInstallHelper::OnInfoBarResponse,
                     weak_ptr_factory_.GetWeakPtr(), render_process_id,
                     render_frame_id));

  infobar_manager->AddInfoBar(
      std::make_unique<infobars::ConfirmInfoBar>(std::move(delegate)));
}

void ArCoreInstallHelper::OnInfoBarResponse(int render_process_id,
                                            int render_frame_id,
                                            bool try_install) {
  DVLOG(1) << __func__ << ": try_install=" << try_install;
  if (!try_install) {
    OnRequestInstallSupportedArCoreResult(nullptr, false);
    return;
  }

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
