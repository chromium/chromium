// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glProduceTextureDirectCHROMIUM(GLuint texture,
                                                           GLbyte* mailbox);
GL_APICALL GLuint GL_APIENTRY
glCreateAndConsumeTextureCHROMIUM(const GLbyte* mailbox);
#endif
typedef void (GL_APIENTRYP PFNGLGENMAILBOXCHROMIUMPROC) (GLbyte* mailbox);
typedef void (GL_APIENTRYP PFNGLPRODUCETEXTUREDIRECTCHROMIUMPROC) (
    GLuint texture, GLenum target, const GLbyte* mailbox);
typedef GLuint(GL_APIENTRYP PFNGLCREATEANDCONSUMETEXTURECHROMIUMPROC)(
    const GLbyte* mailbox);
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

/* GL_CHROMIUM_image */
#ifndef GL_CHROMIUM_image
#define GL_CHROMIUM_image 1

typedef struct _ClientBuffer* ClientBuffer;

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL GLuint GL_APIENTRY glCreateImageCHROMIUM(ClientBuffer buffer,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLenum internalformat);
GL_APICALL void GL_APIENTRY glDestroyImageCHROMIUM(GLuint image_id);
GL_APICALL void GL_APIENTRY glBindTexImage2DCHROMIUM(GLenum target,
                                                     GLint imageId);
GL_APICALL void GL_APIENTRY
glBindTexImage2DWithInternalformatCHROMIUM(GLenum target,
                                           GLenum internalformat,
                                           GLint imageId);
GL_APICALL void GL_APIENTRY glReleaseTexImage2DCHROMIUM(GLenum target,
                                                        GLint imageId);
#endif
typedef GLuint(GL_APIENTRYP PFNGLCREATEIMAGECHROMIUMPROC)(
    ClientBuffer buffer,
    GLsizei width,
    GLsizei height,
    GLenum internalformat);
typedef void (
    GL_APIENTRYP PFNGLDESTROYIMAGECHROMIUMPROC)(GLuint image_id);
typedef void(GL_APIENTRYP PFNGLBINDTEXIMAGE2DCHROMIUMPROC)(GLenum target,
                                                           GLint imageId);
typedef void(GL_APIENTRYP PFNGLBINDTEXIMAGE2DWITHINTERNALFORMATCHROMIUMPROC)(
    GLenum target,
    GLenum internalformat,
    GLint imageId);
typedef void(GL_APIENTRYP PFNGLRELEASETEXIMAGE2DCHROMIUMPROC)(GLenum target,
                                                              GLint imageId);
#endif  /* GL_CHROMIUM_image */

#ifndef GL_RGB_YCRCB_420_CHROMIUM
#define GL_RGB_YCRCB_420_CHROMIUM 0x78FA
#endif

#ifndef GL_RGB_YCBCR_422_CHROMIUM
#define GL_RGB_YCBCR_422_CHROMIUM 0x78FB
#endif

#ifndef GL_RGB_YCBCR_420V_CHROMIUM
#define GL_RGB_YCBCR_420V_CHROMIUM 0x78FC
#endif

#ifndef GL_RGB_YCBCR_P010_CHROMIUM
#define GL_RGB_YCBCR_P010_CHROMIUM 0x78FD
#endif

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

/* GL_CHROMIUM_post_sub_buffer */
#ifndef GL_CHROMIUM_post_sub_buffer
#define GL_CHROMIUM_post_sub_buffer 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glPostSubBufferCHROMIUM(GLuint64 swap_id,
                                                    GLint x,
                                                    GLint y,
                                                    GLint width,
                                                    GLint height,
                                                    GLbitfield flags);
#endif
typedef void(GL_APIENTRYP PFNGLPOSTSUBBUFFERCHROMIUMPROC)(GLuint64 swap_id,
                                                          GLint x,
                                                          GLint y,
                                                          GLint width,
                                                          GLint height,
                                                          GLbitfield flags);
#endif  /* GL_CHROMIUM_post_sub_buffer */

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

/* GL_CHROMIUM_command_buffer_latency_query */
#ifndef GL_CHROMIUM_command_buffer_latency_query
#define GL_CHROMIUM_command_buffer_latency_query 1

