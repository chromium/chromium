// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/content_main_android.h"

#include <memory>

#include "base/android/binder.h"
#include "base/android/binder_box.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/common/content_client.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_app_jni/ContentMain_jni.h"

using base::LazyInstance;
using base::android::JavaParamRef;

namespace content {

namespace {

ContentMainRunner* GetContentMainRunner() {
  static base::NoDestructor<std::unique_ptr<ContentMainRunner>> runner{
      ContentMainRunner::Create()};
  return runner->get();
}

LazyInstance<std::unique_ptr<ContentMainDelegate>>::DestructorAtExit
    g_content_main_delegate = LAZY_INSTANCE_INITIALIZER;

}  // namespace

class ContentClientCreator {
 public:
  static void Create(ContentMainDelegate* delegate) {
    ContentClient* client = delegate->CreateContentClient();
    DCHECK(client);
    SetContentClient(client);
  }
};

static void JNI_ContentMain_SetBindersFromParent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& binder_box) {
  base::android::SetBindersFromParent(
      base::android::UnpackBinderBox(env, binder_box)
          .value_or(std::vector<base::android::BinderRef>()));
}

// TODO(qinmin/hanxi): split this function into 2 separate methods: One to
// start the minimal browser and one to start the remainder of the browser
// process. The first method should always be called upon browser start, and
// the second method can be deferred. See http://crbug.com/854209.
static jint JNI_ContentMain_Start(JNIEnv* env, jboolean start_minimal_browser) {
  TRACE_EVENT0("startup", "content::Start");
  ContentMainParams params(g_content_main_delegate.Get().get());
  params.minimal_browser_mode = start_minimal_browser;
  return RunContentProcess(std::move(params), GetContentMainRunner());
}

void SetContentMainDelegate(ContentMainDelegate* delegate) {
  DCHECK(!g_content_main_delegate.Get().get());
  g_content_main_delegate.Get().reset(delegate);
  // The ContentClient needs to be set early so that it can be used by the
  // content library loader hooks.
  if (!GetContentClient())
    ContentClientCreator::Create(delegate);
}

ContentMainDelegate* GetContentMainDelegateForTesting() {
  DCHECK(g_content_main_delegate.Get().get());
  return g_content_main_delegate.Get().get();
}

}  // namespace content
