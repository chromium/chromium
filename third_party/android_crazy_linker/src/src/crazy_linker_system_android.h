// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_SYSTEM_ANDROID_H
#define CRAZY_LINKER_SYSTEM_ANDROID_H

namespace crazy {

// From android.os.Build.VERSION_CODES.LOLLIPOP.
// Using a macro since constexpr in headers are problematic in C++.
#define ANDROID_SDK_VERSION_CODE_LOLLIPOP 21

// Return the current Android device's API level.
int GetAndroidDeviceApiLevel();

}  // namespace crazy

#endif  // CRAZY_LINKER_SYSTEM_ANDROID_H
