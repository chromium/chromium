// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/activity.h"

#include "base/logging.h"
#include "base/notreached.h"

namespace extensions {

const char Activity::kCancelSuspend[] = "cancel-suspend";
const char Activity::kCreatePage[] = "create-page";
const char Activity::kIPC[] = "IPC";
const char Activity::kPictureInPicture[] = "picture-in-picture";
const char Activity::kRenderFrame[] = "render-frame";

const char* Activity::ToString(Type type) {
  switch (type) {
    case ACCESSIBILITY:
      return "ACCESSIBILITY";
    case API_FUNCTION:
      return "API_FUNCTION";
    case DEV_TOOLS:
      return "DEV_TOOLS";
    case EVENT:
      return "EVENT";
    case LIFECYCLE_MANAGEMENT:
      return "LIFECYCLE_MANAGEMENT";
    case MEDIA:
      return "MEDIA";
    case MESSAGE:
      return "MESSAGE";
    case MESSAGE_PORT:
      return "MESSAGE_PORT";
    case MODAL_DIALOG:
      return "MODAL_DIALOG";
    case MOJO:
      return "MOJO";
    case NETWORK:
      return "NETWORK";
    case PEPPER_API:
      return "PEPPER_API";
    case PROCESS_MANAGER:
      return "PROCESS_MANAGER";
    case DEBUGGER:
      return "DEBUGGER";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

}  // namespace extensions
