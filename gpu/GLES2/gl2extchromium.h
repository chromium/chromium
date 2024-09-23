// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains Chromium-specific GLES2 extensions declarations.

#ifndef GPU_GLES2_GL2EXTCHROMIUM_H_
#define GPU_GLES2_GL2EXTCHROMIUM_H_

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GL_CHROMIUM_texture_mailbox */
#ifndef GL_CHROMIUM_texture_mailbox
#define GL_CHROMIUM_texture_mailbox 1

#ifndef GL_MAILBOX_SIZE_CHROMIUM
#define GL_MAILBOX_SIZE_CHROMIUM 16
#endif
#endif  /* GL_CHROMIUM_texture_mailbox */

/* GL_CHROMIUM_pixel_transfer_buffer_object */
#ifndef GL_CHROMIUM_pixel_transfer_buffer_object
#define GL_CHROMIUM_pixel_transfer_buffer_object 1

#ifndef GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM
// TODO(reveman): Get official numbers for this constants.
#define GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM 0x78EC
#define GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM 0x78ED

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void* GL_APIENTRY glMapBufferCHROMIUM(GLuint target,GLenum access);
GL_APICALL GLboolean GL_APIENTRY glUnmapBufferCHROMIUM(GLuint target);
#endif
typedef void* (GL_APIENTRY PFNGLMAPBUFFERCHROMIUM) (
    GLuint target,GLenum access);
typedef GLboolean (GL_APIENTRY PFNGLUNMAPBUFFERCHROMIUM) (GLuint target);
#endif  /* GL_CHROMIUM_pixel_transfer_buffer_object */

#ifndef GL_PIXEL_UNPACK_TRANSFER_BUFFER_BINDING_CHROMIUM
// TODO(reveman): Get official numbers for this constants.
#define GL_PIXEL_UNPACK_TRANSFER_BUFFER_BINDING_CHROMIUM 0x78EF
#define GL_PIXEL_PACK_TRANSFER_BUFFER_BINDING_CHROMIUM 0x78EE
#endif

#ifndef GL_STREAM_READ
#define GL_STREAM_READ 0x88E1
#endif
#endif  /* GL_CHROMIUM_pixel_transfer_buffer_object */

/* GL_CHROMIUM_deschedule */
#ifndef GL_CHROMIUM_deschedule
#define GL_CHROMIUM_deschedule 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glDescheduleUntilFinishedCHROMIUM();
#endif
typedef void(GL_APIENTRYP PFNGLDESCHEDULEUNTILFINISHEDCHROMIUM)();
#endif  /* GL_CHROMIUM_deschedule */

/* GL_CHROMIUM_map_sub */
#ifndef GL_CHROMIUM_map_sub
#define GL_CHROMIUM_map_sub 1

#ifndef GL_READ_ONLY
#define GL_READ_ONLY 0x88B8
#endif

#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void* GL_APIENTRY glMapBufferSubDataCHROMIUM(
    GLuint target, GLintptr offset, GLsizeiptr size, GLenum access);
GL_APICALL void GL_APIENTRY glUnmapBufferSubDataCHROMIUM(const void* mem);
GL_APICALL void* GL_APIENTRY glMapTexSubImage2DCHROMIUM(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, GLenum access);
GL_APICALL void GL_APIENTRY glUnmapTexSubImage2DCHROMIUM(const void* mem);
#endif
typedef void* (GL_APIENTRYP PFNGLMAPBUFFERSUBDATACHROMIUMPROC) (
    GLuint target, GLintptr offset, GLsizeiptr size, GLenum access);
typedef void (
    GL_APIENTRYP PFNGLUNMAPBUFFERSUBDATACHROMIUMPROC) (const void* mem);
typedef void* (GL_APIENTRYP PFNGLMAPTEXSUBIMAGE2DCHROMIUMPROC) (
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, GLenum access);
typedef void (
    GL_APIENTRYP PFNGLUNMAPTEXSUBIMAGE2DCHROMIUMPROC) (const void* mem);
#endif  /* GL_CHROMIUM_map_sub */

/* GL_CHROMIUM_request_extension */
#ifndef GL_CHROMIUM_request_extension
#define GL_CHROMIUM_request_extension 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL const GLchar* GL_APIENTRY glGetRequestableExtensionsCHROMIUM();
GL_APICALL void GL_APIENTRY glRequestExtensionCHROMIUM(const char* extension);
#endif
typedef const GLchar* (GL_APIENTRYP PFNGLGETREQUESTABLEEXTENSIONSCHROMIUMPROC) (
    );