#ifndef GL_LATENCY_QUERY_CHROMIUM
// TODO(gman): Get official numbers for these constants.
#define GL_LATENCY_QUERY_CHROMIUM 0x6007
#endif
#endif  /* GL_CHROMIUM_command_buffer_latency_query */

/* GL_CHROMIUM_screen_space_antialiasing */
#ifndef GL_CHROMIUM_screen_space_antialiasing
#define GL_CHROMIUM_screen_space_antialiasing 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glApplyScreenSpaceAntialiasingCHROMIUM();
#endif
typedef void(GL_APIENTRYP PFNGLAPPLYSCREENSPACEANTIALIASINGCHROMIUMPROC)();
#endif /* GL_CHROMIUM_screen_space_antialiasing */

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
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glResizeCHROMIUM(GLuint width,
                                             GLuint height,
                                             GLfloat scale_factor,
                                             GLenum color_space,
                                             GLboolean alpha);

#endif
typedef void(GL_APIENTRYP PFNGLRESIZECHROMIUMPROC)(GLuint width,
                                                   GLuint height,
                                                   GLfloat scale_factor,
                                                   GLenum color_space,
                                                   GLboolean alpha);

#ifndef GL_COLOR_SPACE_UNSPECIFIED_CHROMIUM
#define GL_COLOR_SPACE_UNSPECIFIED_CHROMIUM 0x8AF1
#endif

#ifndef GL_COLOR_SPACE_SCRGB_LINEAR_CHROMIUM
#define GL_COLOR_SPACE_SCRGB_LINEAR_CHROMIUM 0x8AF2
#endif

#ifndef GL_COLOR_SPACE_SRGB_CHROMIUM
#define GL_COLOR_SPACE_SRGB_CHROMIUM 0x8AF3
#endif

#ifndef GL_COLOR_SPACE_DISPLAY_P3_CHROMIUM
#define GL_COLOR_SPACE_DISPLAY_P3_CHROMIUM 0x8AF4
#endif

#ifndef GL_COLOR_SPACE_HDR10_CHROMIUM
#define GL_COLOR_SPACE_HDR10_CHROMIUM 0x8AF5
#endif

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

/* GL_CHROMIUM_schedule_overlay_plane */
#ifndef GL_CHROMIUM_schedule_overlay_plane
#define GL_CHROMIUM_schedule_overlay_plane 1

#ifndef GL_OVERLAY_TRANSFORM_NONE_CHROMIUM
#define GL_OVERLAY_TRANSFORM_NONE_CHROMIUM 0x9245
#endif

#ifndef GL_OVERLAY_TRANSFORM_FLIP_HORIZONTAL_CHROMIUM
#define GL_OVERLAY_TRANSFORM_FLIP_HORIZONTAL_CHROMIUM 0x9246
#endif

#ifndef GL_OVERLAY_TRANSFORM_FLIP_VERTICAL_CHROMIUM
#define GL_OVERLAY_TRANSFORM_FLIP_VERTICAL_CHROMIUM 0x9247
#endif

#ifndef GL_OVERLAY_TRANSFORM_ROTATE_90_CHROMIUM
#define GL_OVERLAY_TRANSFORM_ROTATE_90_CHROMIUM 0x9248
#endif

#ifndef GL_OVERLAY_TRANSFORM_ROTATE_180_CHROMIUM
#define GL_OVERLAY_TRANSFORM_ROTATE_180_CHROMIUM 0x9249
#endif

#ifndef GL_OVERLAY_TRANSFORM_ROTATE_270_CHROMIUM
#define GL_OVERLAY_TRANSFORM_ROTATE_270_CHROMIUM 0x924A
#endif

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY
glScheduleOverlayPlaneCHROMIUM(GLint plane_z_order,
                               GLenum plane_transform,
                               GLuint overlay_texture_id,
                               GLint bounds_x,
                               GLint bounds_y,
                               GLint bounds_width,
                               GLint bounds_height,
                               GLfloat uv_x,
                               GLfloat uv_y,
                               GLfloat uv_width,
                               GLfloat uv_height,
                               GLboolean enable_blend,
                               GLuint gpu_fence_id);
