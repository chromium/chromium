// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/date_time_chooser_android.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/unicodestring.h"
#include "content/public/android/content_jni_headers/DateTimeChooserAndroid_jni.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {

base::string16 SanitizeSuggestionString(const base::string16& string) {
  base::string16 trimmed = string.substr(0, 255);
  icu::UnicodeString sanitized;
  base::i18n::UTF16CharIterator sanitized_iterator(&trimmed);
  while (!sanitized_iterator.end()) {
    UChar c = sanitized_iterator.get();
    if (u_isprint(c))
      sanitized.append(c);
    sanitized_iterator.Advance();
  }
  return base::i18n::UnicodeStringToString16(sanitized);
}

}  // namespace

namespace content {

// DateTimeChooserAndroid implementation
DateTimeChooserAndroid::DateTimeChooserAndroid(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      date_time_chooser_receiver_(this) {}

DateTimeChooserAndroid::~DateTimeChooserAndroid() {
}

void DateTimeChooserAndroid::OnDateTimeChooserReceiver(
    mojo::PendingReceiver<blink::mojom::DateTimeChooser> receiver) {
  // Disconnect the previous picker first.
  date_time_chooser_receiver_.reset();
  date_time_chooser_receiver_.Bind(std::move(receiver));
}

void DateTimeChooserAndroid::OpenDateTimeDialog(
    blink::mojom::DateTimeDialogValuePtr value,
    OpenDateTimeDialogCallback callback) {
  JNIEnv* env = AttachCurrentThread();

  if (open_date_time_response_callback_) {
    date_time_chooser_receiver_.ReportBadMessage(
        "DateTimeChooserAndroid: Previous picker's binding isn't closed.");
    return;
  }
  open_date_time_response_callback_ = std::move(callback);

  ScopedJavaLocalRef<jobjectArray> suggestions_array;
  if (value->suggestions.size() > 0) {
    suggestions_array = Java_DateTimeChooserAndroid_createSuggestionsArray(
        env, value->suggestions.size());
    for (size_t i = 0; i < value->suggestions.size(); ++i) {
      const blink::mojom::DateTimeSuggestionPtr suggestion =
          std::move(value->suggestions[i]);
      ScopedJavaLocalRef<jstring> localized_value = ConvertUTF16ToJavaString(
          env, SanitizeSuggestionString(suggestion->localized_value));
      ScopedJavaLocalRef<jstring> label = ConvertUTF16ToJavaString(
          env, SanitizeSuggestionString(suggestion->label));
      Java_DateTimeChooserAndroid_setDateTimeSuggestionAt(
          env, suggestions_array, i, suggestion->value, localized_value, label);
    }
  }

  gfx::NativeWindow native_window = web_contents()->GetTopLevelNativeWindow();

  if (native_window && !(native_window->GetJavaObject()).is_null()) {
    j_date_time_chooser_.Reset(
        Java_DateTimeChooserAndroid_createDateTimeChooser(
            env, native_window->GetJavaObject(),
            reinterpret_cast<intptr_t>(this), value->dialog_type,
            value->dialog_value, value->minimum, value->maximum, value->step,
            suggestions_array));
  }
  if (j_date_time_chooser_.is_null())
    std::move(open_date_time_response_callback_).Run(true, value->dialog_value);
}

void DateTimeChooserAndroid::ReplaceDateTime(JNIEnv* env,
                                             const JavaRef<jobject>&,
                                             jdouble value) {
  std::move(open_date_time_response_callback_).Run(true, value);
}

void DateTimeChooserAndroid::CancelDialog(JNIEnv* env,
                                          const JavaRef<jobject>&) {
  std::move(open_date_time_response_callback_).Run(false, 0.0);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DateTimeChooserAndroid)

}  // namespace content