typedef void (GL_APIENTRYP PFNGLREQUESTEXTENSIONCHROMIUMPROC) (
    const char* extension);
#endif  /* GL_CHROMIUM_request_extension */

/* GL_CHROMIUM_get_error_query */
#ifndef GL_CHROMIUM_get_error_query
#define GL_CHROMIUM_get_error_query 1

#ifndef GL_GET_ERROR_QUERY_CHROMIUM
// TODO(gman): Get official numbers for this constants.
#define GL_GET_ERROR_QUERY_CHROMIUM 0x6003
#endif
#endif  /* GL_CHROMIUM_get_error_query */

/* GL_CHROMIUM_bind_uniform_location */
#ifndef GL_CHROMIUM_bind_uniform_location
#define GL_CHROMIUM_bind_uniform_location 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glBindUniformLocationCHROMIUM(
    GLuint program, GLint location, const char* name);
#endif
typedef void (GL_APIENTRYP PFNGLBINDUNIFORMLOCATIONCHROMIUMPROC) (
    GLuint program, GLint location, const char* name);
#endif  /* GL_CHROMIUM_bind_uniform_location */

/* GL_CHROMIUM_command_buffer_query */
#ifndef GL_CHROMIUM_command_buffer_query
#define GL_CHROMIUM_command_buffer_query 1

#ifndef GL_COMMANDS_ISSUED_CHROMIUM
// TODO(andrescj): Get official numbers for these constants.
#define GL_COMMANDS_ISSUED_CHROMIUM 0x6004
#endif

#ifndef GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM
// TODO(andrescj): Get official numbers for these constants.
#define GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM 0x6005
#endif
#endif  /* GL_CHROMIUM_command_buffer_query */