#endif
typedef void(GL_APIENTRYP PFNGLSCHEDULEOVERLAYPLANECHROMIUMPROC)(
    GLint plane_z_order,
    GLenum plane_transform,
    GLuint overlay_texture_id,
    GLint bounds_x,
    GLint bounds_y,
    GLint bounds_width,
    GLint bounds_height,
    GLfloat uv_x,
    GLfloat uv_y,
    GLfloat uv_width,
    GLfloat uv_height,
    GLboolean enable_blend,
    GLuint gpu_fence_id);
#endif /* GL_CHROMIUM_schedule_overlay_plane */

#ifndef GL_CHROMIUM_schedule_ca_layer
#define GL_CHROMIUM_schedule_ca_layer 1

#ifndef GL_CA_LAYER_EDGE_LEFT_CHROMIUM
#define GL_CA_LAYER_EDGE_LEFT_CHROMIUM 0x1
#endif

#ifndef GL_CA_LAYER_EDGE_RIGHT_CHROMIUM
#define GL_CA_LAYER_EDGE_RIGHT_CHROMIUM 0x2
#endif

#ifndef GL_CA_LAYER_EDGE_BOTTOM_CHROMIUM
#define GL_CA_LAYER_EDGE_BOTTOM_CHROMIUM 0x4
#endif

#ifndef GL_CA_LAYER_EDGE_TOP_CHROMIUM
#define GL_CA_LAYER_EDGE_TOP_CHROMIUM 0x8
#endif

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY
glScheduleCALayerSharedStateCHROMIUM(GLfloat opacity,
                                     GLboolean is_clipped,
                                     const GLfloat* clip_rect,
                                     const GLfloat* rounded_corner_bounds,
                                     GLint sorting_context_id,
                                     const GLfloat* transform);
GL_APICALL void GL_APIENTRY
glScheduleCALayerCHROMIUM(GLuint contents_texture_id,
                          const GLfloat* contents_rect,
                          GLuint background_color,
                          GLuint edge_aa_mask,
                          const GLfloat* bounds_rect,
                          GLuint filter);
GL_APICALL void GL_APIENTRY
glScheduleCALayerInUseQueryCHROMIUM(GLsizei count, const GLuint* textures);
#endif
typedef void(GL_APIENTRYP PFNGLSCHEDULECALAYERSHAREDSTATECHROMIUMPROC)(
    GLfloat opacity,
    GLboolean is_clipped,
    const GLfloat* clip_rect,
    GLfloat clip_rect_corner_radius,
    GLint sorting_context_id,
    const GLfloat* transform);
typedef void(GL_APIENTRYP PFNGLSCHEDULECALAYERCHROMIUMPROC)(
    GLuint contents_texture_id,
    const GLfloat* contents_rect,
    GLuint background_color,
    GLuint edge_aa_mask,
    const GLfloat* bounds_rect,
    GLuint filter);
typedef void(GL_APIENTRYP PFNGLSCHEDULECALAYERINUSEQUERYCHROMIUMPROC)(
    GLsizei count,
    const GLuint* textures);
#endif /* GL_CHROMIUM_schedule_ca_layer */

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

#ifndef GL_CHROMIUM_path_rendering
#define GL_CHROMIUM_path_rendering 1

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY
    glMatrixLoadfCHROMIUM(GLenum mode, const GLfloat* m);
GL_APICALL void GL_APIENTRY glMatrixLoadIdentityCHROMIUM(GLenum mode);
GL_APICALL GLuint GL_APIENTRY glGenPathsCHROMIUM(GLsizei range);
GL_APICALL void GL_APIENTRY glDeletePathsCHROMIUM(GLuint path, GLsizei range);
GL_APICALL GLboolean GL_APIENTRY glIsPathCHROMIUM(GLuint path);
GL_APICALL void GL_APIENTRY glPathCommandsCHROMIUM(GLuint path,
                                                   GLsizei numCommands,
                                                   const GLubyte* commands,
                                                   GLsizei numCoords,
                                                   GLenum coordType,
                                                   const void* coords);
