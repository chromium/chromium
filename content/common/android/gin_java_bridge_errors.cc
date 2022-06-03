// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/gin_java_bridge_errors.h"

#include "base/notreached.h"

namespace content {

const char* GinJavaBridgeErrorToString(GinJavaBridgeError error) {
  switch (error) {
    case kGinJavaBridgeNoError:
      return "No error";
    case kGinJavaBridgeUnknownObjectId:
      return "Unknown Java object ID";
    case kGinJavaBridgeObjectIsGone:
      return "Java object is gone";
    case kGinJavaBridgeMethodNotFound:
      return "Method not found";
    case kGinJavaBridgeAccessToObjectGetClassIsBlocked:
      return "Access to java.lang.Object.getClass is blocked";
    case kGinJavaBridgeJavaExceptionRaised:
      return "Java exception was raised during method invocation";
    case kGinJavaBridgeNonAssignableTypes:
      return "The type of the object passed to the method is incompatible "
          "with the type of method's argument";
    case kGinJavaBridgeRenderFrameDeleted:
      return "RenderFrame has been deleted";
  }
  NOTREACHED();
  return "Unknown error";
}

}  // namespace content
