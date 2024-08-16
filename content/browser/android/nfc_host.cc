// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/nfc_host.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/mojom/nfc.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/NfcHost_jni.h"

namespace content {

namespace {
base::AtomicSequenceNumber g_unique_id;
}  // namespace

NFCHost::NFCHost(WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  DCHECK(web_contents);

  permission_controller_ =
      web_contents->GetBrowserContext()->GetPermissionController();
}

NFCHost::~NFCHost() {
  Close();
}

void NFCHost::GetNFC(RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<device::mojom::NFC> receiver) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from the outermost frame and restrict
  // from the prerendered page. Well-behaved renderer can't trigger this method
  // since mojo capabiliy control blocks during prerendering and permission
  // request of WebNFC from fenced frames is denied.
  if (render_frame_host->GetParent()) {
    mojo::ReportBadMessage("WebNFC is not allowed in an iframe.");
    return;
  }
  if (render_frame_host->GetLifecycleState() ==
      RenderFrameHost::LifecycleState::kPrerendering) {
    mojo::ReportBadMessage("WebNFC is not allowed in a prerendered page.");
    return;
  }
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage("WebNFC is not allowed within in a fenced frame.");
    return;
  }

  if (render_frame_host->GetBrowserContext()
          ->GetPermissionController()
          ->GetPermissionStatusForCurrentDocument(blink::PermissionType::NFC,
                                                  render_frame_host) !=
      blink::mojom::PermissionStatus::GRANTED) {
    return;
  }

  if (!subscription_id_) {
    // base::Unretained() is safe here because the subscription is canceled when
    // this object is destroyed.
    subscription_id_ =
        permission_controller_->SubscribeToPermissionStatusChange(
            blink::PermissionType::NFC, /*render_process_host=*/nullptr,
            render_frame_host,
            render_frame_host->GetMainFrame()
                ->GetLastCommittedOrigin()
                .GetURL(),
            /*should_include_device_status=*/false,
            base::BindRepeating(&NFCHost::OnPermissionStatusChange,
                                base::Unretained(this)));
  }

  if (!nfc_provider_) {
    content::GetDeviceService().BindNFCProvider(
        nfc_provider_.BindNewPipeAndPassReceiver());
    MaybeResumeOrSuspendOperations(web_contents()->GetVisibility());
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  // The created instance's reference is kept inside a map in Java world.
  int id = g_unique_id.GetNext();
  Java_NfcHost_create(env, web_contents()->GetJavaWebContents(), id);

  // Connect to an NFC object, associating it with |id_|.
  nfc_provider_->GetNFCForHost(id, std::move(receiver));
}

void NFCHost::RenderFrameHostChanged(RenderFrameHost* old_host,
                                     RenderFrameHost* new_host) {
  // If the main frame has been replaced then close an old NFC connection.
  if (new_host->IsInPrimaryMainFrame())
    Close();
}

void NFCHost::OnVisibilityChanged(Visibility visibility) {
  MaybeResumeOrSuspendOperations(visibility);
}

void NFCHost::MaybeResumeOrSuspendOperations(Visibility visibility) {
  // For cases NFC not initialized, such as the permission has been revoked.
  if (!nfc_provider_)
    return;

  // NFC operations should be suspended.
  // https://w3c.github.io/web-nfc/#nfc-suspended
  if (visibility == Visibility::VISIBLE)
    nfc_provider_->ResumeNFCOperations();
  else
    nfc_provider_->SuspendNFCOperations();
}

void NFCHost::OnPermissionStatusChange(blink::mojom::PermissionStatus status) {
  if (status != blink::mojom::PermissionStatus::GRANTED)
    Close();
}

void NFCHost::Close() {
  nfc_provider_.reset();
  permission_controller_->UnsubscribeFromPermissionStatusChange(
      subscription_id_);
  subscription_id_ = PermissionController::SubscriptionId();
}

}  // namespace content
