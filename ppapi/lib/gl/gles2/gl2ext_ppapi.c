// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"

#include <stddef.h>

#ifndef GL_FALSE
#define GL_FALSE 0
#endif  // GL_FALSE

#ifndef GL_TRUE
#define GL_TRUE 1
#endif  // GL_TRUE

#if defined(__GNUC__) && !defined(__APPLE__) && !defined(ANDROID)
#define PP_TLS __thread
#elif defined(_MSC_VER)
#define PP_TLS __declspec(thread)
#else
// TODO(alokp): Fix all other platforms.
#define PP_TLS
#endif

// TODO(alokp): This will need to be thread-safe if we build gles2 as a
// shared library.
static const struct PPB_OpenGLES2* g_gles2_interface = NULL;
static const struct PPB_OpenGLES2InstancedArrays*
    g_gles2_instanced_arrays_interface = NULL;
static const struct PPB_OpenGLES2FramebufferBlit*
    g_gles2_framebuffer_blit_interface = NULL;
static const struct PPB_OpenGLES2FramebufferMultisample*
    g_gles2_framebuffer_multisample_interface = NULL;
static const struct PPB_OpenGLES2ChromiumEnableFeature*
    g_gles2_chromium_enable_feature_interface = NULL;
static const struct PPB_OpenGLES2ChromiumMapSub*
    g_gles2_chromium_map_sub_interface = NULL;
static const struct PPB_OpenGLES2Query*
    g_gles2_query_interface = NULL;
static const struct PPB_OpenGLES2VertexArrayObject*
    g_gles2_vertex_array_object_interface = NULL;
static const struct PPB_OpenGLES2DrawBuffers_Dev*
    g_gles2_draw_buffers_interface = NULL;

// TODO(alokp): Make sure PP_TLS works on all supported platforms.
static PP_TLS PP_Resource g_current_context = 0;

GLboolean GL_APIENTRY glInitializePPAPI(
    PPB_GetInterface get_browser_interface) {
  if (!g_gles2_interface) {
    g_gles2_interface = get_browser_interface(PPB_OPENGLES2_INTERFACE);
  }
  if (!g_gles2_instanced_arrays_interface) {
    g_gles2_instanced_arrays_interface =
        get_browser_interface(PPB_OPENGLES2_INSTANCEDARRAYS_INTERFACE);
  }
  if (!g_gles2_framebuffer_blit_interface) {
    g_gles2_framebuffer_blit_interface =
        get_browser_interface(PPB_OPENGLES2_FRAMEBUFFERBLIT_INTERFACE);
  }
  if (!g_gles2_framebuffer_multisample_interface) {
    g_gles2_framebuffer_multisample_interface =
        get_browser_interface(
            PPB_OPENGLES2_FRAMEBUFFERMULTISAMPLE_INTERFACE);
  }
  if (!g_gles2_chromium_enable_feature_interface) {
    g_gles2_chromium_enable_feature_interface =
        get_browser_interface(
            PPB_OPENGLES2_CHROMIUMENABLEFEATURE_INTERFACE);
  }
  if (!g_gles2_chromium_map_sub_interface) {
    g_gles2_chromium_map_sub_interface =
        get_browser_interface(PPB_OPENGLES2_CHROMIUMMAPSUB_INTERFACE);
  }
  if (!g_gles2_query_interface) {
    g_gles2_query_interface =
        get_browser_interface(PPB_OPENGLES2_QUERY_INTERFACE);
  }
  if (!g_gles2_vertex_array_object_interface) {
    g_gles2_vertex_array_object_interface =
        get_browser_interface(PPB_OPENGLES2_VERTEXARRAYOBJECT_INTERFACE);
  }
  if (!g_gles2_draw_buffers_interface) {
    g_gles2_draw_buffers_interface =
        get_browser_interface(PPB_OPENGLES2_DRAWBUFFERS_DEV_INTERFACE);
  }
  return g_gles2_interface ? GL_TRUE : GL_FALSE;
}

GLboolean GL_APIENTRY glTerminatePPAPI(void) {
  g_gles2_interface = NULL;
  return GL_TRUE;
}

void GL_APIENTRY glSetCurrentContextPPAPI(PP_Resource context) {
  g_current_context = context;
}

PP_Resource GL_APIENTRY glGetCurrentContextPPAPI(void) {
  return g_current_context;
}

const struct PPB_OpenGLES2* GL_APIENTRY glGetInterfacePPAPI(void) {
  return g_gles2_interface;
}

const struct PPB_OpenGLES2InstancedArrays* GL_APIENTRY
    glGetInstancedArraysInterfacePPAPI(void) {
  return g_gles2_instanced_arrays_interface;
}

const struct PPB_OpenGLES2FramebufferBlit* GL_APIENTRY
    glGetFramebufferBlitInterfacePPAPI(void) {
  return g_gles2_framebuffer_blit_interface;
}

const struct PPB_OpenGLES2FramebufferMultisample* GL_APIENTRY
    glGetFramebufferMultisampleInterfacePPAPI(void) {
  return g_gles2_framebuffer_multisample_interface;
}

const struct PPB_OpenGLES2ChromiumEnableFeature* GL_APIENTRY
    glGetChromiumEnableFeatureInterfacePPAPI(void) {
  return g_gles2_chromium_enable_feature_interface;
}

const struct PPB_OpenGLES2ChromiumMapSub* GL_APIENTRY
    glGetChromiumMapSubInterfacePPAPI(void) {
  return g_gles2_chromium_map_sub_interface;
}

const struct PPB_OpenGLES2Query* GL_APIENTRY
    glGetQueryInterfacePPAPI(void) {
  return g_gles2_query_interface;
}

const struct PPB_OpenGLES2VertexArrayObject* GL_APIENTRY
    glGetVertexArrayObjectInterfacePPAPI(void) {
  return g_gles2_vertex_array_object_interface;
}

const struct PPB_OpenGLES2DrawBuffers_Dev* GL_APIENTRY
    glGetDrawBuffersInterfacePPAPI(void) {
  return g_gles2_draw_buffers_interface;
}
