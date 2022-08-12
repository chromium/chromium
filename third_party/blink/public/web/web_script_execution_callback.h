// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SCRIPT_EXECUTION_CALLBACK_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SCRIPT_EXECUTION_CALLBACK_H_

#include "base/callback.h"

namespace v8 {
class Value;
template <class T>
class Local;
}

namespace base {
class TimeTicks;
}

namespace blink {

template <typename T>
class WebVector;

using WebScriptExecutionCallback =
    base::OnceCallback<void(const WebVector<v8::Local<v8::Value>>&,
                            base::TimeTicks)>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SCRIPT_EXECUTION_CALLBACK_H_