/* GL_CHROMIUM_framebuffer_multisample */
#ifndef GL_CHROMIUM_framebuffer_multisample
#define GL_CHROMIUM_framebuffer_multisample 1

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glRenderbufferStorageMultisampleCHROMIUM (GLenum, GLsizei, GLenum, GLsizei, GLsizei);
GL_APICALL void GL_APIENTRY glBlitFramebufferCHROMIUM (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
#endif
typedef void (GL_APIENTRYP PFNGLRENDERBUFFERSTORAGEMULTISAMPLECHROMIUMPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP PFNGLBLITFRAMEBUFFERCHROMIUMPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

#ifndef GL_FRAMEBUFFER_BINDING_EXT
#define GL_FRAMEBUFFER_BINDING_EXT GL_FRAMEBUFFER_BINDING
#endif

#ifndef GL_DRAW_FRAMEBUFFER_BINDING_EXT
#define GL_DRAW_FRAMEBUFFER_BINDING_EXT GL_DRAW_FRAMEBUFFER_BINDING
#endif

#ifndef GL_RENDERBUFFER_BINDING_EXT
#define GL_RENDERBUFFER_BINDING_EXT GL_RENDERBUFFER_BINDING
#endif

#ifndef GL_RENDERBUFFER_SAMPLES
#define GL_RENDERBUFFER_SAMPLES 0x8CAB
#endif

#ifndef GL_READ_FRAMEBUFFER_EXT
#define GL_READ_FRAMEBUFFER_EXT GL_READ_FRAMEBUFFER
#endif

#ifndef GL_RENDERBUFFER_SAMPLES_EXT
#define GL_RENDERBUFFER_SAMPLES_EXT GL_RENDERBUFFER_SAMPLES
#endif

#ifndef GL_RENDERBUFFER_BINDING
#define GL_RENDERBUFFER_BINDING 0x8CA7
#endif

#ifndef GL_READ_FRAMEBUFFER_BINDING
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#endif

#ifndef GL_MAX_SAMPLES
#define GL_MAX_SAMPLES 0x8D57
#endif

#ifndef GL_READ_FRAMEBUFFER_BINDING_EXT
#define GL_READ_FRAMEBUFFER_BINDING_EXT GL_READ_FRAMEBUFFER_BINDING
#endif

#ifndef GL_DRAW_FRAMEBUFFER_BINDING
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#endif

#ifndef GL_MAX_SAMPLES_EXT
#define GL_MAX_SAMPLES_EXT GL_MAX_SAMPLES
#endif

#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif

#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif

#ifndef GL_DRAW_FRAMEBUFFER_EXT
#define GL_DRAW_FRAMEBUFFER_EXT GL_DRAW_FRAMEBUFFER
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE 0x8D56
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE  // NOLINT
#endif

#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
#endif  /* GL_CHROMIUM_framebuffer_multisample */

/* GL_ANGLE_texture_compression_dxt3 */
#ifndef GL_ANGLE_texture_compression_dxt3
#define GL_ANGLE_texture_compression_dxt3 1

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#endif /* GL_ANGLE_texture_compression_dxt3 */

/* GL_ANGLE_texture_compression_dxt5 */
#ifndef GL_ANGLE_texture_compression_dxt5
#define GL_ANGLE_texture_compression_dxt5 1

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#endif /* GL_ANGLE_texture_compression_dxt5 */

/* GL_ANGLE_rgbx_internal_format */
#ifndef GL_ANGLE_rgbx_internal_format
#define GL_ANGLE_rgbx_internal_format 1

#ifndef GL_RGBX8_ANGLE
#define GL_RGBX8_ANGLE 0x96BA
#endif
#endif /* GL_ANGLE_rgbx_internal_format */

/* GL_ANGLE_provoking_vertex */
#ifndef GL_ANGLE_provoking_vertex
#define GL_ANGLE_provoking_vertex 1

#ifndef GL_FIRST_VERTEX_CONVENTION_ANGLE
#define GL_FIRST_VERTEX_CONVENTION_ANGLE 0x8E4D
#endif

#ifndef GL_LAST_VERTEX_CONVENTION_ANGLE
#define GL_LAST_VERTEX_CONVENTION_ANGLE 0x8E4E
#endif

#ifndef GL_PROVOKING_VERTEX_ANGLE
#define GL_PROVOKING_VERTEX_ANGLE 0x8E4F
#endif

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glProvokingVertexANGLE(GLenum provokeMode);
#endif
typedef void(GL_APIENTRYP PFNGLPROVOKINGVERTEXANGLEPROC)(GLenum provokeMode);
#endif /* GL_ANGLE_provoking_vertex */

/* GL_ANGLE_shader_pixel_local_storage */
#ifndef GL_ANGLE_shader_pixel_local_storage
#define GL_ANGLE_shader_pixel_local_storage 1

#ifndef GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE
#define GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE 0x96E0
#endif

#ifndef GL_MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE
#define GL_MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE 0x96E1
#endif

#ifndef GL_MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE
#define GL_MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE 0x96E2
#endif

#ifndef GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE
#define GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE 0x96E3
#endif

#ifndef GL_LOAD_OP_ZERO_ANGLE
#define GL_LOAD_OP_ZERO_ANGLE 0x96E4
#endif

#ifndef GL_LOAD_OP_CLEAR_ANGLE
#define GL_LOAD_OP_CLEAR_ANGLE 0x96E5
#endif

#ifndef GL_LOAD_OP_LOAD_ANGLE
#define GL_LOAD_OP_LOAD_ANGLE 0x96E6
#endif

#ifndef GL_STORE_OP_STORE_ANGLE
#define GL_STORE_OP_STORE_ANGLE 0x96E7
#endif

#ifndef GL_PIXEL_LOCAL_FORMAT_ANGLE
#define GL_PIXEL_LOCAL_FORMAT_ANGLE 0x96E8
#endif

#ifndef GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE
#define GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE 0x96E9
#endif

#ifndef GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE
#define GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE 0x96EA
#endif

#ifndef GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE
#define GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE 0x96EB
#endif

#ifndef GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE
#define GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE 0x96EC
#endif

#ifndef GL_PIXEL_LOCAL_CLEAR_VALUE_INT_ANGLE
#define GL_PIXEL_LOCAL_CLEAR_VALUE_INT_ANGLE 0x96ED
#endif

#ifndef GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_ANGLE
#define GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_ANGLE 0x96EE
#endif

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY
glFramebufferMemorylessPixelLocalStorageANGLE(GLint plane,
                                              GLenum internalformat);
GL_APICALL void GL_APIENTRY
glFramebufferTexturePixelLocalStorageANGLE(GLint plane,
                                           GLuint backingtexture,
                                           GLint level,
                                           GLint layer);
GL_APICALL void GL_APIENTRY
glFramebufferPixelLocalClearValuefvANGLE(GLint plane, const GLfloat value[]);
GL_APICALL void GL_APIENTRY
glFramebufferPixelLocalClearValueivANGLE(GLint plane, const GLint value[]);
GL_APICALL void GL_APIENTRY
glFramebufferPixelLocalClearValueuivANGLE(GLint plane, const GLuint value[]);
GL_APICALL void GL_APIENTRY
glBeginPixelLocalStorageANGLE(GLsizei n, const GLenum loadops[]);
GL_APICALL void GL_APIENTRY
glEndPixelLocalStorageANGLE(GLsizei n, const GLenum storeops[]);
GL_APICALL void GL_APIENTRY glPixelLocalStorageBarrierANGLE(void);
GL_APICALL void GL_APIENTRY glFramebufferPixelLocalStorageInterruptANGLE(void);
GL_APICALL void GL_APIENTRY glFramebufferPixelLocalStorageRestoreANGLE(void);
GL_APICALL void GL_APIENTRY
glGetFramebufferPixelLocalStorageParameterfvANGLE(GLint plane,
                                                  GLenum pname,
                                                  GLfloat* params);
GL_APICALL void GL_APIENTRY
glGetFramebufferPixelLocalStorageParameterivANGLE(GLint plane,
                                                  GLenum pname,
                                                  GLint* params);
#endif
typedef void(GL_APIENTRYP PFNGLFRAMEBUFFERMEMORYLESSPIXELLOCALSTORAGEANGLEPROC)(
    GLint plane,
    GLenum internalformat);
typedef void(GL_APIENTRYP PFNGLFRAMEBUFFERTEXTUREPIXELLOCALSTORAGEANGLEPROC)(
    GLint plane,
    GLuint backingtexture,
    GLint level,
    GLint layer);
typedef void(GL_APIENTRYP PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEFVANGLEPROC)(
    GLint plane,
    const GLfloat value[]);
typedef void(GL_APIENTRYP PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEIVANGLEPROC)(
    GLint plane,
    const GLint value[]);
typedef void(GL_APIENTRYP PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEUIVANGLEPROC)(
    GLint plane,
    const GLuint value[]);
typedef void(GL_APIENTRYP PFNGLBEGINPIXELLOCALSTORAGEANGLEPROC)(
    GLsizei n,
    const GLenum loadops[]);
typedef void(GL_APIENTRYP PFNGLENDPIXELLOCALSTORAGEANGLEPROC)(
    GLsizei n,
    const GLenum storeops[]);
typedef void(GL_APIENTRYP PFNGLPIXELLOCALSTORAGEBARRIERANGLEPROC)(void);
typedef void(
    GL_APIENTRYP PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERFVANGLEPROC)(
    GLint plane,
    GLenum pname,
    GLfloat* params);
typedef void(
    GL_APIENTRYP PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERIVANGLEPROC)(
    GLint plane,
    GLenum pname,
    GLint* params);
#endif /* GL_ANGLE_shader_pixel_local_storage */

/* GL_ANGLE_clip_cull_distance */
#ifndef GL_ANGLE_clip_cull_distance
#define GL_ANGLE_clip_cull_distance 1

#ifndef GL_MAX_CLIP_DISTANCES_ANGLE
#define GL_MAX_CLIP_DISTANCES_ANGLE 0x0D32
#endif

#ifndef GL_MAX_CULL_DISTANCES_ANGLE
#define GL_MAX_CULL_DISTANCES_ANGLE 0x82F9
#endif

#ifndef GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES_ANGLE
#define GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES_ANGLE 0x82FA
#endif

#ifndef GL_CLIP_DISTANCE0_ANGLE
#define GL_CLIP_DISTANCE0_ANGLE 0x3000
#endif

#ifndef GL_CLIP_DISTANCE1_ANGLE
#define GL_CLIP_DISTANCE1_ANGLE 0x3001
#endif

#ifndef GL_CLIP_DISTANCE2_ANGLE
#define GL_CLIP_DISTANCE2_ANGLE 0x3002
#endif

#ifndef GL_CLIP_DISTANCE3_ANGLE
#define GL_CLIP_DISTANCE3_ANGLE 0x3003
#endif

#ifndef GL_CLIP_DISTANCE4_ANGLE
#define GL_CLIP_DISTANCE4_ANGLE 0x3004
#endif

#ifndef GL_CLIP_DISTANCE5_ANGLE
#define GL_CLIP_DISTANCE5_ANGLE 0x3005
#endif

#ifndef GL_CLIP_DISTANCE6_ANGLE
#define GL_CLIP_DISTANCE6_ANGLE 0x3006
#endif

#ifndef GL_CLIP_DISTANCE7_ANGLE
#define GL_CLIP_DISTANCE7_ANGLE 0x3007
#endif
#endif /* GL_ANGLE_clip_cull_distance */

/* GL_ANGLE_polygon_mode */
#ifndef GL_ANGLE_polygon_mode
#define GL_ANGLE_polygon_mode 1

#ifndef GL_POLYGON_MODE_ANGLE
#define GL_POLYGON_MODE_ANGLE 0x0B40
#endif

#ifndef GL_POLYGON_OFFSET_LINE_ANGLE
#define GL_POLYGON_OFFSET_LINE_ANGLE 0x2A02
#endif

#ifndef GL_LINE_ANGLE
#define GL_LINE_ANGLE 0x1B01
#endif

#ifndef GL_FILL_ANGLE
#define GL_FILL_ANGLE 0x1B02
#endif

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glPolygonModeANGLE(GLenum face, GLenum mode);
#endif
typedef void(GL_APIENTRYP PFNGLPOLYGONMODEANGLEPROC)(GLenum face, GLenum mode);
#endif /* GL_ANGLE_polygon_mode */

/* GL_ANGLE_stencil_texturing */
#ifndef GL_ANGLE_stencil_texturing
#define GL_ANGLE_stencil_texturing 1

#ifndef GL_DEPTH_STENCIL_TEXTURE_MODE_ANGLE
#define GL_DEPTH_STENCIL_TEXTURE_MODE_ANGLE 0x90EA
#endif

#ifndef GL_STENCIL_INDEX_ANGLE
#define GL_STENCIL_INDEX_ANGLE 0x1901
#endif
#endif /* GL_ANGLE_stencil_texturing */

/* GL_CHROMIUM_async_pixel_transfers */
#ifndef GL_CHROMIUM_async_pixel_transfers
#define GL_CHROMIUM_async_pixel_transfers 1

#ifndef GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM
#define GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM 0x6006
#endif
#endif  /* GL_CHROMIUM_async_pixel_transfers */

#ifndef GL_BIND_GENERATES_RESOURCE_CHROMIUM
#define GL_BIND_GENERATES_RESOURCE_CHROMIUM 0x9244
#endif

/* GL_CHROMIUM_copy_texture */
#ifndef GL_CHROMIUM_copy_texture
#define GL_CHROMIUM_copy_texture 1

#ifndef GL_UNPACK_COLORSPACE_CONVERSION_CHROMIUM
#define GL_UNPACK_COLORSPACE_CONVERSION_CHROMIUM 0x9243
#endif

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY
glCopyTextureCHROMIUM(GLenum source_id,
                      GLint source_level,
                      GLenum dest_target,
                      GLenum dest_id,
                      GLint dest_level,
                      GLint internalformat,
                      GLenum dest_type,
                      GLboolean unpack_flip_y,
                      GLboolean unpack_premultiply_alpha,
                      GLboolean unpack_unmultiply_alpha);

GL_APICALL void GL_APIENTRY
glCopySubTextureCHROMIUM(GLenum source_id,
                         GLint source_level,
                         GLenum dest_target,
                         GLenum dest_id,
                         GLint dest_level,
                         GLint xoffset,
                         GLint yoffset,
                         GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height,
                         GLboolean unpack_flip_y,
                         GLboolean unpack_premultiply_alpha,
                         GLboolean unpack_unmultiply_alpha);
#endif
typedef void(GL_APIENTRYP PFNGLCOPYTEXTURECHROMIUMPROC)(
    GLenum source_id,
    GLint source_level,
    GLenum dest_target,
    GLenum dest_id,
    GLint dest_level,
    GLint internalformat,
    GLenum dest_type,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha);

typedef void(GL_APIENTRYP PFNGLCOPYSUBTEXTURECHROMIUMPROC)(
    GLenum source_id,
    GLint source_level,
    GLenum dest_target,
    GLenum dest_id,
    GLint dest_level,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha);
#endif  /* GL_CHROMIUM_copy_texture */

/* GL_CHROMIUM_lose_context */
#ifndef GL_CHROMIUM_lose_context
#define GL_CHROMIUM_lose_context 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glLoseContextCHROMIUM(GLenum current, GLenum other);
#endif
typedef void (GL_APIENTRYP PFNGLLOSECONTEXTCHROMIUMPROC) (
    GLenum current, GLenum other);
#endif  /* GL_CHROMIUM_lose_context */

/* GL_ARB_texture_rectangle */
#ifndef GL_ARB_texture_rectangle
#define GL_ARB_texture_rectangle 1

#ifndef GL_SAMPLER_2D_RECT_ARB
#define GL_SAMPLER_2D_RECT_ARB 0x8B63
#endif

#ifndef GL_TEXTURE_BINDING_RECTANGLE_ARB
#define GL_TEXTURE_BINDING_RECTANGLE_ARB 0x84F6
#endif

#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif
#endif  /* GL_ARB_texture_rectangle */

/* GL_CHROMIUM_enable_feature */
#ifndef GL_CHROMIUM_enable_feature
#define GL_CHROMIUM_enable_feature 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL GLboolean GL_APIENTRY glEnableFeatureCHROMIUM(const char* feature);
#endif
typedef GLboolean (GL_APIENTRYP PFNGLENABLEFEATURECHROMIUMPROC) (
    const char* feature);
#endif  /* GL_CHROMIUM_enable_feature */

/* GL_ARB_robustness */
#ifndef GL_ARB_robustness
#define GL_ARB_robustness 1

#ifndef GL_GUILTY_CONTEXT_RESET_ARB
#define GL_GUILTY_CONTEXT_RESET_ARB 0x8253
#endif

#ifndef GL_UNKNOWN_CONTEXT_RESET_ARB
#define GL_UNKNOWN_CONTEXT_RESET_ARB 0x8255
#endif

#ifndef GL_INNOCENT_CONTEXT_RESET_ARB
#define GL_INNOCENT_CONTEXT_RESET_ARB 0x8254
#endif
#endif  /* GL_ARB_robustness */

/* GL_EXT_framebuffer_blit */
#ifndef GL_EXT_framebuffer_blit
#define GL_EXT_framebuffer_blit 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glBlitFramebufferEXT(
    GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
    GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
#endif
typedef void (GL_APIENTRYP PFNGLBLITFRAMEBUFFEREXTPROC) (
    GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
    GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
#endif  /* GL_EXT_framebuffer_blit */

/* GL_EXT_draw_buffers */
#ifndef GL_EXT_draw_buffers
#define GL_EXT_draw_buffers 1

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glDrawBuffersEXT(
    GLsizei n, const GLenum* bufs);
#endif
typedef void (GL_APIENTRYP PFNGLDRAWBUFFERSEXTPROC) (
    GLsizei n, const GLenum* bufs);

#ifndef GL_COLOR_ATTACHMENT0_EXT
#define GL_COLOR_ATTACHMENT0_EXT 0x8CE0
#endif
#ifndef GL_COLOR_ATTACHMENT1_EXT
#define GL_COLOR_ATTACHMENT1_EXT 0x8CE1
#endif
#ifndef GL_COLOR_ATTACHMENT2_EXT
#define GL_COLOR_ATTACHMENT2_EXT 0x8CE2
#endif
#ifndef GL_COLOR_ATTACHMENT3_EXT
#define GL_COLOR_ATTACHMENT3_EXT 0x8CE3
#endif
#ifndef GL_COLOR_ATTACHMENT4_EXT
#define GL_COLOR_ATTACHMENT4_EXT 0x8CE4
#endif
#ifndef GL_COLOR_ATTACHMENT5_EXT
#define GL_COLOR_ATTACHMENT5_EXT 0x8CE5
#endif
#ifndef GL_COLOR_ATTACHMENT6_EXT
#define GL_COLOR_ATTACHMENT6_EXT 0x8CE6
#endif
#ifndef GL_COLOR_ATTACHMENT7_EXT
#define GL_COLOR_ATTACHMENT7_EXT 0x8CE7
#endif
#ifndef GL_COLOR_ATTACHMENT8_EXT
#define GL_COLOR_ATTACHMENT8_EXT 0x8CE8
#endif
#ifndef GL_COLOR_ATTACHMENT9_EXT
#define GL_COLOR_ATTACHMENT9_EXT 0x8CE9
#endif
#ifndef GL_COLOR_ATTACHMENT10_EXT
#define GL_COLOR_ATTACHMENT10_EXT 0x8CEA
#endif
#ifndef GL_COLOR_ATTACHMENT11_EXT
#define GL_COLOR_ATTACHMENT11_EXT 0x8CEB
#endif
#ifndef GL_COLOR_ATTACHMENT12_EXT
#define GL_COLOR_ATTACHMENT12_EXT 0x8CEC
#endif
#ifndef GL_COLOR_ATTACHMENT13_EXT
#define GL_COLOR_ATTACHMENT13_EXT 0x8CED
#endif
#ifndef GL_COLOR_ATTACHMENT14_EXT
#define GL_COLOR_ATTACHMENT14_EXT 0x8CEE
#endif
#ifndef GL_COLOR_ATTACHMENT15_EXT
#define GL_COLOR_ATTACHMENT15_EXT 0x8CEF
#endif

#ifndef GL_DRAW_BUFFER0_EXT
#define GL_DRAW_BUFFER0_EXT 0x8825
#endif
#ifndef GL_DRAW_BUFFER1_EXT
#define GL_DRAW_BUFFER1_EXT 0x8826
#endif
#ifndef GL_DRAW_BUFFER2_EXT
#define GL_DRAW_BUFFER2_EXT 0x8827
#endif
#ifndef GL_DRAW_BUFFER3_EXT
#define GL_DRAW_BUFFER3_EXT 0x8828
#endif
#ifndef GL_DRAW_BUFFER4_EXT
#define GL_DRAW_BUFFER4_EXT 0x8829
#endif
#ifndef GL_DRAW_BUFFER5_EXT
#define GL_DRAW_BUFFER5_EXT 0x882A
#endif
#ifndef GL_DRAW_BUFFER6_EXT
#define GL_DRAW_BUFFER6_EXT 0x882B
#endif
#ifndef GL_DRAW_BUFFER7_EXT
#define GL_DRAW_BUFFER7_EXT 0x882C
#endif
#ifndef GL_DRAW_BUFFER8_EXT
#define GL_DRAW_BUFFER8_EXT 0x882D
#endif
#ifndef GL_DRAW_BUFFER9_EXT
#define GL_DRAW_BUFFER9_EXT 0x882E
#endif
#ifndef GL_DRAW_BUFFER10_EXT
#define GL_DRAW_BUFFER10_EXT 0x882F
#endif
#ifndef GL_DRAW_BUFFER11_EXT
#define GL_DRAW_BUFFER11_EXT 0x8830
#endif
#ifndef GL_DRAW_BUFFER12_EXT
#define GL_DRAW_BUFFER12_EXT 0x8831
#endif
#ifndef GL_DRAW_BUFFER13_EXT
#define GL_DRAW_BUFFER13_EXT 0x8832
#endif
#ifndef GL_DRAW_BUFFER14_EXT
#define GL_DRAW_BUFFER14_EXT 0x8833
#endif
#ifndef GL_DRAW_BUFFER15_EXT
#define GL_DRAW_BUFFER15_EXT 0x8834
#endif

#ifndef GL_MAX_COLOR_ATTACHMENTS_EXT
#define GL_MAX_COLOR_ATTACHMENTS_EXT 0x8CDF
#endif

#ifndef GL_MAX_DRAW_BUFFERS_EXT
#define GL_MAX_DRAW_BUFFERS_EXT 0x8824
#endif

#endif  /* GL_EXT_draw_buffers */

/* GL_CHROMIUM_resize */
#ifndef GL_CHROMIUM_resize
#define GL_CHROMIUM_resize 1
typedef const struct _GLcolorSpace* GLcolorSpace;
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glResizeCHROMIUM(GLuint width,
                                             GLuint height,
                                             GLfloat scale_factor,
                                             GLcolorSpace color_space,
                                             GLboolean alpha);

#endif
typedef void(GL_APIENTRYP PFNGLRESIZECHROMIUMPROC)(GLuint width,
                                                   GLuint height,
                                                   GLfloat scale_factor,
                                                   GLcolorSpace color_space,
                                                   GLboolean alpha);
#endif  /* GL_CHROMIUM_resize */

/* GL_CHROMIUM_get_multiple */
#ifndef GL_CHROMIUM_get_multiple
#define GL_CHROMIUM_get_multiple 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glGetProgramInfoCHROMIUM(
    GLuint program, GLsizei bufsize, GLsizei* size, void* info);
#endif
typedef void (GL_APIENTRYP PFNGLGETPROGRAMINFOCHROMIUMPROC) (
   GLuint program, GLsizei bufsize, GLsizei* size, void* info);
#endif  /* GL_CHROMIUM_get_multiple */

/* GL_CHROMIUM_sync_point */
#ifndef GL_CHROMIUM_sync_point
#define GL_CHROMIUM_sync_point 1

#ifndef GL_SYNC_TOKEN_SIZE_CHROMIUM
#define GL_SYNC_TOKEN_SIZE_CHROMIUM 24
#endif

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glGenSyncTokenCHROMIUM(GLbyte* sync_token);
GL_APICALL void GL_APIENTRY
glGenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token);
GL_APICALL void GL_APIENTRY glVerifySyncTokensCHROMIUM(GLbyte **sync_tokens,
                                                       GLsizei count);
GL_APICALL void GL_APIENTRY glWaitSyncTokenCHROMIUM(const GLbyte* sync_token);
#endif
typedef void(GL_APIENTRYP PFNGLGENSYNCTOKENCHROMIUMPROC)(GLbyte* sync_token);
typedef void(GL_APIENTRYP PFNGLGENUNVERIFIEDSYNCTOKENCHROMIUMPROC)(
    GLbyte* sync_token);
typedef void (GL_APIENTRYP PFNGLVERIFYSYNCTOKENSCHROMIUMPROC) (
    GLbyte **sync_tokens, GLsizei count);
typedef void (GL_APIENTRYP PFNGLWAITSYNCTOKENCHROMIUM) (
    const GLbyte* sync_tokens);
#endif  /* GL_CHROMIUM_sync_point */

#ifndef GL_CHROMIUM_color_buffer_float_rgba
#define GL_CHROMIUM_color_buffer_float_rgba 1
#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif
#endif /* GL_CHROMIUM_color_buffer_float_rgba */

#ifndef GL_CHROMIUM_color_buffer_float_rgb
#define GL_CHROMIUM_color_buffer_float_rgb 1
#ifndef GL_RGB32F
#define GL_RGB32F 0x8815
#endif
#endif /* GL_CHROMIUM_color_buffer_float_rgb */

/* GL_CHROMIUM_sync_query */
#ifndef GL_CHROMIUM_sync_query
#define GL_CHROMIUM_sync_query 1

#ifndef GL_COMMANDS_COMPLETED_CHROMIUM
#define GL_COMMANDS_COMPLETED_CHROMIUM 0x84F7
#endif
#endif  /* GL_CHROMIUM_sync_query */

/* GL_CHROMIUM_nonblocking_readback */
#ifndef GL_CHROMIUM_nonblocking_readback
#define GL_CHROMIUM_nonblocking_readback 1

#ifndef GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM
#define GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM 0x84F8
#endif
#endif /* GL_CHROMIUM_nonblocking_readback */

#ifndef GL_EXT_multisample_compatibility
#define GL_EXT_multisample_compatibility 1
#define GL_MULTISAMPLE_EXT 0x809D
#define GL_SAMPLE_ALPHA_TO_ONE_EXT 0x809F
#endif /* GL_EXT_multisample_compatiblity */

#ifndef GL_EXT_blend_func_extended
#define GL_EXT_blend_func_extended 1

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glBindFragDataLocationIndexedEXT(GLuint program,
                                                             GLuint colorNumber,
                                                             GLuint index,
                                                             const char* name);
GL_APICALL void GL_APIENTRY glBindFragDataLocationEXT(GLuint program,
                                                      GLuint colorNumber,
                                                      const char* name);
GL_APICALL GLint GL_APIENTRY glGetFragDataIndexEXT(GLuint program,
                                                   const char* name);
#endif

typedef void(GL_APIENTRYP PFNGLBINDFRAGDATALOCATIONINDEXEDEXT)(
    GLuint program,
    GLuint colorNumber,
    GLuint index,
    const char* name);
typedef void(GL_APIENTRYP PFNGLBINDFRAGDATALOCATIONEXT)(GLuint program,
                                                        GLuint colorNumber,
                                                        const char* name);
typedef GLint(GL_APIENTRYP PFNGLGETFRAGDATAINDEXEXT)(GLuint program,
                                                     const GLchar* name);

#define GL_SRC_ALPHA_SATURATE_EXT 0x0308
#define GL_SRC1_ALPHA_EXT 0x8589  // OpenGL 1.5 token value
#define GL_SRC1_COLOR_EXT 0x88F9
#define GL_ONE_MINUS_SRC1_COLOR_EXT 0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA_EXT 0x88FB
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT 0x88FC
#endif /* GL_EXT_blend_func_extended */

#ifndef GL_ARB_occlusion_query
#define GL_ARB_occlusion_query 1
#define GL_SAMPLES_PASSED_ARB 0x8914
#endif /* GL_ARB_occlusion_query */

#ifndef GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT
#define GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT 0x8868
#endif

/* GL_CHROMIUM_shared_image */
#ifndef GL_CHROMIUM_shared_image
#define GL_CHROMIUM_shared_image 1
#define GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM 0x8AF6
#define GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM 0x8AF7
#endif /* GL_CHROMIUM_shared_image */

/* GL_CHROMIUM_program_completion_query */
#ifndef GL_CHROMIUM_program_completion_query
#define GL_CHROMIUM_program_completion_query 1

#ifndef GL_PROGRAM_COMPLETION_QUERY_CHROMIUM
// TODO(jie.a.chen@intel.com): Get official numbers for this constants.
#define GL_PROGRAM_COMPLETION_QUERY_CHROMIUM 0x6009
#endif
#endif /* GL_CHROMIUM_program_completion_query */

#ifdef __cplusplus
}
#endif

#endif  // GPU_GLES2_GL2EXTCHROMIUM_H_
