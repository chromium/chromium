// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_

#include <jni.h>

#include <cstdint>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace device {
class DiscoverableCredentialMetadata;
}  // namespace device

namespace webauthn {

class WebauthnBrowserBridge {
 public:
  WebauthnBrowserBridge(JNIEnv* env,
                        const base::android::JavaRef<jobject>& jbridge);

  WebauthnBrowserBridge(const WebauthnBrowserBridge&) = delete;
  WebauthnBrowserBridge& operator=(const WebauthnBrowserBridge&) = delete;

  ~WebauthnBrowserBridge();

  void OnCredentialsDetailsListReceived(
      JNIEnv* env,
      std::vector<device::DiscoverableCredentialMetadata>&& credentials,
      content::RenderFrameHost* render_frame_host,
      int32_t mediation_type,
      const base::android::JavaRef<jobject>& jcredential_callback,
      const base::android::JavaRef<jobject>& jhybrid_callback,
      const base::android::JavaRef<jobject>& jnon_credential_callback) const;

  void CleanupRequest(content::RenderFrameHost* render_frame_host) const;

  void CleanupCredManRequest(content::RenderFrameHost* render_frame_host) const;

  void OnCredManConditionalRequestPending(
      content::RenderFrameHost* render_frame_host,
      bool has_results,
      base::RepeatingCallback<void(bool)>&& full_request_callback);

  void OnCredManUiClosed(content::RenderFrameHost* render_frame_host,
                         bool success);

  void OnPasswordCredentialReceived(content::RenderFrameHost* render_frame_host,
                                    std::u16string&& username,
                                    std::u16string&& password);

 private:
  // Java object that owns this WebauthnBrowserBridge.
  base::android::ScopedJavaGlobalRef<jobject> owner_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_
