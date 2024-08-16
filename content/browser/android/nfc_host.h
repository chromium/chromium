// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_NFC_HOST_H_
#define CONTENT_BROWSER_ANDROID_NFC_HOST_H_

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/nfc_provider.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"

namespace content {

// On Android, NFC requires the Activity associated with the context in order to
// access the NFC system APIs. NFCHost provides this functionality by mapping
// NFC context IDs to the WebContents associated with those IDs.
class NFCHost : public WebContentsObserver {
 public:
  explicit NFCHost(WebContents* web_contents);

  NFCHost(const NFCHost&) = delete;
  NFCHost& operator=(const NFCHost&) = delete;

  ~NFCHost() override;

  void GetNFC(RenderFrameHost* render_frame_host,
              mojo::PendingReceiver<device::mojom::NFC> receiver);

  // WebContentsObserver implementation.
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void OnVisibilityChanged(Visibility visibility) override;

 private:
  void MaybeResumeOrSuspendOperations(Visibility visibility);
  void OnPermissionStatusChange(blink::mojom::PermissionStatus status);
  void Close();

  // The permission controller for this browser context.
  raw_ptr<PermissionController> permission_controller_;

  mojo::Remote<device::mojom::NFCProvider> nfc_provider_;

  // Permission change subscription ID provided by |permission_controller_|.
  PermissionController::SubscriptionId subscription_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_NFC_HOST_H_
