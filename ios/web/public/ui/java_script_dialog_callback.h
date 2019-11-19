// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_CALLBACK_H_
#define IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_CALLBACK_H_

#import <Foundation/Foundation.h>

#include "base/callback.h"

namespace web {

// Callback for |RunJavaScriptDialog|. The |success| value is true if the user
// responded with OK, |false| if the dialog was cancelled. The |user_input|
// value will exist for prompt alerts only.
typedef base::OnceCallback<void(bool success, NSString* user_input)>
    DialogClosedCallback;

}  // namespace web

#endif  // IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_CALLBACK_H_
