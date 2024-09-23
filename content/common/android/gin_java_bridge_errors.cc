// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/gin_java_bridge_errors.h"

#include "base/notreached.h"

namespace content {

const char* GinJavaBridgeErrorToString(mojom::GinJavaBridgeError error) {
  switch (error) {
    case mojom::GinJavaBridgeError::kGinJavaBridgeNoError:
      return "No error";
    case mojom::GinJavaBridgeError::kGinJavaBridgeUnknownObjectId:
      return "Unknown Java object ID";
    case mojom::GinJavaBridgeError::kGinJavaBridgeObjectIsGone:
      return "Java object is gone";
    case mojom::GinJavaBridgeError::kGinJavaBridgeMethodNotFound:
      return "Method not found";
    case mojom::GinJavaBridgeError::
        kGinJavaBridgeAccessToObjectGetClassIsBlocked:
      return "Access to java.lang.Object.getClass is blocked";
    case mojom::GinJavaBridgeError::kGinJavaBridgeJavaExceptionRaised:
      return "Java exception was raised during method invocation";
    case mojom::GinJavaBridgeError::kGinJavaBridgeNonAssignableTypes:
      return "The type of the object passed to the method is incompatible "
          "with the type of method's argument";
    case mojom::GinJavaBridgeError::kGinJavaBridgeRenderFrameDeleted:
      return "RenderFrame has been deleted";
  }
  NOTREACHED_IN_MIGRATION();
  return "Unknown error";
}

}  // namespace content