GL_APICALL void GL_APIENTRY
glPathParameteriCHROMIUM(GLuint path, GLenum pname, GLint value);
GL_APICALL void GL_APIENTRY
glPathParameterfCHROMIUM(GLuint path, GLenum pname, GLfloat value);
GL_APICALL void GL_APIENTRY
glPathStencilFuncCHROMIUM(GLenum func, GLint ref, GLuint mask);
GL_APICALL void GL_APIENTRY
glStencilFillPathCHROMIUM(GLuint path, GLenum fillMode, GLuint mask);
GL_APICALL void GL_APIENTRY
glStencilStrokePathCHROMIUM(GLuint path, GLint reference, GLuint mask);
GL_APICALL void GL_APIENTRY
glCoverFillPathCHROMIUM(GLuint path, GLenum coverMode);
GL_APICALL void GL_APIENTRY
glCoverStrokePathCHROMIUM(GLuint name, GLenum coverMode);
GL_APICALL void GL_APIENTRY
glStencilThenCoverFillPathCHROMIUM(GLuint path,
                                   GLenum fillMode,
                                   GLuint mask,
                                   GLenum coverMode);
GL_APICALL void GL_APIENTRY
glStencilThenCoverStrokePathCHROMIUM(GLuint path,
                                     GLint reference,
                                     GLuint mask,
                                     GLenum coverMode);

GL_APICALL void GL_APIENTRY
glStencilFillPathInstancedCHROMIUM(GLsizei numPaths,
                                   GLenum pathNameType,
                                   const GLvoid* paths,
                                   GLuint pathBase,
                                   GLenum fillMode,
                                   GLuint mask,
                                   GLenum transformType,
                                   const GLfloat* transformValues);
GL_APICALL void GL_APIENTRY
glStencilStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                     GLenum pathNameType,
                                     const GLvoid* paths,
                                     GLuint pathBase,
                                     GLint ref,
                                     GLuint mask,
                                     GLenum transformType,
                                     const GLfloat* transformValues);
GL_APICALL void GL_APIENTRY
glCoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                 GLenum pathNameType,
                                 const GLvoid* paths,
                                 GLuint pathBase,
                                 GLenum coverMode,
                                 GLenum transformType,
                                 const GLfloat* transformValues);
GL_APICALL void GL_APIENTRY
glCoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                   GLenum pathNameType,
                                   const GLvoid* paths,
                                   GLuint pathBase,
                                   GLenum coverMode,
                                   GLenum transformType,
                                   const GLfloat* transformValues);
GL_APICALL void GL_APIENTRY
glStencilThenCoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                            GLenum pathNameType,
                                            const GLvoid* paths,
                                            GLuint pathBase,
                                            GLenum fillMode,
                                            GLuint mask,
                                            GLenum coverMode,
                                            GLenum transformType,
                                            const GLfloat* transformValues);
GL_APICALL void GL_APIENTRY
glStencilThenCoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                              GLenum pathNameType,
                                              const GLvoid* paths,
                                              GLuint pathBase,
                                              GLint ref,
                                              GLuint mask,
                                              GLenum coverMode,
                                              GLenum transformType,
                                              const GLfloat* transformValues);
GL_APICALL void GL_APIENTRY
glBindFragmentInputLocationCHROMIUM(GLuint program,
                                    GLint location,
                                    const char* name);
GL_APICALL void GL_APIENTRY
glProgramPathFragmentInputGenCHROMIUM(GLuint program,
                                      GLint location,
                                      GLenum genMode,
                                      GLint components,
                                      const GLfloat* coeffs);

#endif

typedef void(GL_APIENTRYP PFNGLMATRIXLOADFCHROMIUMPROC)(GLenum matrixMode,
                                                        const GLfloat* m);
typedef void(GL_APIENTRYP PFNGLMATRIXLOADIDENTITYCHROMIUMPROC)(
    GLenum matrixMode);
typedef GLuint(GL_APIENTRYP* PFNGLGENPATHSCHROMIUMPROC)(GLsizei range);
typedef void(GL_APIENTRYP* PFNGLDELETEPATHSCHROMIUMPROC)(GLuint path,
                                                         GLsizei range);
