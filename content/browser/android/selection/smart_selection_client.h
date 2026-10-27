// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SELECTION_SMART_SELECTION_CLIENT_H_
#define CONTENT_BROWSER_ANDROID_SELECTION_SMART_SELECTION_CLIENT_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class WebContents;

// The native portion of Java SmartSelectionClient.
// This class performs one operation: retrieves the selected text together with
// its surrounding context from WebContents. This surrounding text is obtained
// asynchronously from the current focused frame and passed back to Java layer.
class SmartSelectionClient : public WebContentsObserver {
 public:
  explicit SmartSelectionClient(WebContents* web_contents);

  SmartSelectionClient(const SmartSelectionClient&) = delete;
  SmartSelectionClient& operator=(const SmartSelectionClient&) = delete;

  ~SmartSelectionClient() override;

  // Sends asynchronius request to retrieve the text.
  void RequestSurroundingText(JNIEnv* env,
                              int num_extra_characters,
                              int callback_data);

  // Cancels all pending requests.
  void CancelAllRequests(JNIEnv* env);

 private:
  // WebContentsObserver:
  void WebContentsDestroyed() override;

  void OnSurroundingTextReceived(int callback_data,
                                 const std::u16string& text,
                                 uint32_t start,
                                 uint32_t end);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject(JNIEnv* env);

  base::WeakPtrFactory<SmartSelectionClient> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SELECTION_SMART_SELECTION_CLIENT_H_
