// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java_interfaces_impl.h"

#include <jni.h>

#include <utility>

#include "base/android/jni_android.h"
#include "base/memory/singleton.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/android/content_jni_headers/InterfaceRegistrarImpl_jni.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

namespace {

class JavaInterfaceProviderHolder {
 public:
  JavaInterfaceProviderHolder() {
    service_manager::mojom::InterfaceProviderPtr provider;
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InterfaceRegistrarImpl_createInterfaceRegistryForContext(
        env, mojo::MakeRequest(&provider).PassMessagePipe().release().value());
    interface_provider_.Bind(std::move(provider));
  }

  static JavaInterfaceProviderHolder* GetInstance() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return base::Singleton<JavaInterfaceProviderHolder>::get();
  }

  service_manager::InterfaceProvider* GetJavaInterfaces() {
    return &interface_provider_;
  }

 private:
  service_manager::InterfaceProvider interface_provider_;
};

}  // namespace

service_manager::InterfaceProvider* GetGlobalJavaInterfaces() {
  return JavaInterfaceProviderHolder::GetInstance()->GetJavaInterfaces();
}

void BindInterfaceRegistryForWebContents(
    service_manager::mojom::InterfaceProviderRequest request,
    WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_InterfaceRegistrarImpl_createInterfaceRegistryForWebContents(
      env, request.PassMessagePipe().release().value(),
      web_contents->GetJavaWebContents());
}

void BindInterfaceRegistryForRenderFrameHost(
    service_manager::mojom::InterfaceProviderRequest request,
    RenderFrameHostImpl* render_frame_host) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_InterfaceRegistrarImpl_createInterfaceRegistryForRenderFrameHost(
      env, request.PassMessagePipe().release().value(),
      render_frame_host->GetJavaRenderFrameHost());
}

}  // namespace content