typedef GLboolean(GL_APIENTRYP* PFNGLISPATHCHROMIUMPROC)(GLuint path);
typedef void(GL_APIENTRYP* PFNGLPATHCOMMANDSCHROMIUMPROC)(
    GLuint path,
    GLsizei numCommands,
    const GLubyte* commands,
    GLsizei numCoords,
    GLenum coordType,
    const GLvoid* coords);
typedef void(GL_APIENTRYP* PFNGLPATHPARAMETERICHROMIUMPROC)(GLuint path,
                                                            GLenum pname,
                                                            GLint value);
typedef void(GL_APIENTRYP* PFNGLPATHPARAMETERFCHROMIUMPROC)(GLuint path,
                                                            GLenum pname,
                                                            GLfloat value);
typedef void(GL_APIENTRYP* PFNGLPATHSTENCILFUNCCHROMIUMPROC)(GLenum func,
                                                             GLint ref,
                                                             GLuint mask);
typedef void(GL_APIENTRYP* PFNGLSTENCILFILLPATHCHROMIUMPROC)(GLuint path,
                                                             GLenum fillMode,
                                                             GLuint mask);
typedef void(GL_APIENTRYP* PFNGLSTENCILSTROKEPATHCHROMIUMPROC)(GLuint path,
                                                               GLint reference,
                                                               GLuint mask);
typedef void(GL_APIENTRYP* PFNGLCOVERFILLPATHCHROMIUMPROC)(GLuint path,
                                                           GLenum coverMode);
typedef void(GL_APIENTRYP* PFNGLCOVERSTROKEPATHCHROMIUMPROC)(GLuint name,
                                                             GLenum coverMode);

typedef void(GL_APIENTRYP* PFNGLSTENCILTHENCOVERFILLPATHCHROMIUMPROC)(
    GLuint path,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode);
typedef void(GL_APIENTRYP* PFNGLSTENCILTHENCOVERSTROKEPATHCHROMIUMPROC)(
    GLuint path,
    GLint reference,
    GLuint mask,
    GLenum coverMode);
typedef void(GL_APIENTRYP PFNGLSTENCILFILLPATHINSTANCEDCHROMIUMPROC)(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_APIENTRYP PFNGLSTENCILSTROKEPATHINSTANCEDCHROMIUMPROC)(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLuint pathBase,
    GLint reference,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_APIENTRYP PFNGLCOVERFILLPATHINSTANCEDCHROMIUMPROC)(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_APIENTRYP PFNGLCOVERSTROKEPATHINSTANCEDCHROMIUMPROC)(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_APIENTRYP PFNGLSTENCILTHENCOVERFILLPATHINSTANCEDCHROMIUMPROC)(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_APIENTRYP PFNGLSTENCILTHENCOVERSTROKEPATHINSTANCEDCHROMIUMPROC)(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLuint pathBase,
    GLint reference,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_APIENTRYP PFNGLBINDFRAGMENTINPUTLOCATIONCHROMIUMPROC)(
    GLuint program,
    GLint location,
    const char* name);
typedef void(GL_APIENTRYP PFNGLPROGRAMPATHFRAGMENTINPUTGENCHROMIUMPROC)(
    GLuint program,
    GLint location,
    GLenum genMode,
    GLint components,
    const GLfloat* coeffs);

