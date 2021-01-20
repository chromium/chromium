// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/choosers/date_time_chooser.mojom.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// Android implementation for DateTimeChooser dialogs.
class DateTimeChooserAndroid
    : public blink::mojom::DateTimeChooser,
      public WebContentsUserData<DateTimeChooserAndroid> {
 public:
  explicit DateTimeChooserAndroid(WebContents* web_contents);
  ~DateTimeChooserAndroid() override;

  void OnDateTimeChooserReceiver(
      mojo::PendingReceiver<blink::mojom::DateTimeChooser> receiver);

  // blink::mojom::DateTimeChooser implementation:
  // Shows the dialog. |value| is the date/time value converted to a
  // number as defined in HTML. (See blink::InputType::parseToNumber())
  void OpenDateTimeDialog(blink::mojom::DateTimeDialogValuePtr value,
                          OpenDateTimeDialogCallback callback) override;

  // Replaces the current value.
  void ReplaceDateTime(JNIEnv* env,
                       const base::android::JavaRef<jobject>&,
                       jdouble value);

  // Closes the dialog without propagating any changes.
  void CancelDialog(JNIEnv* env, const base::android::JavaRef<jobject>&);

 private:
  friend class content::WebContentsUserData<DateTimeChooserAndroid>;
  content::WebContents* web_contents_;

  OpenDateTimeDialogCallback open_date_time_response_callback_;

  base::android::ScopedJavaGlobalRef<jobject> j_date_time_chooser_;

  mojo::Receiver<blink::mojom::DateTimeChooser> date_time_chooser_receiver_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(DateTimeChooserAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_
