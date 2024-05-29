// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/date_time_chooser/android/date_time_chooser_android.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/unicodestring.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/DateTimeChooserAndroid_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {

std::u16string SanitizeSuggestionString(const std::u16string& string) {
  std::u16string trimmed = string.substr(0, 255);
  icu::UnicodeString sanitized;
  for (base::i18n::UTF16CharIterator sanitized_iterator(trimmed);
       !sanitized_iterator.end(); sanitized_iterator.Advance()) {
    UChar c = sanitized_iterator.get();
    if (u_isprint(c)) {
      sanitized.append(c);
    }
  }
  return base::i18n::UnicodeStringToString16(sanitized);
}

}  // namespace

namespace content {

// DateTimeChooserAndroid implementation
DateTimeChooserAndroid::DateTimeChooserAndroid(WebContents* web_contents)
    : DateTimeChooser(web_contents) {}

DateTimeChooserAndroid::~DateTimeChooserAndroid() {
  DismissAndDestroyJavaObject();
}

void DateTimeChooserAndroid::OpenPlatformDialog(
    blink::mojom::DateTimeDialogValuePtr value,
    OpenDateTimeDialogCallback callback) {
  JNIEnv* env = AttachCurrentThread();

  if (open_date_time_response_callback_) {
    ReportBadMessage(
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

  gfx::NativeWindow native_window = GetWebContents().GetTopLevelNativeWindow();

  if (native_window && !(native_window->GetJavaObject()).is_null()) {
    j_date_time_chooser_.Reset(
        Java_DateTimeChooserAndroid_createDateTimeChooser(
            env, native_window->GetJavaObject(),
            reinterpret_cast<intptr_t>(this), value->dialog_type,
            value->dialog_value, value->minimum, value->maximum, value->step,
            suggestions_array));
  }
  if (j_date_time_chooser_.is_null()) {
    std::move(open_date_time_response_callback_).Run(true, value->dialog_value);
  }
}

void DateTimeChooserAndroid::ClosePlatformDialog() {
  DismissAndDestroyJavaObject();
}

void DateTimeChooserAndroid::DismissAndDestroyJavaObject() {
  if (j_date_time_chooser_) {
    JNIEnv* env = AttachCurrentThread();
    Java_DateTimeChooserAndroid_dismissAndDestroy(env, j_date_time_chooser_);
    j_date_time_chooser_.Reset();
  }
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

// static
void DateTimeChooser::CreateDateTimeChooser(WebContents* web_contents) {
  web_contents->SetUserData(
      UserDataKey(), std::make_unique<DateTimeChooserAndroid>(web_contents));
}

}  // namespace content