#ifndef GL_CLOSE_PATH_CHROMIUM
#define GL_CLOSE_PATH_CHROMIUM 0x00
#endif
#ifndef GL_MOVE_TO_CHROMIUM
#define GL_MOVE_TO_CHROMIUM 0x02
#endif
#ifndef GL_LINE_TO_CHROMIUM
#define GL_LINE_TO_CHROMIUM 0x04
#endif
#ifndef GL_QUADRATIC_CURVE_TO_CHROMIUM
#define GL_QUADRATIC_CURVE_TO_CHROMIUM 0x0A
#endif
#ifndef GL_CUBIC_CURVE_TO_CHROMIUM
#define GL_CUBIC_CURVE_TO_CHROMIUM 0x0C
#endif
#ifndef GL_CONIC_CURVE_TO_CHROMIUM
#define GL_CONIC_CURVE_TO_CHROMIUM 0x1A
#endif
#ifndef GL_PATH_MODELVIEW_MATRIX_CHROMIUM
#define GL_PATH_MODELVIEW_MATRIX_CHROMIUM 0x0BA6
#endif
#ifndef GL_PATH_PROJECTION_MATRIX_CHROMIUM
#define GL_PATH_PROJECTION_MATRIX_CHROMIUM 0x0BA7
#endif
#ifndef GL_PATH_MODELVIEW_CHROMIUM
#define GL_PATH_MODELVIEW_CHROMIUM 0x1700
#endif
#ifndef GL_PATH_PROJECTION_CHROMIUM
#define GL_PATH_PROJECTION_CHROMIUM 0x1701
#endif
#ifndef GL_FLAT_CHROMIUM
#define GL_FLAT_CHROMIUM 0x1D00
#endif
#ifndef GL_EYE_LINEAR_CHROMIUM
#define GL_EYE_LINEAR_CHROMIUM 0x2400
#endif
#ifndef GL_OBJECT_LINEAR_CHROMIUM
#define GL_OBJECT_LINEAR_CHROMIUM 0x2401
#endif
#ifndef GL_CONSTANT_CHROMIUM
#define GL_CONSTANT_CHROMIUM 0x8576
#endif
#ifndef GL_PATH_STROKE_WIDTH_CHROMIUM
#define GL_PATH_STROKE_WIDTH_CHROMIUM 0x9075
#endif
#ifndef GL_PATH_END_CAPS_CHROMIUM
#define GL_PATH_END_CAPS_CHROMIUM 0x9076
#endif
#ifndef GL_PATH_JOIN_STYLE_CHROMIUM
#define GL_PATH_JOIN_STYLE_CHROMIUM 0x9079
#endif
#ifndef GL_PATH_MITER_LIMIT_CHROMIUM
#define GL_PATH_MITER_LIMIT_CHROMIUM 0x907a
#endif
#ifndef GL_PATH_STROKE_BOUND_CHROMIUM
#define GL_PATH_STROKE_BOUND_CHROMIUM 0x9086
#endif
#ifndef GL_COUNT_UP_CHROMIUM
#define GL_COUNT_UP_CHROMIUM 0x9088
#endif
#ifndef GL_COUNT_DOWN_CHROMIUM
#define GL_COUNT_DOWN_CHROMIUM 0x9089
#endif
#ifndef GL_CONVEX_HULL_CHROMIUM
#define GL_CONVEX_HULL_CHROMIUM 0x908B
#endif
#ifndef GL_BOUNDING_BOX_CHROMIUM
#define GL_BOUNDING_BOX_CHROMIUM 0x908D
#endif
#ifndef GL_TRANSLATE_X_CHROMIUM
#define GL_TRANSLATE_X_CHROMIUM 0x908E
#endif
#ifndef GL_TRANSLATE_Y_CHROMIUM
#define GL_TRANSLATE_Y_CHROMIUM 0x908F
#endif
#ifndef GL_TRANSLATE_2D_CHROMIUM
#define GL_TRANSLATE_2D_CHROMIUM 0x9090
#endif
#ifndef GL_TRANSLATE_3D_CHROMIUM
#define GL_TRANSLATE_3D_CHROMIUM 0x9091
#endif
#ifndef GL_AFFINE_2D_CHROMIUM
#define GL_AFFINE_2D_CHROMIUM 0x9092
#endif
#ifndef GL_AFFINE_3D_CHROMIUM
#define GL_AFFINE_3D_CHROMIUM 0x9094
#endif
#ifndef GL_TRANSPOSE_AFFINE_2D_CHROMIUM
#define GL_TRANSPOSE_AFFINE_2D_CHROMIUM 0x9096
#endif
#ifndef GL_TRANSPOSE_AFFINE_3D_CHROMIUM
#define GL_TRANSPOSE_AFFINE_3D_CHROMIUM 0x9098
#endif
#ifndef GL_SQUARE_CHROMIUM
#define GL_SQUARE_CHROMIUM 0x90a3
#endif
#ifndef GL_ROUND_CHROMIUM
#define GL_ROUND_CHROMIUM 0x90a4
#endif
#ifndef GL_ROUND_CHROMIUM
#define GL_ROUND_CHROMIUM 0x90A4
#endif
#ifndef GL_BEVEL_CHROMIUM
#define GL_BEVEL_CHROMIUM 0x90A6
#endif
#ifndef GL_MITER_REVERT_CHROMIUM
#define GL_MITER_REVERT_CHROMIUM 0x90A7
#endif
#ifndef GL_PATH_STENCIL_FUNC_CHROMIUM
#define GL_PATH_STENCIL_FUNC_CHROMIUM 0x90B7
#endif
#ifndef GL_PATH_STENCIL_REF_CHROMIUM
#define GL_PATH_STENCIL_REF_CHROMIUM 0x90B8
#endif
#ifndef GL_PATH_STENCIL_VALUE_MASK_CHROMIUM
#define GL_PATH_STENCIL_VALUE_MASK_CHROMIUM 0x90B9
#endif
#ifndef GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM
#define GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM 0x909C
#endif

