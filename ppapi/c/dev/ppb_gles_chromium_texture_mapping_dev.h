// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_H_
#define PPAPI_C_DEV_PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_H_

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_opengles2.h"

#define PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_INTERFACE_0_1 \
    "PPB_GLESChromiumTextureMapping(Dev);0.1"
#define PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_INTERFACE \
    PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_INTERFACE_0_1

// PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_INTERFACE is deprecated. Please use
// PPB_OPENGLES2_CHROMIUMMAPSUB_INTERFACE instead.

#define PPB_OPENGLES2_CHROMIUMMAPSUB_DEV_INTERFACE_1_0 \
    "PPB_OpenGLES2ChromiumMapSub(Dev);1.0"

// PPB_OPENGLES2_CHROMIUMMAPSUB_DEV_INTERFACE_1_0 is also deprecated.  Please
// use PPB_OPENGLES2_CHROMIUMMAPSUB_INTERFACE instead.

struct PPB_GLESChromiumTextureMapping_Dev_0_1 {
  // Maps the sub-image of a texture. 'level', 'xoffset', 'yoffset', 'width',
  // 'height', 'format' and 'type' correspond to the similarly named parameters
  // of TexSubImage2D, and define the sub-image region, as well as the format of
  // the data. 'access' must be one of GL_READ_ONLY, GL_WRITE_ONLY or
  // GL_READ_WRITE. If READ is included, the returned buffer will contain the
  // pixel data for the sub-image. If WRITE is included, the pixel data for the
  // sub-image will be updated to the contents of the buffer when
  // UnmapTexSubImage2DCHROMIUM is called. NOTE: for a GL_WRITE_ONLY map, it
  // means that all the values of the buffer must be written.
  void* (*MapTexSubImage2DCHROMIUM)(
     PP_Resource context,
     GLenum target,
     GLint level,
     GLint xoffset,
     GLint yoffset,
     GLsizei width,
     GLsizei height,
     GLenum format,
     GLenum type,
     GLenum access);

  // Unmaps the sub-image of a texture. If the sub-image was mapped with one of
  // the WRITE accesses, the pixels are updated at this time to the contents of
  // the buffer. 'mem' must be the pointer returned by MapTexSubImage2DCHROMIUM.
  void (*UnmapTexSubImage2DCHROMIUM)(PP_Resource context, const void* mem);
};

typedef struct PPB_GLESChromiumTextureMapping_Dev_0_1
    PPB_GLESChromiumTextureMapping_Dev;

#endif  // PPAPI_C_DEV_PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_H_
