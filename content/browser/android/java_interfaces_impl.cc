// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java_interfaces_impl.h"

#include <jni.h>

#include <utility>

#include "base/android/jni_android.h"
#include "base/memory/singleton.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/interface_provider.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/InterfaceRegistrarImpl_jni.h"

namespace content {

namespace {

template <BrowserThread::ID thread>
class JavaInterfaceProviderHolder {
 public:
  JavaInterfaceProviderHolder() {
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider> provider;
    JNIEnv* env = base::android::AttachCurrentThread();
    if (thread == BrowserThread::IO) {
      Java_InterfaceRegistrarImpl_createInterfaceRegistryOnIOThread(
          env, provider.InitWithNewPipeAndPassReceiver()
                   .PassPipe()
                   .release()
                   .value());
    } else {
      Java_InterfaceRegistrarImpl_createInterfaceRegistry(
          env, provider.InitWithNewPipeAndPassReceiver()
                   .PassPipe()
                   .release()
                   .value());
    }
    interface_provider_.Bind(std::move(provider));
  }

  static JavaInterfaceProviderHolder* GetInstance() {
    DCHECK_CURRENTLY_ON(thread);
    return base::Singleton<JavaInterfaceProviderHolder<thread>>::get();
  }

  service_manager::InterfaceProvider* GetJavaInterfaces() {
    return &interface_provider_;
  }

 private:
  service_manager::InterfaceProvider interface_provider_{
      base::SingleThreadTaskRunner::GetCurrentDefault()};
};

}  // namespace

service_manager::InterfaceProvider* GetGlobalJavaInterfaces() {
  return JavaInterfaceProviderHolder<BrowserThread::UI>::GetInstance()
      ->GetJavaInterfaces();
}

service_manager::InterfaceProvider* GetGlobalJavaInterfacesOnIOThread() {
  return JavaInterfaceProviderHolder<BrowserThread::IO>::GetInstance()
      ->GetJavaInterfaces();
}

void BindInterfaceRegistryForWebContents(
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver,
    WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_InterfaceRegistrarImpl_createInterfaceRegistryForWebContents(
      env, receiver.PassPipe().release().value(),
      web_contents->GetJavaWebContents());
}

void BindInterfaceRegistryForRenderFrameHost(
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver,
    RenderFrameHostImpl* render_frame_host) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_InterfaceRegistrarImpl_createInterfaceRegistryForRenderFrameHost(
      env, receiver.PassPipe().release().value(),
      render_frame_host->GetJavaRenderFrameHost());
}

}  // namespace content
