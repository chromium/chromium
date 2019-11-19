// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/lazy_instance.h"
#include "base/trace_event/trace_event.h"
#include "content/app/content_service_manager_main_delegate.h"
#include "content/public/android/content_jni_headers/ContentMain_jni.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_delegate.h"
#include "services/service_manager/embedder/main.h"

using base::LazyInstance;
using base::android::JavaParamRef;

namespace content {

namespace {

LazyInstance<std::unique_ptr<service_manager::MainDelegate>>::DestructorAtExit
    g_service_manager_main_delegate = LAZY_INSTANCE_INITIALIZER;

LazyInstance<std::unique_ptr<ContentMainDelegate>>::DestructorAtExit
    g_content_main_delegate = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// TODO(qinmin/hanxi): split this function into 2 separate methods: One to
// start the ServiceManager and one to start the remainder of the browser
// process. The first method should always be called upon browser start, and
// the second method can be deferred. See http://crbug.com/854209.
static jint JNI_ContentMain_Start(JNIEnv* env,
                                  jboolean start_service_manager_only) {
  TRACE_EVENT0("startup", "content::Start");

  DCHECK(!g_service_manager_main_delegate.Get() || !start_service_manager_only);

  if (!g_service_manager_main_delegate.Get()) {
    g_service_manager_main_delegate.Get() =
        std::make_unique<ContentServiceManagerMainDelegate>(
            ContentMainParams(g_content_main_delegate.Get().get()));
  }

  static_cast<ContentServiceManagerMainDelegate*>(
      g_service_manager_main_delegate.Get().get())
      ->SetStartServiceManagerOnly(start_service_manager_only);

  service_manager::MainParams main_params(
      g_service_manager_main_delegate.Get().get());
  return service_manager::Main(main_params);
}

void SetContentMainDelegate(ContentMainDelegate* delegate) {
  DCHECK(!g_content_main_delegate.Get().get());
  g_content_main_delegate.Get().reset(delegate);
}

ContentMainDelegate* GetContentMainDelegateForTesting() {
  DCHECK(g_content_main_delegate.Get().get());
  return g_content_main_delegate.Get().get();
}

}  // namespace content
