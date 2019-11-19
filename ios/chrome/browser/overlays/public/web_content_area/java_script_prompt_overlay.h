// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_PROMPT_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_PROMPT_OVERLAY_H_

#include "ios/chrome/browser/overlays/public/overlay_user_data.h"
#include "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_source.h"

// Configuration object for OverlayRequests for JavaScript prompt() calls.
class JavaScriptPromptOverlayRequestConfig
    : public OverlayUserData<JavaScriptPromptOverlayRequestConfig> {
 public:
  ~JavaScriptPromptOverlayRequestConfig() override;

  // The source of the prompt.
  const JavaScriptDialogSource& source() const { return source_; }
  // The message to be displayed in the prompt dialog.
  const std::string& message() const { return message_; }
  // The default text to use in the prompt's text field.
  const std::string& default_prompt_value() const {
    return default_prompt_value_;
  }

 private:
  OVERLAY_USER_DATA_SETUP(JavaScriptPromptOverlayRequestConfig);
  JavaScriptPromptOverlayRequestConfig(const JavaScriptDialogSource& source,
                                       const std::string& message,
                                       const std::string& default_prompt_value);

  const JavaScriptDialogSource source_;
  const std::string message_;
  const std::string default_prompt_value_;
};

// User interaction info for OverlayResponses for JavaScript prompt() calls.
class JavaScriptPromptOverlayResponseInfo
    : public OverlayUserData<JavaScriptPromptOverlayResponseInfo> {
 public:
  ~JavaScriptPromptOverlayResponseInfo() override;

  // The text entered into the prompt's field.
  const std::string& text_input() const { return text_input_; }

 private:
  OVERLAY_USER_DATA_SETUP(JavaScriptPromptOverlayResponseInfo);
  JavaScriptPromptOverlayResponseInfo(const std::string& text_input);

  const std::string text_input_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_PROMPT_OVERLAY_H_
