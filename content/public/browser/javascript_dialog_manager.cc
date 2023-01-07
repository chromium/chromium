// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/javascript_dialog_manager.h"

namespace content {

bool JavaScriptDialogManager::HandleJavaScriptDialog(
    WebContents* web_contents,
    bool accept,
    const std::u16string* prompt_override) {
  return false;
}

}  // namespace content
