/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* From dev/ppb_opengles2ext_dev.idl modified Fri Sep  5 14:52:51 2014. */

#ifndef PPAPI_C_DEV_PPB_OPENGLES2EXT_DEV_H_
#define PPAPI_C_DEV_PPB_OPENGLES2EXT_DEV_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_opengles2.h"

#define PPB_OPENGLES2_DRAWBUFFERS_DEV_INTERFACE_1_0 \
    "PPB_OpenGLES2DrawBuffers(Dev);1.0"
#define PPB_OPENGLES2_DRAWBUFFERS_DEV_INTERFACE \
    PPB_OPENGLES2_DRAWBUFFERS_DEV_INTERFACE_1_0

/**
 * @file
 * This file is auto-generated from
 * gpu/command_buffer/build_gles2_cmd_buffer.py
 * It's formatted by clang-format using chromium coding style:
 *    clang-format -i -style=chromium filename
 * DO NOT EDIT! */


#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_opengles2.h"


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_OpenGLES2DrawBuffers_Dev_1_0 {
  void (*DrawBuffersEXT)(PP_Resource context,
                         GLsizei count,
                         const GLenum* bufs);
};

struct PPB_OpenGLES2DrawBuffers_Dev {
  void (*DrawBuffersEXT)(PP_Resource context,
                         GLsizei count,
                         const GLenum* bufs);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_OPENGLES2EXT_DEV_H_ */

