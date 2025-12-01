// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/ime_text_span.h"

#include <jni.h>

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/ImeTextSpan_jni.h"

namespace content {
base::android::ScopedJavaLocalRef<jobjectArray> ToImeTextSpanJniArray(
    JNIEnv* env,
    const std::vector<ui::mojom::ImeTextSpanInfoPtr>& mojo_spans) {
  jobjectArray ime_text_span_array = env->NewObjectArray(
      mojo_spans.size(),
      org_chromium_content_browser_input_ImeTextSpan_clazz(env), nullptr);

  base::android::CheckException(env);

  int i = 0;
  for (const auto& span_info_ptr : mojo_spans) {
    const auto& span_ptr = span_info_ptr->span;
    base::android::ScopedJavaLocalRef<jobjectArray> j_suggestions =
        base::android::ToJavaArrayOfStrings(env, span_ptr.suggestions);

    base::android::ScopedJavaLocalRef<jobject> j_info = Java_ImeTextSpan_create(
        env, span_ptr.start_offset, span_ptr.end_offset, j_suggestions,
        static_cast<jint>(span_ptr.type), span_ptr.should_hide_suggestion_menu);

    env->SetObjectArrayElement(ime_text_span_array, i++, j_info.obj());
  }

  return base::android::ScopedJavaLocalRef<jobjectArray>::Adopt(
      env, ime_text_span_array);
}
}  // namespace content

DEFINE_JNI(ImeTextSpan)
