// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_NFC_HOST_H_
#define CONTENT_BROWSER_ANDROID_NFC_HOST_H_

#include "base/android/jni_android.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/nfc_provider.mojom.h"

namespace content {

// On Android, NFC requires the Activity associated with the context in order to
// access the NFC system APIs. NFCHost provides this functionality by mapping
// NFC context IDs to the WebContents associated with those IDs.
class NFCHost {
 public:
  explicit NFCHost(WebContents* web_contents);
  ~NFCHost();

  void GetNFC(mojo::PendingReceiver<device::mojom::NFC> receiver);

 private:
  // This instance's ID (passed to the NFC implementation via |nfc_provider_|
  // and used from the implementation to map back to this object).
  int id_;

  // The WebContents that owns this instance.
  WebContents* web_contents_;

  mojo::Remote<device::mojom::NFCProvider> nfc_provider_;

  DISALLOW_COPY_AND_ASSIGN(NFCHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_NFC_HOST_H_
