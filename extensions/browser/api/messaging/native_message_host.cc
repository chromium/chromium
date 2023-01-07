// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/native_message_host.h"

namespace extensions {

const char NativeMessageHost::kFailedToStartError[] =
    "Failed to start native messaging host.";
const char NativeMessageHost::kInvalidNameError[] =
    "Invalid native messaging host name specified.";
const char NativeMessageHost::kNativeHostExited[] = "Native host has exited.";
const char NativeMessageHost::kNotFoundError[] =
    "Specified native messaging host not found.";
const char NativeMessageHost::kForbiddenError[] =
    "Access to the specified native messaging host is forbidden.";
const char NativeMessageHost::kHostInputOutputError[] =
    "Error when communicating with the native messaging host.";

}  // extensions
