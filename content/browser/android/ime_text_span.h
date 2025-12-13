// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IME_TEXT_SPAN_H_
#define CONTENT_BROWSER_ANDROID_IME_TEXT_SPAN_H_

#include <jni.h>

#include <vector>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"

namespace content {

// Converts Mojo ImeTextSpanPtrs to JNI equivalent
// org.chromium.content.browser.input.ImeTextSpan
base::android::ScopedJavaLocalRef<jobjectArray> ToImeTextSpanJniArray(
    JNIEnv* env,
    const std::vector<ui::mojom::ImeTextSpanInfoPtr>& mojo_spans);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IME_TEXT_SPAN_H_
