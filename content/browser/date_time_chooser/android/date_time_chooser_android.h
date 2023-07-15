// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DATE_TIME_CHOOSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_
#define CONTENT_BROWSER_DATE_TIME_CHOOSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/gtest_prod_util.h"
#include "content/browser/date_time_chooser/date_time_chooser.h"
#include "content/common/content_export.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// Android implementation for DateTimeChooser dialogs.
class CONTENT_EXPORT DateTimeChooserAndroid : public DateTimeChooser {
 public:
  explicit DateTimeChooserAndroid(WebContents* web_contents);
  ~DateTimeChooserAndroid() override;

  DateTimeChooserAndroid(const DateTimeChooserAndroid&) = delete;
  DateTimeChooserAndroid& operator=(const DateTimeChooserAndroid&) = delete;

  // Replaces the current value.
  void ReplaceDateTime(JNIEnv* env,
                       const base::android::JavaRef<jobject>&,
                       jdouble value);

  // Closes the dialog without propagating any changes.
  void CancelDialog(JNIEnv* env, const base::android::JavaRef<jobject>&);

 protected:
  // DateTimeChooser:
  void OpenPlatformDialog(blink::mojom::DateTimeDialogValuePtr value,
                          OpenDateTimeDialogCallback callback) override;
  void ClosePlatformDialog() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DateTimeChooserBrowserTest,
                           ResetResponseCallbackViaDisconnectionHandler);

  void DismissAndDestroyJavaObject();

  base::android::ScopedJavaGlobalRef<jobject> j_date_time_chooser_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DATE_TIME_CHOOSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_
