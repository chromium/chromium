// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_TYPE_H_
#define IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_TYPE_H_

namespace web {

// Enum specifying different types of JavaScript messages.
enum JavaScriptDialogType {
  // Dialog with OK button only.
  JAVASCRIPT_DIALOG_TYPE_ALERT = 1,
  // Dialog with OK and Cancel button.
  JAVASCRIPT_DIALOG_TYPE_CONFIRM,
  // Dialog with OK button, Cancel button, and a text field.
  JAVASCRIPT_DIALOG_TYPE_PROMPT
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_TYPE_H_