#endif /* GL_CHROMIUM_path_rendering */


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

#ifndef GL_CHROMIUM_framebuffer_mixed_samples
#define GL_CHROMIUM_framebuffer_mixed_samples 1
typedef void(GL_APIENTRYP PFNGLCOVERAGEMODULATIONCHROMIUMPROC)(
    GLenum components);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glCoverageModulationCHROMIUM(GLenum components);
#endif
#define GL_COVERAGE_MODULATION_CHROMIUM 0x9332
#endif /* GL_CHROMIUM_framebuffer_mixed_samples */

#ifndef GL_ARB_occlusion_query
#define GL_ARB_occlusion_query 1
#define GL_SAMPLES_PASSED_ARB 0x8914
#endif /* GL_ARB_occlusion_query */

#ifndef GL_CHROMIUM_texture_filtering_hint
#define GL_CHROMIUM_texture_filtering_hint 1
#define GL_TEXTURE_FILTERING_HINT_CHROMIUM 0x8AF0
#endif /* GL_CHROMIUM_texture_filtering_hint */

#ifndef GL_CHROMIUM_texture_storage_image
#define GL_CHROMIUM_texture_storage_image 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glTexStorage2DImageCHROMIUM(GLenum target,
                                                        GLenum internalformat,
                                                        GLenum bufferusage,
                                                        GLsizei width,
                                                        GLsizei height);
#endif
typedef void(GL_APIENTRYP PFNGLTEXSTORAGE2DIMAGECHROMIUM)(GLenum target,
                                                          GLenum internalformat,
                                                          GLenum bufferusage,
                                                          GLsizei width,
                                                          GLsizei height);
#define GL_SCANOUT_CHROMIUM 0x6000
#endif /* GL_CHROMIUM_texture_storage_image */

#ifndef GL_CHROMIUM_color_space_metadata
#define GL_CHROMIUM_color_space_metadata 1
typedef struct _GLColorSpace* GLColorSpace;
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY
glSetColorSpaceMetadataCHROMIUM(GLuint texture_id, GLColorSpace color_space);
#endif
typedef void(GL_APIENTRYP PFNGLSETCOLORSPACEMETADATACHROMIUM)(
    GLuint texture_id,
    GLColorSpace color_space);
#endif /* GL_CHROMIUM_color_space_metadata */

/* GL_CHROMIUM_dither_and_premultiply_copy */
#ifndef GL_CHROMIUM_unpremultiply_and_dither_copy
#define GL_CHROMIUM_unpremultiply_and_dither_copy 1

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY
glUnpremultiplyAndDitherCopyCHROMIUM(GLenum source_id,
                                     GLenum dest_id,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height);
#endif
typedef void(GL_APIENTRYP PFNGLUNPREMULTIPLYANDDITHERCOPYCHROMIUMPROC)(
    GLenum source_id,
    GLenum dest_id,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height);
#endif /* GL_CHROMIUM_unpremultiply_and_dither_copy */

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
