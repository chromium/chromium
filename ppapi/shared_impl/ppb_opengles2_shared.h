// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_OPENGLES2_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_OPENGLES2_SHARED_H_

#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/dev/ppb_opengles2ext_dev.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_OpenGLES2_Shared {
 public:
  static const PPB_OpenGLES2* GetInterface();
  static const PPB_OpenGLES2InstancedArrays* GetInstancedArraysInterface();
  static const PPB_OpenGLES2FramebufferBlit* GetFramebufferBlitInterface();
  static const PPB_OpenGLES2FramebufferMultisample*
      GetFramebufferMultisampleInterface();
  static const PPB_OpenGLES2ChromiumEnableFeature*
      GetChromiumEnableFeatureInterface();
  static const PPB_OpenGLES2ChromiumMapSub* GetChromiumMapSubInterface();
  static const PPB_OpenGLES2Query* GetQueryInterface();
  static const PPB_OpenGLES2VertexArrayObject* GetVertexArrayObjectInterface();
  static const PPB_OpenGLES2DrawBuffers_Dev* GetDrawBuffersInterface();
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_OPENGLES2_SHARED_H_
