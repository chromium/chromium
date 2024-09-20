#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""code generator for GLES2 command buffers."""

import filecmp
import os
import sys
from optparse import OptionParser

import build_cmd_buffer_lib

# Additional space required after "type" here and elsewhere because otherwise
# pylint detects "# type:" as invalid syntax on Python 3.8, see
# https://github.com/PyCQA/pylint/issues/3556.
# Named type info object represents a named type that is used in OpenGL call
# arguments.  Each named type defines a set of valid OpenGL call arguments.  The
# named types are used in 'gles2_cmd_buffer_functions.txt'.
# type : The actual GL type of the named type.
# valid: The list of values that are valid for both the client and the service.
# valid_es3: The list of values that are valid in OpenGL ES 3, but not ES 2.
# invalid: Examples of invalid values for the type. At least these values
#          should be tested to be invalid.
# deprecated_es3: The list of values that are valid in OpenGL ES 2, but
#                 deprecated in ES 3.
# is_complete: The list of valid values of type are final and will not be
#              modified during runtime.
# validator: If set to False will prevent creation of a ValueValidator. Values
#            are still expected to be checked for validity and will be tested.
_NAMED_TYPE_INFO = {
  'BlitFilter': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_NEAREST',
      'GL_LINEAR',
    ],
    'invalid': [
      'GL_LINEAR_MIPMAP_LINEAR',
    ],
  },
  'FramebufferTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_FRAMEBUFFER',
    ],
    'valid_es3': [
      'GL_DRAW_FRAMEBUFFER' ,
      'GL_READ_FRAMEBUFFER' ,
    ],
    'invalid': [
      'GL_RENDERBUFFER',
    ],
  },
  'RenderBufferTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_RENDERBUFFER',
    ],
    'invalid': [
      'GL_FRAMEBUFFER',
    ],
  },
  'BufferTarget': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_ARRAY_BUFFER',
      'GL_ELEMENT_ARRAY_BUFFER',
    ],
    'valid_es3': [
      'GL_COPY_READ_BUFFER',
      'GL_COPY_WRITE_BUFFER',
      'GL_PIXEL_PACK_BUFFER',
      'GL_PIXEL_UNPACK_BUFFER',
      'GL_TRANSFORM_FEEDBACK_BUFFER',
      'GL_UNIFORM_BUFFER',
    ],
    'invalid': [
      'GL_RENDERBUFFER',
    ],
  },
  'IndexedBufferTarget': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_TRANSFORM_FEEDBACK_BUFFER',
      'GL_UNIFORM_BUFFER',
    ],
    'invalid': [
      'GL_RENDERBUFFER',
    ],
  },
  'MapBufferAccess': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_MAP_READ_BIT',
      'GL_MAP_WRITE_BIT',
      'GL_MAP_INVALIDATE_RANGE_BIT',
      'GL_MAP_INVALIDATE_BUFFER_BIT',
      'GL_MAP_FLUSH_EXPLICIT_BIT',
      'GL_MAP_UNSYNCHRONIZED_BIT',
    ],
    'invalid': [
      'GL_SYNC_FLUSH_COMMANDS_BIT',
    ],
  },
  'Bufferiv': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_COLOR',
      'GL_STENCIL',
    ],
    'invalid': [
      'GL_RENDERBUFFER',
    ],
  },
  'Bufferuiv': {
    'type': 'GLenum',
    'valid': [
      'GL_COLOR',
    ],
    'invalid': [
      'GL_RENDERBUFFER',
    ],
  },
  'Bufferfv': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_COLOR',
      'GL_DEPTH',
    ],
    'invalid': [
      'GL_RENDERBUFFER',
    ],
  },
  'Bufferfi': {
    'type': 'GLenum',
    'valid': [
      'GL_DEPTH_STENCIL',
    ],
    'invalid': [
      'GL_RENDERBUFFER',
    ],
  },
  'BufferUsage': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_STREAM_DRAW',
      'GL_STATIC_DRAW',
      'GL_DYNAMIC_DRAW',
    ],
    'valid_es3': [
      'GL_STREAM_READ',
      'GL_STREAM_COPY',
      'GL_STATIC_READ',
      'GL_STATIC_COPY',
      'GL_DYNAMIC_READ',
      'GL_DYNAMIC_COPY',
    ],
    'invalid': [
      'GL_NONE',
    ],
  },
  'CompressedTextureFormat': {
    'type': 'GLenum',
    'valid': [
    ],
    'valid_es3': [
    ],
  },
  'GLState': {
    'type': 'GLenum',
    'valid': [
      # NOTE: State an Capability entries added later.
      'GL_ACTIVE_TEXTURE',
      'GL_ALIASED_LINE_WIDTH_RANGE',
      'GL_ALIASED_POINT_SIZE_RANGE',
      'GL_ALPHA_BITS',
      'GL_ARRAY_BUFFER_BINDING',
      'GL_BLUE_BITS',
      'GL_COMPRESSED_TEXTURE_FORMATS',
      'GL_CURRENT_PROGRAM',
      'GL_DEPTH_BITS',
      'GL_DEPTH_RANGE',
      'GL_ELEMENT_ARRAY_BUFFER_BINDING',
      'GL_FRAMEBUFFER_BINDING',
      'GL_GENERATE_MIPMAP_HINT',
      'GL_GREEN_BITS',
      'GL_IMPLEMENTATION_COLOR_READ_FORMAT',
      'GL_IMPLEMENTATION_COLOR_READ_TYPE',
      'GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS',
      'GL_MAX_CUBE_MAP_TEXTURE_SIZE',
      'GL_MAX_FRAGMENT_UNIFORM_VECTORS',
      'GL_MAX_RENDERBUFFER_SIZE',
      'GL_MAX_TEXTURE_IMAGE_UNITS',
      'GL_MAX_TEXTURE_SIZE',
      'GL_MAX_VARYING_VECTORS',
      'GL_MAX_VERTEX_ATTRIBS',
      'GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS',
      'GL_MAX_VERTEX_UNIFORM_VECTORS',
      'GL_MAX_VIEWPORT_DIMS',
      'GL_NUM_COMPRESSED_TEXTURE_FORMATS',
      'GL_NUM_SHADER_BINARY_FORMATS',
      'GL_PACK_ALIGNMENT',
      'GL_RED_BITS',
      'GL_RENDERBUFFER_BINDING',
      'GL_SAMPLE_BUFFERS',
      'GL_SAMPLE_COVERAGE_INVERT',
      'GL_SAMPLE_COVERAGE_VALUE',
      'GL_SAMPLES',
      'GL_SCISSOR_BOX',
      'GL_SHADER_BINARY_FORMATS',
      'GL_SHADER_COMPILER',
      'GL_SUBPIXEL_BITS',
      'GL_STENCIL_BITS',
      'GL_TEXTURE_BINDING_2D',
      'GL_TEXTURE_BINDING_CUBE_MAP',
      'GL_UNPACK_ALIGNMENT',
      'GL_BIND_GENERATES_RESOURCE_CHROMIUM',
      # we can add this because we emulate it if the driver does not support it.
      'GL_VERTEX_ARRAY_BINDING_OES',
      'GL_VIEWPORT',
    ],
    'valid_es3': [
      'GL_COPY_READ_BUFFER_BINDING',
      'GL_COPY_WRITE_BUFFER_BINDING',
      'GL_DRAW_BUFFER0',
      'GL_DRAW_BUFFER1',
      'GL_DRAW_BUFFER2',
      'GL_DRAW_BUFFER3',
      'GL_DRAW_BUFFER4',
      'GL_DRAW_BUFFER5',
      'GL_DRAW_BUFFER6',
      'GL_DRAW_BUFFER7',
      'GL_DRAW_BUFFER8',
      'GL_DRAW_BUFFER9',
      'GL_DRAW_BUFFER10',
      'GL_DRAW_BUFFER11',
      'GL_DRAW_BUFFER12',
      'GL_DRAW_BUFFER13',
      'GL_DRAW_BUFFER14',
      'GL_DRAW_BUFFER15',
      'GL_DRAW_FRAMEBUFFER_BINDING',
      'GL_FRAGMENT_SHADER_DERIVATIVE_HINT',
      'GL_GPU_DISJOINT_EXT',
      'GL_MAJOR_VERSION',
      'GL_MAX_3D_TEXTURE_SIZE',
      'GL_MAX_ARRAY_TEXTURE_LAYERS',
      'GL_MAX_COLOR_ATTACHMENTS',
      'GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS',
      'GL_MAX_COMBINED_UNIFORM_BLOCKS',
      'GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS',
      'GL_MAX_DRAW_BUFFERS',
      'GL_MAX_ELEMENT_INDEX',
      'GL_MAX_ELEMENTS_INDICES',
      'GL_MAX_ELEMENTS_VERTICES',
      'GL_MAX_FRAGMENT_INPUT_COMPONENTS',
      'GL_MAX_FRAGMENT_UNIFORM_BLOCKS',
      'GL_MAX_FRAGMENT_UNIFORM_COMPONENTS',
      'GL_MAX_PROGRAM_TEXEL_OFFSET',
      'GL_MAX_SAMPLES',
      'GL_MAX_SERVER_WAIT_TIMEOUT',
      'GL_MAX_TEXTURE_LOD_BIAS',
      'GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS',
      'GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS',
      'GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS',
      'GL_MAX_UNIFORM_BLOCK_SIZE',
      'GL_MAX_UNIFORM_BUFFER_BINDINGS',
      'GL_MAX_VARYING_COMPONENTS',
      'GL_MAX_VERTEX_OUTPUT_COMPONENTS',
      'GL_MAX_VERTEX_UNIFORM_BLOCKS',
      'GL_MAX_VERTEX_UNIFORM_COMPONENTS',
      'GL_MIN_PROGRAM_TEXEL_OFFSET',
      'GL_MINOR_VERSION',
      'GL_NUM_EXTENSIONS',
      'GL_NUM_PROGRAM_BINARY_FORMATS',
      'GL_PACK_ROW_LENGTH',
      'GL_PACK_SKIP_PIXELS',
      'GL_PACK_SKIP_ROWS',
      'GL_PIXEL_PACK_BUFFER_BINDING',
      'GL_PIXEL_UNPACK_BUFFER_BINDING',
      'GL_PROGRAM_BINARY_FORMATS',
      'GL_READ_BUFFER',
      'GL_READ_FRAMEBUFFER_BINDING',
      'GL_SAMPLER_BINDING',
      'GL_TIMESTAMP_EXT',
      'GL_TEXTURE_BINDING_2D_ARRAY',
      'GL_TEXTURE_BINDING_3D',
      'GL_TRANSFORM_FEEDBACK_BINDING',
      'GL_TRANSFORM_FEEDBACK_ACTIVE',
      'GL_TRANSFORM_FEEDBACK_BUFFER_BINDING',
      'GL_TRANSFORM_FEEDBACK_PAUSED',
      'GL_UNIFORM_BUFFER_BINDING',
      'GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT',
      'GL_UNPACK_IMAGE_HEIGHT',
      'GL_UNPACK_ROW_LENGTH',
      'GL_UNPACK_SKIP_IMAGES',
      'GL_UNPACK_SKIP_PIXELS',
      'GL_UNPACK_SKIP_ROWS',
      'GL_BLEND_EQUATION_RGB',
      'GL_BLEND_EQUATION_ALPHA',
      'GL_BLEND_SRC_RGB',
      'GL_BLEND_SRC_ALPHA',
      'GL_BLEND_DST_RGB',
      'GL_BLEND_DST_ALPHA',
      'GL_COLOR_WRITEMASK',
      # GL_VERTEX_ARRAY_BINDING is the same as GL_VERTEX_ARRAY_BINDING_OES
      # 'GL_VERTEX_ARRAY_BINDING',
    ],
    'invalid': [
      'GL_FOG_HINT',
    ],
  },
  'IndexedGLState': {
    'type': 'GLenum',
    'valid': [
      'GL_TRANSFORM_FEEDBACK_BUFFER_BINDING',
      'GL_TRANSFORM_FEEDBACK_BUFFER_SIZE',
      'GL_TRANSFORM_FEEDBACK_BUFFER_START',
      'GL_UNIFORM_BUFFER_BINDING',
      'GL_UNIFORM_BUFFER_SIZE',
      'GL_UNIFORM_BUFFER_START',
      'GL_BLEND_EQUATION_RGB',
      'GL_BLEND_EQUATION_ALPHA',
      'GL_BLEND_SRC_RGB',
      'GL_BLEND_SRC_ALPHA',
      'GL_BLEND_DST_RGB',
      'GL_BLEND_DST_ALPHA',
      'GL_COLOR_WRITEMASK',
    ],
    'invalid': [
      'GL_FOG_HINT',
    ],
  },
  'GetTexParamTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_2D',
      'GL_TEXTURE_CUBE_MAP',
    ],
    'valid_es3': [
      'GL_TEXTURE_2D_ARRAY',
      'GL_TEXTURE_3D',
    ],
    'invalid': [
      'GL_PROXY_TEXTURE_CUBE_MAP',
    ]
  },
  'ReadBuffer': {
    'type': 'GLenum',
    'valid': [
      'GL_NONE',
      'GL_BACK',
      'GL_COLOR_ATTACHMENT0',
      'GL_COLOR_ATTACHMENT1',
      'GL_COLOR_ATTACHMENT2',
      'GL_COLOR_ATTACHMENT3',
      'GL_COLOR_ATTACHMENT4',
      'GL_COLOR_ATTACHMENT5',
      'GL_COLOR_ATTACHMENT6',
      'GL_COLOR_ATTACHMENT7',
      'GL_COLOR_ATTACHMENT8',
      'GL_COLOR_ATTACHMENT9',
      'GL_COLOR_ATTACHMENT10',
      'GL_COLOR_ATTACHMENT11',
      'GL_COLOR_ATTACHMENT12',
      'GL_COLOR_ATTACHMENT13',
      'GL_COLOR_ATTACHMENT14',
      'GL_COLOR_ATTACHMENT15',
    ],
    'invalid': [
      'GL_RENDERBUFFER',
    ]
  },
  'TextureTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_2D',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_X',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_X',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_Y',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_Y',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_Z',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_Z',
    ],
    'invalid': [
      'GL_PROXY_TEXTURE_CUBE_MAP',
    ]
  },
  'TextureFboTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_2D',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_X',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_X',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_Y',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_Y',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_Z',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_Z',
    ],
    'invalid': [
      'GL_PROXY_TEXTURE_CUBE_MAP',
    ]
  },
  'Texture3DTarget': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_TEXTURE_3D',
      'GL_TEXTURE_2D_ARRAY',
    ],
    'invalid': [
      'GL_TEXTURE_2D',
    ]
  },
  'TextureBindTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_2D',
      'GL_TEXTURE_CUBE_MAP',
    ],
    'valid_es3': [
      'GL_TEXTURE_3D',
      'GL_TEXTURE_2D_ARRAY',
    ],
    'invalid': [
      'GL_TEXTURE_1D',
      'GL_TEXTURE_3D',
    ],
  },
  'TransformFeedbackBindTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_TRANSFORM_FEEDBACK',
    ],
    'invalid': [
      'GL_TEXTURE_2D',
    ],
  },
  'TransformFeedbackPrimitiveMode': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_POINTS',
      'GL_LINES',
      'GL_TRIANGLES',
    ],
    'invalid': [
      'GL_LINE_LOOP',
    ],
  },
  'ShaderType': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_VERTEX_SHADER',
      'GL_FRAGMENT_SHADER',
    ],
    'invalid': [
      'GL_GEOMETRY_SHADER',
    ],
  },
  'FaceType': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_FRONT',
      'GL_BACK',
      'GL_FRONT_AND_BACK',
    ],
  },
  'FaceMode': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_CW',
      'GL_CCW',
    ],
  },
  'CmpFunction': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_NEVER',
      'GL_LESS',
      'GL_EQUAL',
      'GL_LEQUAL',
      'GL_GREATER',
      'GL_NOTEQUAL',
      'GL_GEQUAL',
      'GL_ALWAYS',
    ],
  },
  'Equation': {
    'type': 'GLenum',
    'valid': [
      'GL_FUNC_ADD',
      'GL_FUNC_SUBTRACT',
      'GL_FUNC_REVERSE_SUBTRACT',
    ],
    'valid_es3': [
      'GL_MIN',
      'GL_MAX',
    ],
    'invalid': [
      'GL_NONE',
    ],
  },
  'SrcBlendFactor': {
    'type': 'GLenum',
    'valid': [
      'GL_ZERO',
      'GL_ONE',
      'GL_SRC_COLOR',
      'GL_ONE_MINUS_SRC_COLOR',
      'GL_DST_COLOR',
      'GL_ONE_MINUS_DST_COLOR',
      'GL_SRC_ALPHA',
      'GL_ONE_MINUS_SRC_ALPHA',
      'GL_DST_ALPHA',
      'GL_ONE_MINUS_DST_ALPHA',
      'GL_CONSTANT_COLOR',
      'GL_ONE_MINUS_CONSTANT_COLOR',
      'GL_CONSTANT_ALPHA',
      'GL_ONE_MINUS_CONSTANT_ALPHA',
      'GL_SRC_ALPHA_SATURATE',
    ],
  },
  'DstBlendFactor': {
    'type': 'GLenum',
    'valid': [
      'GL_ZERO',
      'GL_ONE',
      'GL_SRC_COLOR',
      'GL_ONE_MINUS_SRC_COLOR',
      'GL_DST_COLOR',
      'GL_ONE_MINUS_DST_COLOR',
      'GL_SRC_ALPHA',
      'GL_ONE_MINUS_SRC_ALPHA',
      'GL_DST_ALPHA',
      'GL_ONE_MINUS_DST_ALPHA',
      'GL_CONSTANT_COLOR',
      'GL_ONE_MINUS_CONSTANT_COLOR',
      'GL_CONSTANT_ALPHA',
      'GL_ONE_MINUS_CONSTANT_ALPHA',
    ],
    'valid_es3': [
      'GL_SRC_ALPHA_SATURATE'
    ]
  },
  'Capability': {
    'type': 'GLenum',
    'valid': ["GL_%s" % cap['name'].upper()
              for cap in build_cmd_buffer_lib._CAPABILITY_FLAGS
              if ('es3' not in cap or cap['es3'] != True)
              and 'extension_flag' not in cap],
    'valid_es3': ["GL_%s" % cap['name'].upper()
                  for cap in build_cmd_buffer_lib._CAPABILITY_FLAGS
                  if ('es3' in cap and cap['es3'] == True)
                  and 'extension_flag' not in cap],
    'invalid': [
      'GL_CLIP_PLANE0',
      'GL_POINT_SPRITE',
    ],
  },
  'DrawMode': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_POINTS',
      'GL_LINE_STRIP',
      'GL_LINE_LOOP',
      'GL_LINES',
      'GL_TRIANGLE_STRIP',
      'GL_TRIANGLE_FAN',
      'GL_TRIANGLES',
    ],
    'invalid': [
      'GL_QUADS',
      'GL_POLYGON',
    ],
  },
  'IndexType': {
    'type': 'GLenum',
    'valid': [
      'GL_UNSIGNED_BYTE',
      'GL_UNSIGNED_SHORT',
    ],
    'valid_es3': [
      'GL_UNSIGNED_INT',
    ],
    'invalid': [
      'GL_INT',
    ],
  },
  'GetMaxIndexType': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_UNSIGNED_BYTE',
      'GL_UNSIGNED_SHORT',
      'GL_UNSIGNED_INT',
    ],
    'invalid': [
      'GL_INT',
    ],
  },
  'Attachment': {
    'type': 'GLenum',
    'valid': [
      'GL_COLOR_ATTACHMENT0',
      'GL_DEPTH_ATTACHMENT',
      'GL_STENCIL_ATTACHMENT',
    ],
    'valid_es3': [
      'GL_DEPTH_STENCIL_ATTACHMENT',
    ],
  },
  'AttachmentQuery': {
    'type': 'GLenum',
    'valid': [
      'GL_COLOR_ATTACHMENT0',
      'GL_DEPTH_ATTACHMENT',
      'GL_STENCIL_ATTACHMENT',
    ],
    'valid_es3': [
      'GL_DEPTH_STENCIL_ATTACHMENT',
      # For backbuffer.
      'GL_COLOR_EXT',
      'GL_DEPTH_EXT',
      'GL_STENCIL_EXT',
    ],
  },
  'BackbufferAttachment': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_COLOR_EXT',
      'GL_DEPTH_EXT',
      'GL_STENCIL_EXT',
    ],
  },
  'BufferParameter': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_BUFFER_SIZE',
      'GL_BUFFER_USAGE',
    ],
    'valid_es3': [
      'GL_BUFFER_ACCESS_FLAGS',
      'GL_BUFFER_MAPPED',
    ],
    'invalid': [
      'GL_PIXEL_PACK_BUFFER',
    ],
  },
  'BufferParameter64': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_BUFFER_SIZE',
      'GL_BUFFER_MAP_LENGTH',
      'GL_BUFFER_MAP_OFFSET',
    ],
    'invalid': [
      'GL_PIXEL_PACK_BUFFER',
    ],
  },
  'BufferMode': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_INTERLEAVED_ATTRIBS',
      'GL_SEPARATE_ATTRIBS',
    ],
    'invalid': [
      'GL_PIXEL_PACK_BUFFER',
    ],
  },
  'FramebufferAttachmentParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE',
      'GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME',
      'GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL',
      'GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE',
    ],
    'valid_es3': [
      'GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE',
      'GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE',
      'GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE',
      'GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE',
      'GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE',
      'GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE',
      'GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE',
      'GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING',
      'GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER',
    ],
  },
  'FramebufferParameter' : {
    'type': 'GLenum',
    'valid' : [],
  },
  'ProgramParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_DELETE_STATUS',
      'GL_LINK_STATUS',
      'GL_VALIDATE_STATUS',
      'GL_INFO_LOG_LENGTH',
      'GL_ATTACHED_SHADERS',
      'GL_ACTIVE_ATTRIBUTES',
      'GL_ACTIVE_ATTRIBUTE_MAX_LENGTH',
      'GL_ACTIVE_UNIFORMS',
      'GL_ACTIVE_UNIFORM_MAX_LENGTH',
    ],
    'valid_es3': [
      'GL_ACTIVE_UNIFORM_BLOCKS',
      'GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH',
      'GL_TRANSFORM_FEEDBACK_BUFFER_MODE',
      'GL_TRANSFORM_FEEDBACK_VARYINGS',
      'GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH',
    ],
    'invalid': [
      'GL_PROGRAM_BINARY_RETRIEVABLE_HINT',  # not supported in Chromium.
    ],
  },
  'QueryObjectParameter': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_QUERY_RESULT_EXT',
      'GL_QUERY_RESULT_AVAILABLE_EXT',
      'GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT',
    ],
  },
  'QueryParameter': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_CURRENT_QUERY_EXT',
    ],
  },
  'QueryTarget': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_SAMPLES_PASSED_ARB',
      'GL_ANY_SAMPLES_PASSED_EXT',
      'GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT',
      'GL_COMMANDS_ISSUED_CHROMIUM',
      'GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM',
      'GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM',
      'GL_COMMANDS_COMPLETED_CHROMIUM',
      'GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM',
      'GL_PROGRAM_COMPLETION_QUERY_CHROMIUM',
    ],
  },
  'RenderBufferParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_RENDERBUFFER_RED_SIZE',
      'GL_RENDERBUFFER_GREEN_SIZE',
      'GL_RENDERBUFFER_BLUE_SIZE',
      'GL_RENDERBUFFER_ALPHA_SIZE',
      'GL_RENDERBUFFER_DEPTH_SIZE',
      'GL_RENDERBUFFER_STENCIL_SIZE',
      'GL_RENDERBUFFER_WIDTH',
      'GL_RENDERBUFFER_HEIGHT',
      'GL_RENDERBUFFER_INTERNAL_FORMAT',
    ],
    'valid_es3': [
      'GL_RENDERBUFFER_SAMPLES',
    ],
  },
  'InternalFormatParameter': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_NUM_SAMPLE_COUNTS',
      'GL_SAMPLES',
    ],
  },
  'SamplerParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_MAG_FILTER',
      'GL_TEXTURE_MIN_FILTER',
      'GL_TEXTURE_MIN_LOD',
      'GL_TEXTURE_MAX_LOD',
      'GL_TEXTURE_WRAP_S',
      'GL_TEXTURE_WRAP_T',
      'GL_TEXTURE_WRAP_R',
      'GL_TEXTURE_COMPARE_MODE',
      'GL_TEXTURE_COMPARE_FUNC',
    ],
    'invalid': [
      'GL_GENERATE_MIPMAP',
    ],
  },
  'ShaderParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_SHADER_TYPE',
      'GL_DELETE_STATUS',
      'GL_COMPILE_STATUS',
      'GL_INFO_LOG_LENGTH',
      'GL_SHADER_SOURCE_LENGTH',
      'GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE',
    ],
  },
  'ShaderPrecision': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_LOW_FLOAT',
      'GL_MEDIUM_FLOAT',
      'GL_HIGH_FLOAT',
      'GL_LOW_INT',
      'GL_MEDIUM_INT',
      'GL_HIGH_INT',
    ],
  },
  'StringType': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_VENDOR',
      'GL_RENDERER',
      'GL_VERSION',
      'GL_SHADING_LANGUAGE_VERSION',
      'GL_EXTENSIONS',
    ],
  },
  'IndexedStringType': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_EXTENSIONS',
    ],
  },

  'TextureParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_MAG_FILTER',
      'GL_TEXTURE_MIN_FILTER',
      'GL_TEXTURE_WRAP_S',
      'GL_TEXTURE_WRAP_T',
    ],
    'valid_es3': [
      'GL_TEXTURE_BASE_LEVEL',
      'GL_TEXTURE_COMPARE_FUNC',
      'GL_TEXTURE_COMPARE_MODE',
      'GL_TEXTURE_IMMUTABLE_FORMAT',
      'GL_TEXTURE_IMMUTABLE_LEVELS',
      'GL_TEXTURE_MAX_LEVEL',
      'GL_TEXTURE_MAX_LOD',
      'GL_TEXTURE_MIN_LOD',
      'GL_TEXTURE_WRAP_R',
    ],
    'invalid': [
      'GL_GENERATE_MIPMAP',
    ],
  },
  'TextureWrapMode': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_CLAMP_TO_EDGE',
      'GL_MIRRORED_REPEAT',
      'GL_REPEAT',
    ],
  },
  'TextureMinFilterMode': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_NEAREST',
      'GL_LINEAR',
      'GL_NEAREST_MIPMAP_NEAREST',
      'GL_LINEAR_MIPMAP_NEAREST',
      'GL_NEAREST_MIPMAP_LINEAR',
      'GL_LINEAR_MIPMAP_LINEAR',
    ],
  },
  'TextureMagFilterMode': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_NEAREST',
      'GL_LINEAR',
    ],
  },
  'TextureCompareFunc': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_LEQUAL',
      'GL_GEQUAL',
      'GL_LESS',
      'GL_GREATER',
      'GL_EQUAL',
      'GL_NOTEQUAL',
      'GL_ALWAYS',
      'GL_NEVER',
    ],
  },
  'TextureCompareMode': {
    'type': 'GLenum',
    'valid': [
      'GL_NONE',
      'GL_COMPARE_REF_TO_TEXTURE',
    ],
  },
  'TextureSrgbDecodeExt': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_DECODE_EXT',
      'GL_SKIP_DECODE_EXT',
    ],
  },
  'TextureSwizzle': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_RED',
      'GL_GREEN',
      'GL_BLUE',
      'GL_ALPHA',
      'GL_ZERO',
      'GL_ONE',
    ],
  },
  'TextureUsage': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_NONE',
      'GL_FRAMEBUFFER_ATTACHMENT_ANGLE',
    ],
  },
  'VertexAttribute': {
    'type': 'GLenum',
    'valid': [
      # some enum that the decoder actually passes through to GL needs
      # to be the first listed here since it's used in unit tests.
      'GL_VERTEX_ATTRIB_ARRAY_NORMALIZED',
      'GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING',
      'GL_VERTEX_ATTRIB_ARRAY_ENABLED',
      'GL_VERTEX_ATTRIB_ARRAY_SIZE',
      'GL_VERTEX_ATTRIB_ARRAY_STRIDE',
      'GL_VERTEX_ATTRIB_ARRAY_TYPE',
      'GL_CURRENT_VERTEX_ATTRIB',
    ],
    'valid_es3': [
      'GL_VERTEX_ATTRIB_ARRAY_INTEGER',
      'GL_VERTEX_ATTRIB_ARRAY_DIVISOR',
    ],
  },
  'VertexPointer': {
    'type': 'GLenum',
    'valid': [
      'GL_VERTEX_ATTRIB_ARRAY_POINTER',
    ],
  },
  'HintTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_GENERATE_MIPMAP_HINT',
    ],
    'valid_es3': [
      'GL_FRAGMENT_SHADER_DERIVATIVE_HINT',
    ],
    'invalid': [
      'GL_PERSPECTIVE_CORRECTION_HINT',
    ],
  },
  'HintMode': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_FASTEST',
      'GL_NICEST',
      'GL_DONT_CARE',
    ],
  },
  'PixelStore': {
    'type': 'GLenum',
    'valid': [
      'GL_PACK_ALIGNMENT',
      'GL_UNPACK_ALIGNMENT',
    ],
    'valid_es3': [
      'GL_PACK_ROW_LENGTH',
      'GL_PACK_SKIP_PIXELS',
      'GL_PACK_SKIP_ROWS',
      'GL_UNPACK_ROW_LENGTH',
      'GL_UNPACK_IMAGE_HEIGHT',
      'GL_UNPACK_SKIP_PIXELS',
      'GL_UNPACK_SKIP_ROWS',
      'GL_UNPACK_SKIP_IMAGES',
    ],
    'invalid': [
      'GL_PACK_SWAP_BYTES',
      'GL_UNPACK_SWAP_BYTES',
    ],
  },
  'PixelStoreAlignment': {
    'type': 'GLint',
    'is_complete': True,
    'valid': [
      '1',
      '2',
      '4',
      '8',
    ],
    'invalid': [
      '3',
      '9',
    ],
  },
  'ReadPixelFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_ALPHA',
      'GL_RGB',
      'GL_RGBA',
    ],
    'valid_es3': [
      'GL_RED',
      'GL_RED_INTEGER',
      'GL_RG',
      'GL_RG_INTEGER',
      'GL_RGB_INTEGER',
      'GL_RGBA_INTEGER',
    ],
  },
  'PixelType': {
    'type': 'GLenum',
    'valid': [
      'GL_UNSIGNED_BYTE',
      'GL_UNSIGNED_SHORT_5_6_5',
      'GL_UNSIGNED_SHORT_4_4_4_4',
      'GL_UNSIGNED_SHORT_5_5_5_1',
    ],
    'valid_es3': [
      'GL_BYTE',
      'GL_UNSIGNED_SHORT',
      'GL_SHORT',
      'GL_UNSIGNED_INT',
      'GL_INT',
      'GL_HALF_FLOAT',
      'GL_FLOAT',
      'GL_UNSIGNED_INT_2_10_10_10_REV',
      'GL_UNSIGNED_INT_10F_11F_11F_REV',
      'GL_UNSIGNED_INT_5_9_9_9_REV',
      'GL_UNSIGNED_INT_24_8',
      'GL_FLOAT_32_UNSIGNED_INT_24_8_REV',
    ],
    'invalid': [
      'GL_UNSIGNED_BYTE_3_3_2',
    ],
  },
  'ReadPixelType': {
    'type': 'GLenum',
    'valid': [
      'GL_UNSIGNED_BYTE',
      'GL_UNSIGNED_SHORT_5_6_5',
      'GL_UNSIGNED_SHORT_4_4_4_4',
      'GL_UNSIGNED_SHORT_5_5_5_1',
    ],
    'valid_es3': [
      'GL_BYTE',
      'GL_UNSIGNED_SHORT',
      'GL_SHORT',
      'GL_UNSIGNED_INT',
      'GL_INT',
      'GL_HALF_FLOAT',
      'GL_FLOAT',
      'GL_UNSIGNED_INT_2_10_10_10_REV',
    ],
  },
  'RenderBufferFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_RGBA4',
      'GL_RGB565',
      'GL_RGB5_A1',
      'GL_DEPTH_COMPONENT16',
      'GL_STENCIL_INDEX8',
    ],
    'valid_es3': [
      'GL_R8',
      'GL_R8UI',
      'GL_R8I',
      'GL_R16UI',
      'GL_R16I',
      'GL_R32UI',
      'GL_R32I',
      'GL_RG8',
      'GL_RG8UI',
      'GL_RG8I',
      'GL_RG16UI',
      'GL_RG16I',
      'GL_RG32UI',
      'GL_RG32I',
      'GL_RGB8',
      'GL_RGBA8',
      'GL_SRGB8_ALPHA8',
      'GL_RGB10_A2',
      'GL_RGBA8UI',
      'GL_RGBA8I',
      'GL_RGB10_A2UI',
      'GL_RGBA16UI',
      'GL_RGBA16I',
      'GL_RGBA32UI',
      'GL_RGBA32I',
      'GL_DEPTH_COMPONENT24',
      'GL_DEPTH_COMPONENT32F',
      'GL_DEPTH24_STENCIL8',
      'GL_DEPTH32F_STENCIL8',
    ],
  },
  'ShaderBinaryFormat': {
    'type': 'GLenum',
    'valid': [
    ],
  },
  'StencilOp': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_KEEP',
      'GL_ZERO',
      'GL_REPLACE',
      'GL_INCR',
      'GL_INCR_WRAP',
      'GL_DECR',
      'GL_DECR_WRAP',
      'GL_INVERT',
    ],
  },
  'TextureFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_ALPHA',
      'GL_LUMINANCE',
      'GL_LUMINANCE_ALPHA',
      'GL_RGB',
      'GL_RGBA',
    ],
    'valid_es3': [
      'GL_RED',
      'GL_RED_INTEGER',
      'GL_RG',
      'GL_RG_INTEGER',
      'GL_RGB_INTEGER',
      'GL_RGBA_INTEGER',
      'GL_DEPTH_COMPONENT',
      'GL_DEPTH_STENCIL',
    ],
    'invalid': [
      'GL_BGRA',
      'GL_BGR',
    ],
  },
  'TextureInternalFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_ALPHA',
      'GL_LUMINANCE',
      'GL_LUMINANCE_ALPHA',
      'GL_RGB',
      'GL_RGBA',
    ],
    'valid_es3': [
      'GL_R8',
      'GL_R8_SNORM',
      'GL_R16F',
      'GL_R32F',
      'GL_R8UI',
      'GL_R8I',
      'GL_R16UI',
      'GL_R16I',
      'GL_R32UI',
      'GL_R32I',
      'GL_RG8',
      'GL_RG8_SNORM',
      'GL_RG16F',
      'GL_RG32F',
      'GL_RG8UI',
      'GL_RG8I',
      'GL_RG16UI',
      'GL_RG16I',
      'GL_RG32UI',
      'GL_RG32I',
      'GL_RGB8',
      'GL_SRGB8',
      'GL_RGB565',
      'GL_RGB8_SNORM',
      'GL_R11F_G11F_B10F',
      'GL_RGB9_E5',
      'GL_RGB16F',
      'GL_RGB32F',
      'GL_RGB8UI',
      'GL_RGB8I',
      'GL_RGB16UI',
      'GL_RGB16I',
      'GL_RGB32UI',
      'GL_RGB32I',
      'GL_RGBA8',
      'GL_SRGB8_ALPHA8',
      'GL_RGBA8_SNORM',
      'GL_RGB5_A1',
      'GL_RGBA4',
      'GL_RGB10_A2',
      'GL_RGBA16F',
      'GL_RGBA32F',
      'GL_RGBA8UI',
      'GL_RGBA8I',
      'GL_RGB10_A2UI',
      'GL_RGBA16UI',
      'GL_RGBA16I',
      'GL_RGBA32UI',
      'GL_RGBA32I',
      # The DEPTH/STENCIL formats are not supported in CopyTexImage2D.
      # We will reject them dynamically in GPU command buffer.
      'GL_DEPTH_COMPONENT16',
      'GL_DEPTH_COMPONENT24',
      'GL_DEPTH_COMPONENT32F',
      'GL_DEPTH24_STENCIL8',
      'GL_DEPTH32F_STENCIL8',
    ],
    'invalid': [
      'GL_BGRA',
      'GL_BGR',
    ],
  },
  'TextureUnsizedInternalFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_ALPHA',
      'GL_LUMINANCE',
      'GL_LUMINANCE_ALPHA',
      'GL_RGB',
      'GL_RGBA',
    ],
  },
  'TextureSizedColorRenderableInternalFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_R8',
      'GL_R8UI',
      'GL_R8I',
      'GL_R16UI',
      'GL_R16I',
      'GL_R32UI',
      'GL_R32I',
      'GL_RG8',
      'GL_RG8UI',
      'GL_RG8I',
      'GL_RG16UI',
      'GL_RG16I',
      'GL_RG32UI',
      'GL_RG32I',
      'GL_RGB8',
      'GL_RGB565',
      'GL_RGBA8',
      'GL_SRGB8_ALPHA8',
      'GL_RGB5_A1',
      'GL_RGBA4',
      'GL_RGB10_A2',
      'GL_RGBA8UI',
      'GL_RGBA8I',
      'GL_RGB10_A2UI',
      'GL_RGBA16UI',
      'GL_RGBA16I',
      'GL_RGBA32UI',
      'GL_RGBA32I',
    ],
  },
  'TextureDepthRenderableInternalFormat': {
    'type': 'GLenum',
    'valid': [],
    'valid_es3': [
      'GL_DEPTH_COMPONENT16',
      'GL_DEPTH_COMPONENT24',
      'GL_DEPTH_COMPONENT32F',
      'GL_DEPTH24_STENCIL8',
      'GL_DEPTH32F_STENCIL8',
    ],
  },
  'TextureStencilRenderableInternalFormat': {
    'type': 'GLenum',
    'valid': [],
    'valid_es3': [
      'GL_STENCIL_INDEX8',
      'GL_DEPTH24_STENCIL8',
      'GL_DEPTH32F_STENCIL8',
    ],
  },
  'TextureSizedTextureFilterableInternalFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_R8',
      'GL_R8_SNORM',
      'GL_R16F',
      'GL_RG8',
      'GL_RG8_SNORM',
      'GL_RG16F',
      'GL_RGB8',
      'GL_SRGB8',
      'GL_RGB565',
      'GL_RGB8_SNORM',
      'GL_R11F_G11F_B10F',
      'GL_RGB9_E5',
      'GL_RGB16F',
      'GL_RGBA8',
      'GL_SRGB8_ALPHA8',
      'GL_RGBA8_SNORM',
      'GL_RGB5_A1',
      'GL_RGBA4',
      'GL_RGB10_A2',
      'GL_RGBA16F',
      'GL_R16_EXT',
    ],
  },
  'TextureInternalFormatStorage': {
    'type': 'GLenum',
    'valid': [
      'GL_RGB565',
      'GL_RGBA4',
      'GL_RGB5_A1',
      'GL_ALPHA8_EXT',
      'GL_LUMINANCE8_EXT',
      'GL_LUMINANCE8_ALPHA8_EXT',
      'GL_RGB8_OES',
      'GL_RGBA8_OES',
    ],
    'valid_es3': [
      'GL_R8',
      'GL_R8_SNORM',
      'GL_R16F',
      'GL_R32F',
      'GL_R8UI',
      'GL_R8I',
      'GL_R16UI',
      'GL_R16I',
      'GL_R32UI',
      'GL_R32I',
      'GL_RG8',
      'GL_RG8_SNORM',
      'GL_RG16F',
      'GL_RG32F',
      'GL_RG8UI',
      'GL_RG8I',
      'GL_RG16UI',
      'GL_RG16I',
      'GL_RG32UI',
      'GL_RG32I',
      'GL_RGB8',
      'GL_SRGB8',
      'GL_RGB8_SNORM',
      'GL_R11F_G11F_B10F',
      'GL_RGB9_E5',
      'GL_RGB16F',
      'GL_RGB32F',
      'GL_RGB8UI',
      'GL_RGB8I',
      'GL_RGB16UI',
      'GL_RGB16I',
      'GL_RGB32UI',
      'GL_RGB32I',
      'GL_RGBA8',
      'GL_SRGB8_ALPHA8',
      'GL_RGBA8_SNORM',
      'GL_RGB10_A2',
      'GL_RGBA16F',
      'GL_RGBA32F',
      'GL_RGBA8UI',
      'GL_RGBA8I',
      'GL_RGB10_A2UI',
      'GL_RGBA16UI',
      'GL_RGBA16I',
      'GL_RGBA32UI',
      'GL_RGBA32I',
      'GL_DEPTH_COMPONENT16',
      'GL_DEPTH_COMPONENT24',
      'GL_DEPTH_COMPONENT32F',
      'GL_DEPTH24_STENCIL8',
      'GL_DEPTH32F_STENCIL8',
    ],
    'deprecated_es3': [
      'GL_ALPHA8_EXT',
      'GL_LUMINANCE8_EXT',
      'GL_LUMINANCE8_ALPHA8_EXT',
      'GL_ALPHA16F_EXT',
      'GL_LUMINANCE16F_EXT',
      'GL_LUMINANCE_ALPHA16F_EXT',
      'GL_ALPHA32F_EXT',
      'GL_LUMINANCE32F_EXT',
      'GL_LUMINANCE_ALPHA32F_EXT',
    ],
  },
  'ImageInternalFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_RGB',
      'GL_RGBA',
    ],
  },
  'UniformParameter': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_UNIFORM_SIZE',
      'GL_UNIFORM_TYPE',
      'GL_UNIFORM_NAME_LENGTH',
      'GL_UNIFORM_BLOCK_INDEX',
      'GL_UNIFORM_OFFSET',
      'GL_UNIFORM_ARRAY_STRIDE',
      'GL_UNIFORM_MATRIX_STRIDE',
      'GL_UNIFORM_IS_ROW_MAJOR',
    ],
    'invalid': [
      'GL_UNIFORM_BLOCK_NAME_LENGTH',
    ],
  },
  'UniformBlockParameter': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_UNIFORM_BLOCK_BINDING',
      'GL_UNIFORM_BLOCK_DATA_SIZE',
      'GL_UNIFORM_BLOCK_NAME_LENGTH',
      'GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS',
      'GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES',
      'GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER',
      'GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER',
    ],
    'invalid': [
      'GL_NEAREST',
    ],
  },
  'VertexAttribType': {
    'type': 'GLenum',
    'valid': [
      'GL_BYTE',
      'GL_UNSIGNED_BYTE',
      'GL_SHORT',
      'GL_UNSIGNED_SHORT',
      # 'GL_FIXED',  // This is not available on Desktop GL.
      'GL_FLOAT',
    ],
    'valid_es3': [
      'GL_INT',
      'GL_UNSIGNED_INT',
      'GL_HALF_FLOAT',
      'GL_INT_2_10_10_10_REV',
      'GL_UNSIGNED_INT_2_10_10_10_REV',
    ],
    'invalid': [
      'GL_DOUBLE',
    ],
  },
  'VertexAttribIType': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_BYTE',
      'GL_UNSIGNED_BYTE',
      'GL_SHORT',
      'GL_UNSIGNED_SHORT',
      'GL_INT',
      'GL_UNSIGNED_INT',
    ],
    'invalid': [
      'GL_FLOAT',
      'GL_DOUBLE',
    ],
  },
  'TextureBorder': {
    'type': 'GLint',
    'is_complete': True,
    'valid': [
      '0',
    ],
    'invalid': [
      '1',
    ],
  },
  'VertexAttribSize': {
    'type': 'GLint',
    'validator': False,
    'valid': [
      '1',
      '2',
      '3',
      '4',
    ],
    'invalid': [
      '0',
      '5',
    ],
  },
  'ResetStatus': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_GUILTY_CONTEXT_RESET_ARB',
      'GL_INNOCENT_CONTEXT_RESET_ARB',
      'GL_UNKNOWN_CONTEXT_RESET_ARB',
    ],
  },
  'SyncCondition': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_SYNC_GPU_COMMANDS_COMPLETE',
    ],
    'invalid': [
      '0',
    ],
  },
  'SyncFlags': {
    'type': 'GLbitfield',
    'is_complete': True,
    'valid': [
      '0',
    ],
    'invalid': [
      '1',
    ],
  },
  'SyncFlushFlags': {
    'type': 'GLbitfield',
    'valid': [
      'GL_SYNC_FLUSH_COMMANDS_BIT',
      '0',
    ],
    'invalid': [
      '0xFFFFFFFF',
    ],
  },
  'SyncParameter': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_SYNC_STATUS',  # This needs to be the 1st; all others are cached.
      'GL_OBJECT_TYPE',
      'GL_SYNC_CONDITION',
      'GL_SYNC_FLAGS',
    ],
    'invalid': [
      'GL_SYNC_FENCE',
    ],
  },
  'WindowRectanglesMode': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_INCLUSIVE_EXT',
      'GL_EXCLUSIVE_EXT',
    ],
  },
  'SwapBuffersFlags': {
    'type': 'GLbitfield',
    'is_complete': True,
    'valid': [
      '0',
      'gpu::SwapBuffersFlags::kVSyncParams',
    ],
  },
  'SharedImageAccessMode': {
    'type': 'GLenum',
    'is_complete': True,
    'valid': [
      'GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM',
      'GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM',
    ],
  },
}

# A function info object specifies the type and other special data for the
# command that will be generated. A base function info object is generated by
# parsing the "gles2_cmd_buffer_functions.txt", one for each function in the
# file. These function info objects can be augmented and their values can be
# overridden by adding an object to the table below.
#
# Must match function names specified in "gles2_cmd_buffer_functions.txt".
#
# cmd_comment:  A comment added to the cmd format.
# type :        defines which handler will be used to generate code.
# decoder_func: defines which function to call in the decoder to execute the
#               corresponding GL command. If not specified the GL command will
#               be called directly.
# gl_test_func: GL function that is expected to be called when testing.
# cmd_args:     The arguments to use for the command. This overrides generating
#               them based on the GL function arguments.
# data_transfer_methods: Array of methods that are used for transfering the
#               pointer data.  Possible values: 'immediate', 'shm', 'bucket'.
#               The default is 'immediate' if the command has one pointer
#               argument, otherwise 'shm'. One command is generated for each
#               transfer method. Affects only commands which are not of type
#               'GETn' or 'GLcharN'.
#               Note: the command arguments that affect this are the final args,
#               taking cmd_args override into consideration.
# impl_func:    Whether or not to generate the GLES2Implementation part of this
#               command.
# internal:     If true, this is an internal command only, not exposed to the
#               client.
# needs_size:   If True a data_size field is added to the command.
# count:        The number of units per element. For PUTn or PUT types.
# use_count_func: If True the actual data count needs to be computed; the count
#               argument specifies the maximum count.
# unit_test:    If False no service side unit test will be generated.
# client_test:  If False no client side unit test will be generated.
# expectation:  If False the unit test will have no expected calls.
# gen_func:     Name of function that generates GL resource for corresponding
#               bind function.
# states:       array of states that get set by this function corresponding to
#               the given arguments
# no_gl:        no GL function is called.
# valid_args:   A dictionary of argument indices to args to use in unit tests
#               when they can not be automatically determined.
# pepper_interface: The pepper interface that is used for this extension
# pepper_name:  The name of the function as exposed to pepper.
# pepper_args:  A string representing the argument list (what would appear in
#               C/C++ between the parentheses for the function declaration)
#               that the Pepper API expects for this function. Use this only if
#               the stable Pepper API differs from the GLES2 argument list.
# invalid_test: False if no invalid test needed.
# shadowed:     True = the value is shadowed so no glGetXXX call will be made.
# first_element_only: For PUT types, True if only the first element of an
#               array is used and we end up calling the single value
#               corresponding function. eg. TexParameteriv -> TexParameteri
# extension:    Function is an extension to GL and should not be exposed to
#               pepper unless pepper_interface is defined.
# extension_flag: Function is an extension and should be enabled only when
#               the corresponding feature info flag is enabled. Implies
#               'extension': True.
# not_shared:   For GENn types, True if objects can't be shared between contexts
# es3:          ES3 API. True if the function requires an ES3 or WebGL2 context.
# es31:         ES31 API. True if the function requires an WebGL2Compute
#               context.

_FUNCTION_INFO = {
  'ActiveTexture': {
    'decoder_func': 'DoActiveTexture',
    'unit_test': False,
    'impl_func': False,
    'client_test': False,
  },
  'AttachShader': {'decoder_func': 'DoAttachShader'},
  'BindAttribLocation': {
    'type': 'GLchar',
    'data_transfer_methods': ['bucket'],
    'needs_size': True,
  },
  'BindBuffer': {
    'type': 'Bind',
    'decoder_func': 'DoBindBuffer',
    'gen_func': 'GenBuffersARB',
  },
  'BindBufferBase': {
    'type': 'Bind',
    'decoder_func': 'DoBindBufferBase',
    'gen_func': 'GenBuffersARB',
    'unit_test': False,
    'es3': True,
  },
  'BindBufferRange': {
    'type': 'Bind',
    'decoder_func': 'DoBindBufferRange',
    'gen_func': 'GenBuffersARB',
    'unit_test': False,
    'valid_args': {
      '3': '4',
      '4': '4'
    },
    'es3': True,
  },
  'BindFramebuffer': {
    'type': 'Bind',
    'decoder_func': 'DoBindFramebuffer',
    'gl_test_func': 'glBindFramebufferEXT',
    'gen_func': 'GenFramebuffersEXT',
    'trace_level': 1,
  },
  'BindImageTexture':{
    'cmd_args': 'GLuint unit, GLuint texture, GLint level, GLboolean layered, '
                'GLint layer, GLenum access, GLenum format',
    'unit_test': False,
    'trace_level': 2,
    'es31': True,
  },
  'BindRenderbuffer': {
    'type': 'Bind',
    'decoder_func': 'DoBindRenderbuffer',
    'gl_test_func': 'glBindRenderbufferEXT',
    'gen_func': 'GenRenderbuffersEXT',
  },
  'BindSampler': {
    'type': 'Bind',
    'decoder_func': 'DoBindSampler',
    'es3': True,
  },
  'BindTexture': {
    'type': 'Bind',
    'decoder_func': 'DoBindTexture',
    'gen_func': 'GenTextures',
    # TODO: remove this once client side caching works.
    'client_test': False,
    'unit_test': False,
    'trace_level': 2,
  },
  'BindTransformFeedback': {
    'type': 'Bind',
    'decoder_func': 'DoBindTransformFeedback',
    'es3': True,
    'unit_test': False,
  },
  'BlitFramebufferCHROMIUM': {
    'decoder_func': 'DoBlitFramebufferCHROMIUM',
    'unit_test': False,
    'extension': 'chromium_framebuffer_multisample',
    'extension_flag': 'chromium_framebuffer_multisample',
    'pepper_interface': 'FramebufferBlit',
    'pepper_name': 'BlitFramebufferEXT',
    'trace_level': 1,
  },
  'BufferData': {
    'type': 'Custom',
    'impl_func': False,
    'data_transfer_methods': ['shm'],
    'size_args': {
      'data': 'size', },
    'client_test': False,
    'trace_level': 2,
  },
  'BufferSubData': {
    'type': 'Data',
    'client_test': False,
    'decoder_func': 'DoBufferSubData',
    'data_transfer_methods': ['shm'],
    'size_args': {
      'data': 'size', },
    'trace_level': 2,
  },
  'CheckFramebufferStatus': {
    'type': 'Is',
    'decoder_func': 'DoCheckFramebufferStatus',
    'gl_test_func': 'glCheckFramebufferStatusEXT',
    'error_value': 'GL_FRAMEBUFFER_UNSUPPORTED',
    'result': ['GLenum'],
  },
  'Clear': {
    'decoder_func': 'DoClear',
    'trace_level': 2,
    'valid_args': {
      '0': 'GL_COLOR_BUFFER_BIT'
    },
  },
  'ClearBufferiv': {
    'type': 'PUT',
    'use_count_func': True,
    'count': 4,
    'decoder_func': 'DoClearBufferiv',
    'unit_test': False,
    'es3': True,
    'trace_level': 2,
  },
  'ClearBufferuiv': {
    'type': 'PUT',
    'use_count_func': True,
    'count': 4,
    'decoder_func': 'DoClearBufferuiv',
    'unit_test': False,
    'es3': True,
    'trace_level': 2,
  },
  'ClearBufferfv': {
    'type': 'PUT',
    'use_count_func': True,
    'count': 4,
    'decoder_func': 'DoClearBufferfv',
    'unit_test': False,
    'es3': True,
    'trace_level': 2,
  },
  'ClearBufferfi': {
    'es3': True,
    'decoder_func': 'DoClearBufferfi',
    'unit_test': False,
    'trace_level': 2,
  },
  'ClearColor': {
    'type': 'StateSet',
    'state': 'ClearColor',
  },
  'ClearDepthf': {
    'type': 'StateSet',
    'state': 'ClearDepthf',
    'decoder_func': 'glClearDepth',
    'gl_test_func': 'glClearDepth',
    'valid_args': {
      '0': '0.5f'
    },
  },
  'ClientWaitSync': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args': 'GLuint sync, GLbitfieldSyncFlushFlags flags, '
                'GLuint64 timeout, GLenum* result',
    'es3': True,
    'result': ['GLenum'],
    'trace_level': 2,
  },
  'ClipControlEXT': {
    'extension_flag': 'ext_clip_control',
    'unit_test': False,
    'extension': 'EXT_clip_control',
  },
  'ColorMask': {
    'type': 'StateSet',
    'state': 'ColorMask',
    'no_gl': True,
    'expectation': False,
  },
  'ColorMaskiOES': {
    'extension_flag': 'oes_draw_buffers_indexed',
    'unit_test': False,
    'extension': 'OES_draw_buffers_indexed',
  },
  'ContextVisibilityHintCHROMIUM': {
    'decoder_func': 'DoContextVisibilityHintCHROMIUM',
    'extension': 'CHROMIUM_context_visibility_hint',
    'unit_test': False,
    'client_test': False,
  },
  'CopyBufferSubData': {
    'decoder_func': 'DoCopyBufferSubData',
    'impl_func': False,
    'unit_test': False,
    'es3': True,
  },
  'ClearStencil': {
    'type': 'StateSet',
    'state': 'ClearStencil',
  },
  'EnableFeatureCHROMIUM': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'decoder_func': 'DoEnableFeatureCHROMIUM',
    'cmd_args': 'GLuint bucket_id, GLint* result',
    'result': ['GLint'],
    'extension': 'GL_CHROMIUM_enable_feature',
    'pepper_interface': 'ChromiumEnableFeature',
  },
  'CompileShader': {'decoder_func': 'DoCompileShader', 'unit_test': False},
  'CompressedTexImage2D': {
    'type': 'Custom',
    'data_transfer_methods': ['bucket', 'shm'],
    'trace_level': 1,
  },
  'CompressedTexSubImage2D': {
    'type': 'Custom',
    'data_transfer_methods': ['bucket', 'shm'],
    'trace_level': 1,
  },
  'CopyTexImage2D': {
    'decoder_func': 'DoCopyTexImage2D',
    'unit_test': False,
    'trace_level': 1,
  },
  'CopyTexSubImage2D': {
    'decoder_func': 'DoCopyTexSubImage2D',
    'trace_level': 1,
  },
  'CompressedTexImage3D': {
    'type': 'Custom',
    'data_transfer_methods': ['bucket', 'shm'],
    'es3': True,
    'trace_level': 1,
  },
  'CompressedTexSubImage3D': {
    'type': 'Custom',
    'data_transfer_methods': ['bucket', 'shm'],
    'es3': True,
    'trace_level': 1,
  },
  'CopyTexSubImage3D': {
    'decoder_func': 'DoCopyTexSubImage3D',
    'unit_test': False,
    'es3': True,
    'trace_level': 1,
  },
  'DescheduleUntilFinishedCHROMIUM': {
    'type': 'Custom',
    'decoder_func': 'DoDescheduleUntilFinishedCHROMIUM',
    'extension': "CHROMIUM_deschedule",
    'trace_level': 1,
  },
  'CreateProgram': {
    'type': 'Create',
    'client_test': False,
  },
  'CreateShader': {
    'type': 'Create',
    'client_test': False,
  },
  'BlendColor': {
    'type': 'StateSet',
    'state': 'BlendColor',
  },
  'BlendEquation': {
    'type': 'StateSetRGBAlpha',
    'state': 'BlendEquation',
    'valid_args': {
      '0': 'GL_FUNC_SUBTRACT'
    },
  },
  'BlendEquationiOES': {
    'extension_flag': 'oes_draw_buffers_indexed',
    'unit_test': False,
    'extension': 'OES_draw_buffers_indexed',
    'valid_args': {
      '1': 'GL_FUNC_SUBTRACT',
      '2': 'GL_FUNC_SUBTRACT'
    },
  },
  'BlendEquationSeparate': {
    'type': 'StateSet',
    'state': 'BlendEquation',
    'valid_args': {
      '0': 'GL_FUNC_SUBTRACT'
    },
  },
  'BlendEquationSeparateiOES': {
    'extension_flag': 'oes_draw_buffers_indexed',
    'unit_test': False,
    'extension': 'OES_draw_buffers_indexed',
    'valid_args': {
      '1': 'GL_FUNC_SUBTRACT',
      '2': 'GL_FUNC_SUBTRACT'
    },
  },
  'BlendFunc': {
    'type': 'StateSetRGBAlpha',
    'state': 'BlendFunc',
  },
  'BlendFunciOES': {
    'extension_flag': 'oes_draw_buffers_indexed',
    'unit_test': False,
    'extension': 'OES_draw_buffers_indexed',
  },
  'BlendFuncSeparate': {
    'type': 'StateSet',
    'state': 'BlendFunc',
  },
  'BlendFuncSeparateiOES': {
    'extension_flag': 'oes_draw_buffers_indexed',
    'unit_test': False,
    'extension': 'OES_draw_buffers_indexed',
  },
  'BlendBarrierKHR': {
    'gl_test_func': 'glBlendBarrierKHR',
    'extension': 'KHR_blend_equation_advanced',
    'extension_flag': 'blend_equation_advanced',
    'client_test': False,
  },
  'SampleCoverage': {'decoder_func': 'DoSampleCoverage'},
  'StencilFunc': {
    'type': 'StateSetFrontBack',
    'state': 'StencilFunc',
  },
  'StencilFuncSeparate': {
    'type': 'StateSetFrontBackSeparate',
    'state': 'StencilFunc',
  },
  'StencilOp': {
    'type': 'StateSetFrontBack',
    'state': 'StencilOp',
    'valid_args': {
      '1': 'GL_INCR'
    },
  },
  'StencilOpSeparate': {
    'type': 'StateSetFrontBackSeparate',
    'state': 'StencilOp',
    'valid_args': {
      '1': 'GL_INCR'
    },
  },
  'Hint': {
    'type': 'StateSetNamedParameter',
    'state': 'Hint',
  },
  'CullFace': {'type': 'StateSet', 'state': 'CullFace'},
  'FrontFace': {'type': 'StateSet', 'state': 'FrontFace'},
  'DepthFunc': {'type': 'StateSet', 'state': 'DepthFunc'},
  'LineWidth': {
    'type': 'StateSet',
    'state': 'LineWidth',
    'decoder_func': 'DoLineWidth',
    'valid_args': {
      '0': '2.0f'
    },
  },
  'PolygonModeANGLE': {
    'extension_flag': 'angle_polygon_mode',
    'unit_test': False,
    'extension': 'ANGLE_polygon_mode',
  },
  'PolygonOffset': {
    'type': 'StateSet',
    'state': 'PolygonOffset',
  },
  'PolygonOffsetClampEXT': {
    'extension_flag': 'ext_polygon_offset_clamp',
    'unit_test': False,
    'extension': 'EXT_polygon_offset_clamp',
  },
  'DeleteBuffers': {
    'type': 'DELn',
    'gl_test_func': 'glDeleteBuffersARB',
    'resource_type': 'Buffer',
    'resource_types': 'Buffers',
  },
  'DeleteFramebuffers': {
    'type': 'DELn',
    'gl_test_func': 'glDeleteFramebuffersEXT',
    'resource_type': 'Framebuffer',
    'resource_types': 'Framebuffers',
    'trace_level': 2,
  },
  'DeleteProgram': { 'type': 'Delete' },
  'DeleteRenderbuffers': {
    'type': 'DELn',
    'gl_test_func': 'glDeleteRenderbuffersEXT',
    'resource_type': 'Renderbuffer',
    'resource_types': 'Renderbuffers',
    'trace_level': 2,
  },
  'DeleteSamplers': {
    'type': 'DELn',
    'resource_type': 'Sampler',
    'resource_types': 'Samplers',
    'es3': True,
  },
  'DeleteShader': { 'type': 'Delete' },
  'DeleteSync': {
    'type': 'Delete',
    'cmd_args': 'GLuint sync',
    'resource_type': 'Sync',
    'es3': True,
  },
  'DeleteTextures': {
    'type': 'DELn',
    'resource_type': 'Texture',
    'resource_types': 'Textures',
  },
  'DeleteTransformFeedbacks': {
    'type': 'DELn',
    'resource_type': 'TransformFeedback',
    'resource_types': 'TransformFeedbacks',
    'es3': True,
    'unit_test': False,
  },
  'DepthRangef': {
    'decoder_func': 'DoDepthRangef',
    'gl_test_func': 'glDepthRange',
  },
  'DepthMask': {
    'type': 'StateSet',
    'state': 'DepthMask',
    'no_gl': True,
    'expectation': False,
  },
  'DetachShader': {'decoder_func': 'DoDetachShader'},
  'Disable': {
    'decoder_func': 'DoDisable',
    'impl_func': False,
    'client_test': False,
  },
  'DisableiOES': {
    'extension_flag': 'oes_draw_buffers_indexed',
    'extension': 'OES_draw_buffers_indexed',
    'decoder_func': 'DoDisableiOES',
    'impl_func': False,
    'unit_test': False,
  },
  'DisableVertexAttribArray': {
    'decoder_func': 'DoDisableVertexAttribArray',
    'impl_func': False,
    'unit_test': False,
  },
  'DispatchCompute': {
    'cmd_args': 'GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z',
    'trace_level': 2,
    'es31': True,
    'unit_test': False,
  },
  'DispatchComputeIndirect': {
    'cmd_args': 'GLintptrNotNegative offset',
    'trace_level': 2,
    'es31': True,
    'unit_test': False,
  },
  'DrawArrays': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumDrawMode mode, GLint first, GLsizei count',
    'trace_level': 2,
  },
  'DrawArraysIndirect': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumDrawMode mode, GLuint offset',
    'trace_level': 2,
    'es31': True,
    'unit_test': False,
    'client_test': False,
  },
  'DrawElements': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumDrawMode mode, GLsizei count, '
                'GLenumIndexType type, GLuint index_offset',
    'client_test': False,
    'trace_level': 2,
  },
  'DrawElementsIndirect': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumDrawMode mode, GLenumIndexType type, GLuint offset',
    'trace_level': 2,
    'es31': True,
    'unit_test': False,
    'client_test': False,
  },
  'DrawRangeElements': {
    'type': 'NoCommand',
    'es3': True,
  },
  'Enable': {
    'decoder_func': 'DoEnable',
    'impl_func': False,
    'client_test': False,
  },
  'EnableiOES': {
    'extension_flag': 'oes_draw_buffers_indexed',
    'extension': 'OES_draw_buffers_indexed',
    'decoder_func': 'DoEnableiOES',
    'impl_func': False,
    'unit_test': False,
  },
  'EnableVertexAttribArray': {
    'decoder_func': 'DoEnableVertexAttribArray',
    'impl_func': False,
    'unit_test': False,
  },
  'FenceSync': {
    'type': 'Create',
    'client_test': False,
    'decoder_func': 'DoFenceSync',
    'es3': True,
    'trace_level': 1,
  },
  'Finish': {
    'impl_func': False,
    'client_test': False,
    'decoder_func': 'DoFinish',
    'trace_level': 1,
  },
  'Flush': {
    'impl_func': False,
    'decoder_func': 'DoFlush',
    'trace_level': 1,
  },
  'FlushMappedBufferRange': {
    'decoder_func': 'DoFlushMappedBufferRange',
    'trace_level': 1,
    'unit_test': False,
    'es3': True,
  },
  'FramebufferRenderbuffer': {
    'decoder_func': 'DoFramebufferRenderbuffer',
    'gl_test_func': 'glFramebufferRenderbufferEXT',
    'trace_level': 1,
  },
  'FramebufferTexture2D': {
    'decoder_func': 'DoFramebufferTexture2D',
    'gl_test_func': 'glFramebufferTexture2DEXT',
    'unit_test': False,
    'trace_level': 1,
  },
  'FramebufferTexture2DMultisampleEXT': {
    'decoder_func': 'DoFramebufferTexture2DMultisample',
    'gl_test_func': 'glFramebufferTexture2DMultisampleEXT',
    'unit_test': False,
    'extension': 'EXT_multisampled_render_to_texture',
    'extension_flag': 'multisampled_render_to_texture',
    'trace_level': 1,
  },
  'FramebufferTextureLayer': {
    'decoder_func': 'DoFramebufferTextureLayer',
    'es3': True,
    'unit_test': False,
    'trace_level': 1,
  },
  'GenerateMipmap': {
    'decoder_func': 'DoGenerateMipmap',
    'gl_test_func': 'glGenerateMipmapEXT',
    'trace_level': 1,
  },
  'GenBuffers': {
    'type': 'GENn',
    'gl_test_func': 'glGenBuffersARB',
    'resource_type': 'Buffer',
    'resource_types': 'Buffers',
  },
  'GenFramebuffers': {
    'type': 'GENn',
    'gl_test_func': 'glGenFramebuffersEXT',
    'resource_type': 'Framebuffer',
    'resource_types': 'Framebuffers',
    'not_shared': 'True',
  },
  'GenRenderbuffers': {
    'type': 'GENn', 'gl_test_func': 'glGenRenderbuffersEXT',
    'resource_type': 'Renderbuffer',
    'resource_types': 'Renderbuffers',
  },
  'GenSamplers': {
    'type': 'GENn',
    'gl_test_func': 'glGenSamplers',
    'resource_type': 'Sampler',
    'resource_types': 'Samplers',
    'es3': True,
  },
  'GenTextures': {
    'type': 'GENn',
    'gl_test_func': 'glGenTextures',
    'resource_type': 'Texture',
    'resource_types': 'Textures',
  },
  'GenTransformFeedbacks': {
    'type': 'GENn',
    'gl_test_func': 'glGenTransformFeedbacks',
    'resource_type': 'TransformFeedback',
    'resource_types': 'TransformFeedbacks',
    'es3': True,
    'not_shared': 'True',
  },
  'GetActiveAttrib': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, GLuint index, uint32_t name_bucket_id, '
        'void* result',
    'result': [
      'int32_t success',
      'int32_t size',
      'uint32_t type',
    ],
  },
  'GetActiveUniform': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, GLuint index, uint32_t name_bucket_id, '
        'void* result',
    'result': [
      'int32_t success',
      'int32_t size',
      'uint32_t type',
    ],
  },
  'GetActiveUniformBlockiv': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'result': ['SizedResult<GLint>'],
    'es3': True,
  },
  'GetActiveUniformBlockName': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, GLuint index, uint32_t name_bucket_id, '
        'void* result',
    'result': ['int32_t'],
    'es3': True,
  },
  'GetActiveUniformsiv': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, uint32_t indices_bucket_id, GLenum pname, '
        'GLint* params',
    'result': ['SizedResult<GLint>'],
    'es3': True,
  },
  'GetAttachedShaders': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args': 'GLidProgram program, void* result, uint32_t result_size',
    'result': ['SizedResult<GLuint>'],
  },
  'GetAttribLocation': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, uint32_t name_bucket_id, GLint* location',
    'result': ['GLint'],
    'error_return': -1,
  },
  'GetFragDataIndexEXT': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, uint32_t name_bucket_id, GLint* index',
    'result': ['GLint'],
    'error_return': -1,
    'extension': 'EXT_blend_func_extended',
    'extension_flag': 'ext_blend_func_extended',
  },
  'GetFragDataLocation': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, uint32_t name_bucket_id, GLint* location',
    'result': ['GLint'],
    'error_return': -1,
    'es3': True,
  },
  'GetBooleanv': {
    'type': 'GETn',
    'result': ['SizedResult<GLboolean>'],
    'decoder_func': 'DoGetBooleanv',
    'gl_test_func': 'glGetIntegerv',
  },
  'GetBooleani_v': {
    'type': 'GETn',
    'result': ['SizedResult<GLboolean>'],
    'decoder_func': 'DoGetBooleani_v',
    'shadowed': True,
    'client_test': False,
    'unit_test': False,
    'es3': True
  },
  'GetBufferParameteri64v': {
    'type': 'GETn',
    'result': ['SizedResult<GLint64>'],
    'decoder_func': 'DoGetBufferParameteri64v',
    'expectation': False,
    'shadowed': True,
    'es3': True,
  },
  'GetBufferParameteriv': {
    'type': 'GETn',
    'result': ['SizedResult<GLint>'],
    'decoder_func': 'DoGetBufferParameteriv',
    'expectation': False,
    'shadowed': True,
  },
  'GetError': {
    'type': 'Is',
    'decoder_func': 'GetErrorState()->GetGLError',
    'impl_func': False,
    'result': ['GLenum'],
    'client_test': False,
  },
  'GetFloatv': {
    'type': 'GETn',
    'result': ['SizedResult<GLfloat>'],
    'decoder_func': 'DoGetFloatv',
    'gl_test_func': 'glGetIntegerv',
  },
  'GetFramebufferAttachmentParameteriv': {
    'type': 'GETn',
    'decoder_func': 'DoGetFramebufferAttachmentParameteriv',
    'gl_test_func': 'glGetFramebufferAttachmentParameterivEXT',
    'result': ['SizedResult<GLint>'],
  },
  'GetGraphicsResetStatusKHR': {
    'type': 'NoCommand',
    'extension': True,
    'trace_level': 1,
  },
  'GetInteger64v': {
    'type': 'GETn',
    'result': ['SizedResult<GLint64>'],
    'client_test': False,
    'decoder_func': 'DoGetInteger64v',
    'gl_test_func': 'glGetIntegerv',
    'es3': True
  },
  'GetIntegerv': {
    'type': 'GETn',
    'result': ['SizedResult<GLint>'],
    'decoder_func': 'DoGetIntegerv',
    'client_test': False,
  },
  'GetInteger64i_v': {
    'type': 'GETn',
    'result': ['SizedResult<GLint64>'],
    'decoder_func': 'DoGetInteger64i_v',
    'shadowed': True,
    'client_test': False,
    'unit_test': False,
    'es3': True
  },
  'GetIntegeri_v': {
    'type': 'GETn',
    'result': ['SizedResult<GLint>'],
    'decoder_func': 'DoGetIntegeri_v',
    'shadowed': True,
    'client_test': False,
    'unit_test': False,
    'es3': True
  },
  'GetInternalformativ': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'result': ['SizedResult<GLint>'],
    'cmd_args':
        'GLenumRenderBufferTarget target, GLenumRenderBufferFormat format, '
        'GLenumInternalFormatParameter pname, GLint* params',
    'es3': True,
  },
  'GetMaxValueInBufferCHROMIUM': {
    'type': 'Is',
    'decoder_func': 'DoGetMaxValueInBufferCHROMIUM',
    'result': ['GLuint'],
    'unit_test': False,
    'client_test': False,
    'extension': True,
    'impl_func': False,
  },
  'GetProgramiv': {
    'type': 'GETn',
    'decoder_func': 'DoGetProgramiv',
    'result': ['SizedResult<GLint>'],
    'expectation': False,
  },
  'GetProgramInfoCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'extension': 'CHROMIUM_get_multiple',
    'client_test': False,
    'cmd_args': 'GLidProgram program, uint32_t bucket_id',
    'result': [
      'uint32_t link_status',
      'uint32_t num_attribs',
      'uint32_t num_uniforms',
    ],
  },
  'GetProgramInfoLog': {
    'type': 'STRn',
    'expectation': False,
  },
  'GetProgramInterfaceiv': {
    'type': 'GETn',
    'decoder_func': 'DoGetProgramInterfaceiv',
    'result': ['SizedResult<GLint>'],
    'unit_test': False,
    'trace_level': 2,
    'es31': True,
  },
  'GetProgramResourceiv': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, GLenum program_interface, GLuint index, '
        'uint32_t props_bucket_id, GLint* params',
    'result': ['SizedResult<GLint>'],
    'unit_test': False,
    'trace_level': 2,
    'es31': True,
  },
  'GetProgramResourceIndex': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, GLenum program_interface, '
        'uint32_t name_bucket_id, GLuint* index',
    'result': ['GLuint'],
    'error_return': 'GL_INVALID_INDEX',
    'unit_test': False,
    'trace_level': 2,
    'es31': True,
  },
  'GetProgramResourceLocation': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, GLenum program_interface, '
        'uint32_t name_bucket_id, GLint* location',
    'result': ['GLint'],
    'error_return': -1,
    'unit_test': False,
    'trace_level': 2,
    'es31': True,
  },
  'GetProgramResourceName': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, GLenum program_interface, GLuint index, '
        'uint32_t name_bucket_id, void* result',
    'result': ['int32_t'],
    'unit_test': False,
    'trace_level': 2,
    'es31': True,
  },
  'GetRenderbufferParameteriv': {
    'type': 'GETn',
    'decoder_func': 'DoGetRenderbufferParameteriv',
    'gl_test_func': 'glGetRenderbufferParameterivEXT',
    'result': ['SizedResult<GLint>'],
  },
  'GetSamplerParameterfv': {
    'type': 'GETn',
    'decoder_func': 'DoGetSamplerParameterfv',
    'result': ['SizedResult<GLfloat>'],
    'es3': True,
  },
  'GetSamplerParameteriv': {
    'type': 'GETn',
    'decoder_func': 'DoGetSamplerParameteriv',
    'result': ['SizedResult<GLint>'],
    'es3': True,
  },
  'GetShaderiv': {
    'type': 'GETn',
    'decoder_func': 'DoGetShaderiv',
    'result': ['SizedResult<GLint>'],
  },
  'GetShaderInfoLog': {
    'type': 'STRn',
    'get_len_func': 'glGetShaderiv',
    'get_len_enum': 'GL_INFO_LOG_LENGTH',
    'unit_test': False,
  },
  'GetShaderPrecisionFormat': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
      'GLenumShaderType shadertype, GLenumShaderPrecision precisiontype, '
      'void* result',
    'result': [
      'int32_t success',
      'int32_t min_range',
      'int32_t max_range',
      'int32_t precision',
    ],
  },
  'GetShaderSource': {
    'type': 'STRn',
    'get_len_func': 'DoGetShaderiv',
    'get_len_enum': 'GL_SHADER_SOURCE_LENGTH',
    'unit_test': False,
    'client_test': False,
  },
  'GetString': {
    'type': 'Custom',
    'client_test': False,
    'cmd_args': 'GLenumStringType name, uint32_t bucket_id',
  },
  'GetStringi': {
    'type': 'NoCommand',
    'es3': True,
  },
  'GetSynciv': {
    'type': 'GETn',
    'cmd_args': 'GLuint sync, GLenumSyncParameter pname, void* values',
    'decoder_func': 'DoGetSynciv',
    'result': ['SizedResult<GLint>'],
    'es3': True,
  },
  'GetTexParameterfv': {
    'type': 'GETn',
    'decoder_func': 'DoGetTexParameterfv',
    'result': ['SizedResult<GLfloat>']
  },
  'GetTexParameteriv': {
    'type': 'GETn',
    'decoder_func': 'DoGetTexParameteriv',
    'result': ['SizedResult<GLint>']
  },
  'GetTranslatedShaderSourceANGLE': {
    'type': 'STRn',
    'get_len_func': 'DoGetShaderiv',
    'get_len_enum': 'GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE',
    'unit_test': False,
    'extension': True,
  },
  'GetUniformBlockIndex': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, uint32_t name_bucket_id, GLuint* index',
    'result': ['GLuint'],
    'error_return': 'GL_INVALID_INDEX',
    'es3': True,
  },
  'GetUniformBlocksCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'extension': True,
    'client_test': False,
    'cmd_args': 'GLidProgram program, uint32_t bucket_id',
    'result': ['uint32_t'],
    'es3': True,
  },
  'GetUniformsES3CHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'extension': True,
    'client_test': False,
    'cmd_args': 'GLidProgram program, uint32_t bucket_id',
    'result': ['uint32_t'],
    'es3': True,
  },
  'GetTransformFeedbackVarying': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, GLuint index, uint32_t name_bucket_id, '
        'void* result',
    'result': [
      'int32_t success',
      'int32_t size',
      'uint32_t type',
    ],
    'es3': True,
  },
  'GetTransformFeedbackVaryingsCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'extension': True,
    'client_test': False,
    'cmd_args': 'GLidProgram program, uint32_t bucket_id',
    'result': ['uint32_t'],
    'es3': True,
  },
  'GetUniformfv': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'result': ['SizedResult<GLfloat>'],
  },
  'GetUniformiv': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'result': ['SizedResult<GLint>'],
  },
  'GetUniformuiv': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'result': ['SizedResult<GLuint>'],
    'es3': True,
  },
  'GetUniformIndices': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'result': ['SizedResult<GLuint>'],
    'cmd_args': 'GLidProgram program, uint32_t names_bucket_id, '
                'GLuint* indices',
    'es3': True,
  },
  'GetUniformLocation': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args':
        'GLidProgram program, uint32_t name_bucket_id, GLint* location',
    'result': ['GLint'],

    # http://www.opengl.org/sdk/docs/man/xhtml/glGetUniformLocation.xml
    'error_return': -1,
  },
  'GetVertexAttribfv': {
    'type': 'GETn',
    'result': ['SizedResult<GLfloat>'],
    'impl_func': False,
    'decoder_func': 'DoGetVertexAttribfv',
    'expectation': False,
    'client_test': False,
  },
  'GetVertexAttribiv': {
    'type': 'GETn',
    'result': ['SizedResult<GLint>'],
    'impl_func': False,
    'decoder_func': 'DoGetVertexAttribiv',
    'expectation': False,
    'client_test': False,
  },
  'GetVertexAttribIiv': {
    'type': 'GETn',
    'result': ['SizedResult<GLint>'],
    'impl_func': False,
    'decoder_func': 'DoGetVertexAttribIiv',
    'expectation': False,
    'client_test': False,
    'es3': True,
  },
  'GetVertexAttribIuiv': {
    'type': 'GETn',
    'result': ['SizedResult<GLuint>'],
    'impl_func': False,
    'decoder_func': 'DoGetVertexAttribIuiv',
    'expectation': False,
    'client_test': False,
    'es3': True,
  },
  'GetVertexAttribPointerv': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'result': ['SizedResult<GLuint>'],
    'client_test': False,
  },
  'InvalidateFramebuffer': {
    'type': 'PUTn',
    'count': 1,
    'decoder_func': 'DoInvalidateFramebuffer',
    'unit_test': False,
    'es3': True,
  },
  'InvalidateSubFramebuffer': {
    'type': 'PUTn',
    'count': 1,
    'decoder_func': 'DoInvalidateSubFramebuffer',
    'unit_test': False,
    'es3': True,
  },
  'IsBuffer': {
    'type': 'Is',
    'decoder_func': 'DoIsBuffer',
    'expectation': False,
  },
  'IsEnabled': {
    'type': 'Is',
    'decoder_func': 'DoIsEnabled',
    'client_test': False,
    'impl_func': False,
    'expectation': False,
  },
  'IsEnablediOES': {
    'extension_flag': 'oes_draw_buffers_indexed',
    'unit_test': False,
    'extension': 'OES_draw_buffers_indexed',
    'type': 'Is',
    'decoder_func': 'DoIsEnablediOES',
    'client_test': False,
    'impl_func': False,
    'expectation': False,
  },
  'IsFramebuffer': {
    'type': 'Is',
    'decoder_func': 'DoIsFramebuffer',
    'expectation': False,
  },
  'IsProgram': {
    'type': 'Is',
    'decoder_func': 'DoIsProgram',
    'expectation': False,
  },
  'IsRenderbuffer': {
    'type': 'Is',
    'decoder_func': 'DoIsRenderbuffer',
    'expectation': False,
  },
  'IsShader': {
    'type': 'Is',
    'decoder_func': 'DoIsShader',
    'expectation': False,
  },
  'IsSampler': {
    'type': 'Is',
    'decoder_func': 'DoIsSampler',
    'expectation': False,
    'es3': True,
  },
  'IsSync': {
    'type': 'Is',
    'cmd_args': 'GLuint sync',
    'decoder_func': 'DoIsSync',
    'expectation': False,
    'es3': True,
  },
  'IsTexture': {
    'type': 'Is',
    'decoder_func': 'DoIsTexture',
    'expectation': False,
  },
  'IsTransformFeedback': {
    'type': 'Is',
    'decoder_func': 'DoIsTransformFeedback',
    'expectation': False,
    'es3': True,
  },
  'GetLastFlushIdCHROMIUM': {
    'type': 'NoCommand',
    'impl_func': False,
    'result': ['GLuint'],
    'extension': True,
  },
  'LinkProgram': {
    'decoder_func': 'DoLinkProgram',
    'impl_func':  False,
    'trace_level': 1,
  },
  'MapBufferCHROMIUM': {
    'type': 'NoCommand',
    'extension': "CHROMIUM_pixel_transfer_buffer_object",
    'trace_level': 1,
  },
  'MapBufferSubDataCHROMIUM': {
    'type': 'NoCommand',
    'extension': 'CHROMIUM_map_sub',
    'pepper_interface': 'ChromiumMapSub',
    'trace_level': 1,
  },
  'MapTexSubImage2DCHROMIUM': {
    'type': 'NoCommand',
    'extension': "CHROMIUM_sub_image",
    'pepper_interface': 'ChromiumMapSub',
    'trace_level': 1,
  },
  'MapBufferRange': {
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'cmd_args': 'GLenumBufferTarget target, GLintptrNotNegative offset, '
                'GLsizeiptr size, GLbitfieldMapBufferAccess access, '
                'uint32_t data_shm_id, uint32_t data_shm_offset, '
                'uint32_t result_shm_id, uint32_t result_shm_offset',
    'es3': True,
    'result': ['uint32_t'],
    'trace_level': 1,
  },
  # MemoryBarrierEXT is in order to avoid the conflicting MemoryBarrier macro
  # in windows.
  'MemoryBarrierEXT': {
    'cmd_args': 'GLbitfield barriers',
    'unit_test': False,
    'trace_level': 2,
    'es31': True
  },
  'MemoryBarrierByRegion': {
    'cmd_args': 'GLbitfield barriers',
    'unit_test': False,
    'trace_level': 2,
    'es31': True
  },
  'MultiDrawBeginCHROMIUM': {
    'decoder_func': 'DoMultiDrawBeginCHROMIUM',
    'extension': 'WEBGL_multi_draw',
    'extension_flag': 'webgl_multi_draw',
    'internal': True,
    'trace_level': 1,
    'impl_func': False,
    'unit_test': False,
  },
  'MultiDrawEndCHROMIUM': {
    'decoder_func': 'DoMultiDrawEndCHROMIUM',
    'extension': 'WEBGL_multi_draw',
    'extension_flag': 'webgl_multi_draw',
    'internal': True,
    'trace_level': 1,
    'impl_func': False,
    'unit_test': False,
  },
  'MultiDrawArraysCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLenumDrawMode mode, '
                'uint32_t firsts_shm_id, uint32_t firsts_shm_offset, '
                'uint32_t counts_shm_id, uint32_t counts_shm_offset, '
                'GLsizei drawcount',
    'extension': 'WEBGL_multi_draw',
    'extension_flag': 'webgl_multi_draw',
    'data_transfer_methods': ['shm'],
    'size_args': {
      'firsts': 'drawcount * sizeof(GLint)',
      'counts': 'drawcount * sizeof(GLsizei)', },
    'impl_func': False,
    'client_test': False,
    'internal': True,
    'trace_level': 2,
  },
  'MultiDrawArraysInstancedCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLenumDrawMode mode, '
                'uint32_t firsts_shm_id, uint32_t firsts_shm_offset, '
                'uint32_t counts_shm_id, uint32_t counts_shm_offset, '
                'uint32_t instance_counts_shm_id, '
                'uint32_t instance_counts_shm_offset, GLsizei drawcount',
    'extension': 'WEBGL_multi_draw',
    'extension_flag': 'webgl_multi_draw',
    'data_transfer_methods': ['shm'],
    'size_args': {
      'firsts': 'drawcount * sizeof(GLint)',
      'counts': 'drawcount * sizeof(GLsizei)',
      'instance_counts': 'drawcount * sizeof(GLsizei)', },
    'impl_func': False,
    'client_test': False,
    'internal': True,
    'trace_level': 2,
  },
  'MultiDrawArraysInstancedBaseInstanceCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLenumDrawMode mode, '
                'uint32_t firsts_shm_id, uint32_t firsts_shm_offset, '
                'uint32_t counts_shm_id, uint32_t counts_shm_offset, '
                'uint32_t instance_counts_shm_id, '
                'uint32_t instance_counts_shm_offset, '
                'uint32_t baseinstances_shm_id, '
                'uint32_t baseinstances_shm_offset, '
                'GLsizei drawcount',
    'extension': 'WEBGL_multi_draw_instanced_base_vertex_base_instance',
    'extension_flag': 'webgl_multi_draw_instanced_base_vertex_base_instance',
    'data_transfer_methods': ['shm'],
    'size_args': {
      'firsts': 'drawcount * sizeof(GLint)',
      'counts': 'drawcount * sizeof(GLsizei)',
      'instance_counts': 'drawcount * sizeof(GLsizei)',
      'baseinstances': 'drawcount * sizeof(GLuint)',
    },
    'impl_func': False,
    'client_test': False,
    'internal': True,
    'trace_level': 2,
  },
  'MultiDrawElementsCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLenumDrawMode mode, '
                'uint32_t counts_shm_id, uint32_t counts_shm_offset, '
                'GLenumIndexType type, '
                'uint32_t offsets_shm_id, uint32_t offsets_shm_offset, '
                'GLsizei drawcount',
    'extension': 'WEBGL_multi_draw',
    'extension_flag': 'webgl_multi_draw',
    'data_transfer_methods': ['shm'],
    'size_args': {
      'counts': 'drawcount * sizeof(GLsizei)',
      'offsets': 'drawcount * sizeof(GLsizei)', },
    'impl_func': False,
    'client_test': False,
    'internal': True,
    'trace_level': 2,
  },
  'MultiDrawElementsInstancedCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLenumDrawMode mode, '
                'uint32_t counts_shm_id, uint32_t counts_shm_offset, '
                'GLenumIndexType type, '
                'uint32_t offsets_shm_id, uint32_t offsets_shm_offset, '
                'uint32_t instance_counts_shm_id, '
                'uint32_t instance_counts_shm_offset, GLsizei drawcount',
    'extension': 'WEBGL_multi_draw',
    'extension_flag': 'webgl_multi_draw',
    'data_transfer_methods': ['shm'],
    'size_args': {
      'counts': 'drawcount * sizeof(GLsizei)',
      'offsets': 'drawcount * sizeof(GLsizei)',
      'instance_counts': 'drawcount * sizeof(GLsizei)', },
    'impl_func': False,
    'client_test': False,
    'internal': True,
    'trace_level': 2,
  },
  'MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLenumDrawMode mode, '
                'uint32_t counts_shm_id, uint32_t counts_shm_offset, '
                'GLenumIndexType type, '
                'uint32_t offsets_shm_id, uint32_t offsets_shm_offset, '
                'uint32_t instance_counts_shm_id, '
                'uint32_t instance_counts_shm_offset, '
                'uint32_t basevertices_shm_id, '
                'uint32_t basevertices_shm_offset, '
                'uint32_t baseinstances_shm_id, '
                'uint32_t baseinstances_shm_offset, '
                'GLsizei drawcount',
    'extension': 'WEBGL_multi_draw_instanced_base_vertex_base_instance',
    'extension_flag': 'webgl_multi_draw_instanced_base_vertex_base_instance',
    'data_transfer_methods': ['shm'],
    'size_args': {
      'counts': 'drawcount * sizeof(GLsizei)',
      'offsets': 'drawcount * sizeof(GLsizei)',
      'instance_counts': 'drawcount * sizeof(GLsizei)',
      'basevertices': 'drawcount * sizeof(GLint)',
      'baseinstances': 'drawcount * sizeof(GLuint)',
    },
    'impl_func': False,
    'client_test': False,
    'internal': True,
    'trace_level': 2,
  },
  'MultiDrawArraysWEBGL': {
    'type': 'NoCommand',
    'extension': 'WEBGL_multi_draw',
    'extension_flag': 'webgl_multi_draw',
  },
  'MultiDrawArraysInstancedWEBGL': {
    'type': 'NoCommand',
    'extension': 'WEBGL_multi_draw',
    'extension_flag': 'webgl_multi_draw',
  },
  'MultiDrawArraysInstancedBaseInstanceWEBGL': {
    'type': 'NoCommand',
    'extension': 'WEBGL_multi_draw_instanced_base_vertex_base_instance',
    'extension_flag': 'webgl_multi_draw_instanced_base_vertex_base_instance',
  },
  'MultiDrawElementsWEBGL': {
    'type': 'NoCommand',
    'extension': 'WEBGL_multi_draw',
    'extension_flag': 'webgl_multi_draw',
  },
  'MultiDrawElementsInstancedWEBGL': {
    'type': 'NoCommand',
    'extension': 'WEBGL_multi_draw',
    'extension_flag': 'webgl_multi_draw',
  },
  'MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL': {
    'type': 'NoCommand',
    'extension': 'WEBGL_multi_draw_instanced_base_vertex_base_instance',
    'extension_flag': 'webgl_multi_draw_instanced_base_vertex_base_instance',
  },
  'PauseTransformFeedback': {
    'decoder_func': 'DoPauseTransformFeedback',
    'unit_test': False,
    'es3': True,
  },
  'PixelStorei': {
    'type': 'Custom',
    'impl_func': False,
  },
  'ProvokingVertexANGLE': {
    'extension_flag': 'angle_provoking_vertex',
    'unit_test': False,
    'extension': 'ANGLE_provoking_vertex',
  },
  'RenderbufferStorage': {
    'decoder_func': 'DoRenderbufferStorage',
    'gl_test_func': 'glRenderbufferStorageEXT',
    'expectation': False,
    'trace_level': 1,
  },
  'RenderbufferStorageMultisampleCHROMIUM': {
    'cmd_comment':
        '// GL_CHROMIUM_framebuffer_multisample\n',
    'decoder_func': 'DoRenderbufferStorageMultisampleCHROMIUM',
    'gl_test_func': 'glRenderbufferStorageMultisampleCHROMIUM',
    'unit_test': False,
    'extension': 'chromium_framebuffer_multisample',
    'extension_flag': 'chromium_framebuffer_multisample',
    'pepper_interface': 'FramebufferMultisample',
    'pepper_name': 'RenderbufferStorageMultisampleEXT',
    'trace_level': 1,
  },
  'RenderbufferStorageMultisampleAdvancedAMD': {
    'cmd_comment':
        '// GL_AMD_framebuffer_multisample_advanced\n',
    'decoder_func': 'DoRenderbufferStorageMultisampleAdvancedAMD',
    'gl_test_func': 'glRenderbufferStorageMultisampleAdvancedAMD',
    'unit_test': False,
    'extension': 'amd_framebuffer_multisample_advanced',
    'extension_flag': 'amd_framebuffer_multisample_advanced',
    'trace_level': 1,
  },
  'RenderbufferStorageMultisampleEXT': {
    'cmd_comment':
        '// GL_EXT_multisampled_render_to_texture\n',
    'decoder_func': 'DoRenderbufferStorageMultisampleEXT',
    'gl_test_func': 'glRenderbufferStorageMultisampleEXT',
    'unit_test': False,
    'extension': 'EXT_multisampled_render_to_texture',
    'extension_flag': 'multisampled_render_to_texture',
    'trace_level': 1,
  },
  'ReadBuffer': {
    'es3': True,
    'decoder_func': 'DoReadBuffer',
    'trace_level': 1,
  },
  'ReadPixels': {
    'cmd_comment':
        '// ReadPixels has the result separated from the pixel buffer so that\n'
        '// it is easier to specify the result going to some specific place\n'
        '// that exactly fits the rectangle of pixels.\n',
    'type': 'Custom',
    'data_transfer_methods': ['shm'],
    'impl_func': False,
    'client_test': False,
    'cmd_args':
        'GLint x, GLint y, GLsizei width, GLsizei height, '
        'GLenumReadPixelFormat format, GLenumReadPixelType type, '
        'uint32_t pixels_shm_id, uint32_t pixels_shm_offset, '
        'uint32_t result_shm_id, uint32_t result_shm_offset, '
        'GLboolean async',
    'result': [
      'uint32_t success',
      # Below args exclude out-of-bounds area.
      'int32_t row_length',
      'int32_t num_rows',
    ],
    'trace_level': 1,
  },
  'ReleaseShaderCompiler': {
    'decoder_func': 'DoReleaseShaderCompiler',
    'unit_test': False,
  },
  'ResumeTransformFeedback': {
    'decoder_func': 'DoResumeTransformFeedback',
    'unit_test': False,
    'es3': True,
  },
  'SamplerParameterf': {
    'valid_args': {
      '2': 'GL_NEAREST'
    },
    'decoder_func': 'DoSamplerParameterf',
    'es3': True,
  },
  'SamplerParameterfv': {
    'type': 'PUT',
    'data_value': 'GL_NEAREST',
    'count': 1,
    'gl_test_func': 'glSamplerParameterf',
    'decoder_func': 'DoSamplerParameterfv',
    'first_element_only': True,
    'es3': True,
  },
  'SamplerParameteri': {
    'valid_args': {
      '2': 'GL_NEAREST'
    },
    'decoder_func': 'DoSamplerParameteri',
    'es3': True,
  },
  'SamplerParameteriv': {
    'type': 'PUT',
    'data_value': 'GL_NEAREST',
    'count': 1,
    'gl_test_func': 'glSamplerParameteri',
    'decoder_func': 'DoSamplerParameteriv',
    'first_element_only': True,
    'es3': True,
  },
  'ShaderBinary': {
    'type': 'Custom',
    'client_test': False,
  },
  'ShaderSource': {
    'type': 'PUTSTR',
    'decoder_func': 'DoShaderSource',
    'expectation': False,
    'data_transfer_methods': ['bucket'],
    'cmd_args':
        'GLuint shader, const char** str',
    'pepper_args':
        'GLuint shader, GLsizei count, const char** str, const GLint* length',
  },
  'StencilMask': {
    'type': 'StateSetFrontBack',
    'state': 'StencilMask',
    'no_gl': True,
    'expectation': False,
  },
  'StencilMaskSeparate': {
    'type': 'StateSetFrontBackSeparate',
    'state': 'StencilMask',
    'no_gl': True,
    'expectation': False,
  },
  'SwapBuffers': {
    'impl_func': False,
    'decoder_func': 'DoSwapBuffers',
    'client_test': False,
    'expectation': False,
    'extension': True,
    'trace_level': 1,
    'trace_queueing_flow': True,
  },
  'TexImage2D': {
    'type': 'Custom',
    'impl_func': False,
    'data_transfer_methods': ['shm'],
    'client_test': False,
    'trace_level': 2,
  },
  'TexImage3D': {
    'type': 'Custom',
    'impl_func': False,
    'data_transfer_methods': ['shm'],
    'client_test': False,
    'es3': True,
    'trace_level': 2,
  },
  'TexParameterf': {
    'decoder_func': 'DoTexParameterf',
    'valid_args': {
      '2': 'GL_NEAREST'
    },
  },
  'TexParameteri': {
    'decoder_func': 'DoTexParameteri',
    'valid_args': {
      '2': 'GL_NEAREST'
    },
  },
  'TexParameterfv': {
    'type': 'PUT',
    'data_value': 'GL_NEAREST',
    'count': 1,
    'decoder_func': 'DoTexParameterfv',
    'gl_test_func': 'glTexParameterf',
    'first_element_only': True,
  },
  'TexParameteriv': {
    'type': 'PUT',
    'data_value': 'GL_NEAREST',
    'count': 1,
    'decoder_func': 'DoTexParameteriv',
    'gl_test_func': 'glTexParameteri',
    'first_element_only': True,
  },
  'TexStorage3D': {
    'es3': True,
    'unit_test': False,
    'decoder_func': 'DoTexStorage3D',
    'trace_level': 2,
  },
  'TexSubImage2D': {
    'type': 'Custom',
    'impl_func': False,
    'data_transfer_methods': ['shm'],
    'client_test': False,
    'trace_level': 2,
    'cmd_args': 'GLenumTextureTarget target, GLint level, '
                'GLint xoffset, GLint yoffset, '
                'GLsizei width, GLsizei height, '
                'GLenumTextureFormat format, GLenumPixelType type, '
                'const void* pixels, GLboolean internal'
  },
  'TexSubImage3D': {
    'type': 'Custom',
    'impl_func': False,
    'data_transfer_methods': ['shm'],
    'client_test': False,
    'trace_level': 2,
    'cmd_args': 'GLenumTextureTarget target, GLint level, '
                'GLint xoffset, GLint yoffset, GLint zoffset, '
                'GLsizei width, GLsizei height, GLsizei depth, '
                'GLenumTextureFormat format, GLenumPixelType type, '
                'const void* pixels, GLboolean internal',
    'es3': True,
  },
  'TransformFeedbackVaryings': {
    'type': 'PUTSTR',
    'data_transfer_methods': ['bucket'],
    'decoder_func': 'DoTransformFeedbackVaryings',
    'cmd_args':
        'GLuint program, const char** varyings, GLenum buffermode',
    'expectation': False,
    'es3': True,
  },
  'Uniform1f': {'type': 'PUTXn', 'count': 1},
  'Uniform1fv': {
    'type': 'PUTn',
    'count': 1,
    'decoder_func': 'DoUniform1fv',
  },
  'Uniform1i': {'decoder_func': 'DoUniform1i', 'unit_test': False},
  'Uniform1iv': {
    'type': 'PUTn',
    'count': 1,
    'decoder_func': 'DoUniform1iv',
    'unit_test': False,
  },
  'Uniform1ui': {
    'type': 'PUTXn',
    'count': 1,
    'unit_test': False,
    'es3': True,
  },
  'Uniform1uiv': {
    'type': 'PUTn',
    'count': 1,
    'decoder_func': 'DoUniform1uiv',
    'unit_test': False,
    'es3': True,
  },
  'Uniform2i': {'type': 'PUTXn', 'count': 2},
  'Uniform2f': {'type': 'PUTXn', 'count': 2},
  'Uniform2fv': {
    'type': 'PUTn',
    'count': 2,
    'decoder_func': 'DoUniform2fv',
  },
  'Uniform2iv': {
    'type': 'PUTn',
    'count': 2,
    'decoder_func': 'DoUniform2iv',
  },
  'Uniform2ui': {
    'type': 'PUTXn',
    'count': 2,
    'unit_test': False,
    'es3': True,
  },
  'Uniform2uiv': {
    'type': 'PUTn',
    'count': 2,
    'decoder_func': 'DoUniform2uiv',
    'unit_test': False,
    'es3': True,
  },
  'Uniform3i': {'type': 'PUTXn', 'count': 3},
  'Uniform3f': {'type': 'PUTXn', 'count': 3},
  'Uniform3fv': {
    'type': 'PUTn',
    'count': 3,
    'decoder_func': 'DoUniform3fv',
  },
  'Uniform3iv': {
    'type': 'PUTn',
    'count': 3,
    'decoder_func': 'DoUniform3iv',
  },
  'Uniform3ui': {
    'type': 'PUTXn',
    'count': 3,
    'unit_test': False,
    'es3': True,
  },
  'Uniform3uiv': {
    'type': 'PUTn',
    'count': 3,
    'decoder_func': 'DoUniform3uiv',
    'unit_test': False,
    'es3': True,
  },
  'Uniform4i': {'type': 'PUTXn', 'count': 4},
  'Uniform4f': {'type': 'PUTXn', 'count': 4},
  'Uniform4fv': {
    'type': 'PUTn',
    'count': 4,
    'decoder_func': 'DoUniform4fv',
  },
  'Uniform4iv': {
    'type': 'PUTn',
    'count': 4,
    'decoder_func': 'DoUniform4iv',
  },
  'Uniform4ui': {
    'type': 'PUTXn',
    'count': 4,
    'unit_test': False,
    'es3': True,
  },
  'Uniform4uiv': {
    'type': 'PUTn',
    'count': 4,
    'decoder_func': 'DoUniform4uiv',
    'unit_test': False,
    'es3': True,
  },
  'UniformMatrix2fv': {
    'type': 'PUTn',
    'count': 4,
    'decoder_func': 'DoUniformMatrix2fv',
    'unit_test': False,
  },
  'UniformMatrix2x3fv': {
    'type': 'PUTn',
    'count': 6,
    'decoder_func': 'DoUniformMatrix2x3fv',
    'es3': True,
  },
  'UniformMatrix2x4fv': {
    'type': 'PUTn',
    'count': 8,
    'decoder_func': 'DoUniformMatrix2x4fv',
    'es3': True,
  },
  'UniformMatrix3fv': {
    'type': 'PUTn',
    'count': 9,
    'decoder_func': 'DoUniformMatrix3fv',
    'unit_test': False,
  },
  'UniformMatrix3x2fv': {
    'type': 'PUTn',
    'count': 6,
    'decoder_func': 'DoUniformMatrix3x2fv',
    'es3': True,
  },
  'UniformMatrix3x4fv': {
    'type': 'PUTn',
    'count': 12,
    'decoder_func': 'DoUniformMatrix3x4fv',
    'es3': True,
  },
  'UniformMatrix4fv': {
    'type': 'PUTn',
    'count': 16,
    'decoder_func': 'DoUniformMatrix4fv',
    'unit_test': False,
  },
  'UniformMatrix4x2fv': {
    'type': 'PUTn',
    'count': 8,
    'decoder_func': 'DoUniformMatrix4x2fv',
    'es3': True,
  },
  'UniformMatrix4x3fv': {
    'type': 'PUTn',
    'count': 12,
    'decoder_func': 'DoUniformMatrix4x3fv',
    'es3': True,
  },
  'UniformBlockBinding': {
    'type': 'Custom',
    'impl_func': False,
    'es3': True,
  },
  'UnmapBufferCHROMIUM': {
    'type': 'NoCommand',
    'extension': "CHROMIUM_pixel_transfer_buffer_object",
    'trace_level': 1,
  },
  'UnmapBufferSubDataCHROMIUM': {
    'type': 'NoCommand',
    'extension': 'CHROMIUM_map_sub',
    'pepper_interface': 'ChromiumMapSub',
    'trace_level': 1,
  },
  'UnmapBuffer': {
    'type': 'Custom',
    'es3': True,
    'trace_level': 1,
  },
  'UnmapTexSubImage2DCHROMIUM': {
    'type': 'NoCommand',
    'extension': "CHROMIUM_sub_image",
    'pepper_interface': 'ChromiumMapSub',
    'trace_level': 1,
  },
  'UseProgram': {
    'type': 'Bind',
    'decoder_func': 'DoUseProgram',
  },
  'ValidateProgram': {'decoder_func': 'DoValidateProgram'},
  'VertexAttrib1f': {'decoder_func': 'DoVertexAttrib1f'},
  'VertexAttrib1fv': {
    'type': 'PUT',
    'count': 1,
    'decoder_func': 'DoVertexAttrib1fv',
  },
  'VertexAttrib2f': {'decoder_func': 'DoVertexAttrib2f'},
  'VertexAttrib2fv': {
    'type': 'PUT',
    'count': 2,
    'decoder_func': 'DoVertexAttrib2fv',
  },
  'VertexAttrib3f': {'decoder_func': 'DoVertexAttrib3f'},
  'VertexAttrib3fv': {
    'type': 'PUT',
    'count': 3,
    'decoder_func': 'DoVertexAttrib3fv',
  },
  'VertexAttrib4f': {'decoder_func': 'DoVertexAttrib4f'},
  'VertexAttrib4fv': {
    'type': 'PUT',
    'count': 4,
    'decoder_func': 'DoVertexAttrib4fv',
  },
  'VertexAttribI4i': {
    'es3': True,
    'decoder_func': 'DoVertexAttribI4i',
  },
  'VertexAttribI4iv': {
    'type': 'PUT',
    'count': 4,
    'es3': True,
    'decoder_func': 'DoVertexAttribI4iv',
  },
  'VertexAttribI4ui': {
    'es3': True,
    'decoder_func': 'DoVertexAttribI4ui',
  },
  'VertexAttribI4uiv': {
    'type': 'PUT',
    'count': 4,
    'es3': True,
    'decoder_func': 'DoVertexAttribI4uiv',
  },
  'VertexAttribIPointer': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLuint indx, GLintVertexAttribSize size, '
                'GLenumVertexAttribIType type, GLsizei stride, '
                'GLuint offset',
    'client_test': False,
    'es3': True,
  },
  'VertexAttribPointer': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLuint indx, GLintVertexAttribSize size, '
                'GLenumVertexAttribType type, GLboolean normalized, '
                'GLsizei stride, GLuint offset',
    'client_test': False,
  },
  'WaitSync': {
    'type': 'Custom',
    'cmd_args': 'GLuint sync, GLbitfieldSyncFlushFlags flags, '
                'GLuint64 timeout',
    'impl_func': False,
    'client_test': False,
    'es3': True,
    'trace_level': 1,
  },
  'Scissor': {
    'type': 'StateSet',
    'state': 'Scissor',
    'decoder_func': 'DoScissor',
  },
  'Viewport': {
    'impl_func': False,
    'decoder_func': 'DoViewport',
  },
  'ResizeCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'client_test': False,
    'cmd_args': 'GLint width, GLint height, GLfloat scale_factor, GLboolean '
                'alpha, GLuint shm_id, GLuint shm_offset, GLsizei '
                'color_space_size',
    'extension': True,
    'trace_level': 1,
  },
  'GetRequestableExtensionsCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'uint32_t bucket_id',
    'extension': True,
  },
  'RequestExtensionCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'client_test': False,
    'cmd_args': 'uint32_t bucket_id',
    'extension': 'CHROMIUM_request_extension',
  },
  'CopyTextureCHROMIUM': {
    'decoder_func': 'DoCopyTextureCHROMIUM',
    'unit_test': False,
    'extension': "CHROMIUM_copy_texture",
    'trace_level': 2,
  },
  'CopySubTextureCHROMIUM': {
    'decoder_func': 'DoCopySubTextureCHROMIUM',
    'unit_test': False,
    'extension': "CHROMIUM_copy_texture",
    'trace_level': 2,
  },
  'TexStorage2DEXT': {
    'unit_test': False,
    'extension': 'EXT_texture_storage',
    'extension_flag': 'ext_texture_storage',
    'decoder_func': 'DoTexStorage2DEXT',
    'trace_level': 2,
  },
  'DrawArraysInstancedANGLE': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumDrawMode mode, GLint first, GLsizei count, '
                'GLsizei primcount',
    'extension': 'ANGLE_instanced_arrays',
    'pepper_interface': 'InstancedArrays',
    'trace_level': 2,
  },
  'DrawArraysInstancedBaseInstanceANGLE': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumDrawMode mode, GLint first, GLsizei count, '
                'GLsizei primcount, GLuint baseinstance',
    'extension': 'ANGLE_base_vertex_base_instance',
    'trace_level': 2,
  },
  'DrawBuffersEXT': {
    'type': 'PUTn',
    'decoder_func': 'DoDrawBuffersEXT',
    'count': 1,
    'unit_test': False,
    # could use 'extension_flag': 'ext_draw_buffers' but currently expected to
    # work without.
    'extension': 'EXT_draw_buffers',
    'pepper_interface': 'DrawBuffers',
    'trace_level': 2,
  },
  'DrawElementsInstancedANGLE': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumDrawMode mode, GLsizei count, '
                'GLenumIndexType type, GLuint index_offset, GLsizei primcount',
    'extension': 'ANGLE_instanced_arrays',
    'client_test': False,
    'pepper_interface': 'InstancedArrays',
    'trace_level': 2,
  },
  'DrawElementsInstancedBaseVertexBaseInstanceANGLE': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumDrawMode mode, GLsizei count, '
                'GLenumIndexType type, GLuint index_offset, GLsizei primcount, '
                'GLint basevertex, GLuint baseinstance',
    'extension': 'ANGLE_base_vertex_base_instance',
    'client_test': False,
    'trace_level': 2,
  },
  'VertexAttribDivisorANGLE': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLuint index, GLuint divisor',
    'extension': 'ANGLE_instanced_arrays',
    'pepper_interface': 'InstancedArrays',
  },
  'GenQueriesEXT': {
    'type': 'GENn',
    'gl_test_func': 'glGenQueriesARB',
    'resource_type': 'Query',
    'resource_types': 'Queries',
    'unit_test': False,
    'pepper_interface': 'Query',
    'not_shared': 'True',
    'extension': "occlusion_query_EXT",
  },
  'DeleteQueriesEXT': {
    'type': 'DELn',
    'gl_test_func': 'glDeleteQueriesARB',
    'resource_type': 'Query',
    'resource_types': 'Queries',
    'unit_test': False,
    'pepper_interface': 'Query',
    'extension': "occlusion_query_EXT",
  },
  'IsQueryEXT': {
    'type': 'NoCommand',
    'pepper_interface': 'Query',
    'extension': "occlusion_query_EXT",
  },
  'BeginQueryEXT': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumQueryTarget target, GLidQuery id, void* sync_data',
    'data_transfer_methods': ['shm'],
    'gl_test_func': 'glBeginQuery',
    'pepper_interface': 'Query',
    'extension': "occlusion_query_EXT",
  },
  'BeginTransformFeedback': {
    'decoder_func': 'DoBeginTransformFeedback',
    'unit_test': False,
    'es3': True,
  },
  'EndQueryEXT': {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLenumQueryTarget target, GLuint submit_count',
    'gl_test_func': 'glEndnQuery',
    'client_test': False,
    'pepper_interface': 'Query',
    'extension': "occlusion_query_EXT",
  },
  'EndTransformFeedback': {
    'decoder_func': 'DoEndTransformFeedback',
    'unit_test': False,
    'es3': True,
  },
  'FlushDriverCachesCHROMIUM': {
    'decoder_func': 'DoFlushDriverCachesCHROMIUM',
    'unit_test': False,
    'extension': True,
    'trace_level': 1,
  },
  'GetQueryivEXT': {
    'type': 'NoCommand',
    'gl_test_func': 'glGetQueryiv',
    'pepper_interface': 'Query',
    'extension': "occlusion_query_EXT",
  },
  'QueryCounterEXT' : {
    'type': 'Custom',
    'impl_func': False,
    'cmd_args': 'GLidQuery id, GLenumQueryTarget target, '
                'void* sync_data, GLuint submit_count',
    'data_transfer_methods': ['shm'],
    'gl_test_func': 'glQueryCounter',
    'extension': "disjoint_timer_query_EXT",
  },
  'GetQueryObjectivEXT': {
    'type': 'NoCommand',
    'gl_test_func': 'glGetQueryObjectiv',
    'extension': "disjoint_timer_query_EXT",
  },
  'GetQueryObjectuivEXT': {
    'type': 'NoCommand',
    'gl_test_func': 'glGetQueryObjectuiv',
    'pepper_interface': 'Query',
    'extension': "occlusion_query_EXT",
  },
  'GetQueryObjecti64vEXT': {
    'type': 'NoCommand',
    'gl_test_func': 'glGetQueryObjecti64v',
    'extension': "disjoint_timer_query_EXT",
  },
  'GetQueryObjectui64vEXT': {
    'type': 'NoCommand',
    'gl_test_func': 'glGetQueryObjectui64v',
    'extension': "disjoint_timer_query_EXT",
  },
  'SetDisjointValueSyncCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'data_transfer_methods': ['shm'],
    'client_test': False,
    'cmd_args': 'void* sync_data',
    'extension': True,
  },
  'BindFragDataLocationEXT': {
    'type': 'GLchar',
    'data_transfer_methods': ['bucket'],
    'needs_size': True,
    'gl_test_func': 'DoBindFragDataLocationEXT',
    'extension': 'EXT_blend_func_extended',
    'extension_flag': 'ext_blend_func_extended',
  },
  'BindFragDataLocationIndexedEXT': {
    'type': 'GLchar',
    'data_transfer_methods': ['bucket'],
    'needs_size': True,
    'gl_test_func': 'DoBindFragDataLocationIndexedEXT',
    'extension': 'EXT_blend_func_extended',
    'extension_flag': 'ext_blend_func_extended',
  },
  'BindUniformLocationCHROMIUM': {
    'type': 'GLchar',
    'extension': 'CHROMIUM_bind_uniform_location',
    'data_transfer_methods': ['bucket'],
    'needs_size': True,
    'gl_test_func': 'DoBindUniformLocationCHROMIUM',
  },
  'InsertEventMarkerEXT': {
    'type': 'GLcharN',
    'decoder_func': 'DoInsertEventMarkerEXT',
    'expectation': False,
    'extension': 'EXT_debug_marker',
  },
  'PushGroupMarkerEXT': {
    'type': 'GLcharN',
    'decoder_func': 'DoPushGroupMarkerEXT',
    'expectation': False,
    'extension': 'EXT_debug_marker',
  },
  'PopGroupMarkerEXT': {
    'decoder_func': 'DoPopGroupMarkerEXT',
    'expectation': False,
    'extension': 'EXT_debug_marker',
    'impl_func': False,
  },

  'GenVertexArraysOES': {
    'type': 'GENn',
    'extension': 'OES_vertex_array_object',
    'gl_test_func': 'glGenVertexArraysOES',
    'resource_type': 'VertexArray',
    'resource_types': 'VertexArrays',
    'unit_test': False,
    'pepper_interface': 'VertexArrayObject',
    'not_shared': 'True',
  },
  'BindVertexArrayOES': {
    'type': 'Bind',
    'extension': 'OES_vertex_array_object',
    'gl_test_func': 'glBindVertexArrayOES',
    'decoder_func': 'DoBindVertexArrayOES',
    'gen_func': 'GenVertexArraysOES',
    'unit_test': False,
    'client_test': False,
    'pepper_interface': 'VertexArrayObject',
  },
  'DeleteVertexArraysOES': {
    'type': 'DELn',
    'extension': 'OES_vertex_array_object',
    'gl_test_func': 'glDeleteVertexArraysOES',
    'resource_type': 'VertexArray',
    'resource_types': 'VertexArrays',
    'unit_test': False,
    'pepper_interface': 'VertexArrayObject',
  },
  'IsVertexArrayOES': {
    'type': 'Is',
    'extension': 'OES_vertex_array_object',
    'gl_test_func': 'glIsVertexArrayOES',
    'decoder_func': 'DoIsVertexArrayOES',
    'unit_test': False,
    'pepper_interface': 'VertexArrayObject',
  },
  'ShallowFinishCHROMIUM': {
    'type': 'NoCommand',
    'extension': 'CHROMIUM_ordering_barrier',
  },
  'OrderingBarrierCHROMIUM': {
    'type': 'NoCommand',
    'extension': 'CHROMIUM_ordering_barrier',
  },
  'TraceBeginCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'client_test': False,
    'cmd_args': 'GLuint category_bucket_id, GLuint name_bucket_id',
    'extension': 'CHROMIUM_trace_marker',
  },
  'TraceEndCHROMIUM': {
    'impl_func': False,
    'client_test': False,
    'decoder_func': 'DoTraceEndCHROMIUM',
    'unit_test': False,
    'extension': 'CHROMIUM_trace_marker',
  },
  'SetActiveURLCHROMIUM': {
    'type': 'Custom',
    'impl_func': False,
    'client_test': False,
    'cmd_args': 'GLuint url_bucket_id',
    'extension': True,
    'chromium': True,
  },
  'DiscardFramebufferEXT': {
    'type': 'PUTn',
    'count': 1,
    'decoder_func': 'DoDiscardFramebufferEXT',
    'unit_test': False,
    'extension': 'EXT_discard_framebuffer',
    'extension_flag': 'ext_discard_framebuffer',
    'trace_level': 2,
  },
  'LoseContextCHROMIUM': {
    'decoder_func': 'DoLoseContextCHROMIUM',
    'unit_test': False,
    'extension': 'CHROMIUM_lose_context',
    'trace_level': 1,
  },
  'InitializeDiscardableTextureCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLuint texture_id, uint32_t shm_id, '
                'uint32_t shm_offset',
    'impl_func': False,
    'client_test': False,
    'extension': True,
  },
  'UnlockDiscardableTextureCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLuint texture_id',
    'impl_func': False,
    'client_test': False,
    'extension': True,
  },
  'LockDiscardableTextureCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLuint texture_id',
    'impl_func': False,
    'client_test': False,
    'extension': True,
  },
  'WindowRectanglesEXT': {
    'type': 'PUTn',
    'count': 4,
    'decoder_func': 'DoWindowRectanglesEXT',
    'unit_test': False,
    'extension': 'EXT_window_rectangles',
    'extension_flag': 'ext_window_rectangles',
    'es3': True,
  },
  'CreateGpuFenceCHROMIUM': {
    'type': 'NoCommand',
    'impl_func': False,
    'cmd_args': 'void',
    'result': ['GLuint'],
    'extension': 'CHROMIUM_gpu_fence',
  },
  'CreateGpuFenceINTERNAL': {
    'type': 'Custom',
    'cmd_args': 'GLuint gpu_fence_id',
    'extension': 'CHROMIUM_gpu_fence',
    'extension_flag': 'chromium_gpu_fence',
    'internal': True,
  },
  'CreateClientGpuFenceCHROMIUM': {
    'type': 'NoCommand',
    'impl_func': False,
    'cmd_args': 'ClientGpuFence source',
    'result': ['GLuint'],
    'extension': 'CHROMIUM_gpu_fence',
    'extension_flag': 'chromium_gpu_fence',
  },
  'WaitGpuFenceCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLuint gpu_fence_id',
    'extension': 'CHROMIUM_gpu_fence',
    'extension_flag': 'chromium_gpu_fence',
  },
  'DestroyGpuFenceCHROMIUM': {
    'type': 'Custom',
    'cmd_args': 'GLuint gpu_fence_id',
    'extension': 'CHROMIUM_gpu_fence',
    'extension_flag': 'chromium_gpu_fence',
  },
  'InvalidateReadbackBufferShadowDataCHROMIUM': {
    'type': 'NoCommand',
    'impl_func': False,
    'es3': True,
    'extension': 'CHROMIUM_nonblocking_readback',
  },
  'SetReadbackBufferShadowAllocationINTERNAL': {
    'decoder_func': 'DoSetReadbackBufferShadowAllocationINTERNAL',
    'client_test': False,
    'unit_test': False,
    'impl_func': True,
    'internal': True,
    'es3': True,
  },
  'FramebufferParameteri': {
    'decoder_func': 'DoFramebufferParameteri',
    'unit_test': False,
    'extension': 'MESA_framebuffer_flip_y',
    'extension_flag': 'mesa_framebuffer_flip_y',
  },
  'FramebufferTextureMultiviewOVR': {
    'decoder_func': 'DoFramebufferTextureMultiviewOVR',
    'unit_test': False,
    'extension': 'OVR_multiview2',
    'extension_flag': 'ovr_multiview2',
    'trace_level': 1,
    'es3': True
  },
  'MaxShaderCompilerThreadsKHR': {
    'cmd_args': 'GLuint count',
    'unit_test': False,
    'client_test': False,
    'extension': 'KHRParallelShaderCompile',
    'extension_flag': 'khr_parallel_shader_compile',
  },
  'CreateAndTexStorage2DSharedImageCHROMIUM': {
    'type': 'NoCommand',
    'extension': "CHROMIUM_shared_image",
    'trace_level': 2,
  },
  'CreateAndTexStorage2DSharedImageINTERNAL': {
    'decoder_func': 'DoCreateAndTexStorage2DSharedImageINTERNAL',
    'internal': True,
    'type': 'PUT',
    'count': 16,  # GL_MAILBOX_SIZE_CHROMIUM
    'impl_func': False,
    'unit_test': False,
    'trace_level': 2,
  },
  'BeginSharedImageAccessDirectCHROMIUM': {
    'decoder_func': 'DoBeginSharedImageAccessDirectCHROMIUM',
    'extension': 'CHROMIUM_shared_image',
    'unit_test': False,
    'client_test': False,
    'cmd_args': 'GLuint texture, GLenumSharedImageAccessMode mode',
  },
  'EndSharedImageAccessDirectCHROMIUM': {
    'decoder_func': 'DoEndSharedImageAccessDirectCHROMIUM',
    'extension': 'CHROMIUM_shared_image',
    'unit_test': False,
  },
  # NOTE: Following functions are given an INTERNAL suffix but they're not
  # truly 'internal'. This is because they are to be accessed as client only
  # from RasterImplementationGLES to be used with Passthrough Command Decoder.
  # Also, they have similar implementations to corresponding functions for
  # Raster Decoder.
  'CopySharedImageINTERNAL': {
    'decoder_func': 'DoCopySharedImageINTERNAL',
    'extension': 'CHROMIUM_shared_image',
    'internal': False,
    'type': 'PUT',
    'count': 32, #GL_MAILBOX_SIZE_CHROMIUM x2
    'impl_func': True,
    'unit_test': False,
    'trace_level': 2,
  },
  'CopySharedImageToTextureINTERNAL': {
    'decoder_func': 'DoCopySharedImageToTextureINTERNAL',
    'extension': 'CHROMIUM_shared_image',
    'internal': False,
    'type': 'PUT',
    'count': 16, #GL_MAILBOX_SIZE_CHROMIUM
    'impl_func': True,
    'unit_test': False,
    'trace_level': 2,
  },
  # mailbox_offset refers to the offset in shared memory pointing to shared
  # image mailbox.
  'ReadbackARGBImagePixelsINTERNAL': {
    'type': 'Custom',
    'extension': 'CHROMIUM_shared_image',
    'impl_func': False,
    'client_test': False,
    'cmd_args':
        'GLint src_x, GLint src_y, GLint plane_index, GLuint dst_width, '
        'GLuint dst_height, GLuint row_bytes, GLuint dst_sk_color_type, '
        'GLuint dst_sk_alpha_type, GLint shm_id, GLuint shm_offset, '
        'GLuint color_space_offset, GLuint pixels_offset, '
        'GLuint mailbox_offset',
    'result': ['uint32_t'],
    'trace_level': 2,
  },
  # mailbox_offset refers to the offset in shared memory pointing to shared
  # image mailbox.
  'WritePixelsYUVINTERNAL': {
    'type': 'Custom',
    'extension': 'CHROMIUM_shared_image',
    'impl_func': False,
    'client_test': False,
    'cmd_args':
        'GLuint src_width, GLuint src_height, GLuint src_row_bytes_plane1, '
        'GLuint src_row_bytes_plane2, GLuint src_row_bytes_plane3, '
        'GLuint src_row_bytes_plane4, GLuint src_yuv_plane_config, '
        'GLuint src_yuv_subsampling, GLuint src_yuv_datatype, GLint shm_id, '
        'GLuint shm_offset, GLuint pixels_offset_plane1, '
        'GLuint pixels_offset_plane2, GLuint pixels_offset_plane3, '
        'GLuint pixels_offset_plane4',
    'trace_level': 2,
  },
  'FramebufferMemorylessPixelLocalStorageANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'unit_test': False,
    'es3': True,
    'decoder_func': 'DoFramebufferMemorylessPixelLocalStorageANGLE',
  },
  'FramebufferTexturePixelLocalStorageANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'unit_test': False,
    'es3': True,
    'decoder_func': 'DoFramebufferTexturePixelLocalStorageANGLE',
  },
  'FramebufferPixelLocalClearValuefvANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'type': 'PUT',
    'count': 4,
    'unit_test': False,
    'es3': True,
    'decoder_func': 'DoFramebufferPixelLocalClearValuefvANGLE',
  },
  'FramebufferPixelLocalClearValueivANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'type': 'PUT',
    'count': 4,
    'unit_test': False,
    'es3': True,
    'decoder_func': 'DoFramebufferPixelLocalClearValueivANGLE',
  },
  'FramebufferPixelLocalClearValueuivANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'type': 'PUT',
    'use_count_func': True,
    'count': 4,
    'unit_test': False,
    'es3': True,
    'decoder_func': 'DoFramebufferPixelLocalClearValueuivANGLE',
  },
  'BeginPixelLocalStorageANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'type': 'PUTn',
    'count': 1,
    'unit_test': False,
    'es3': True,
    'decoder_func': 'DoBeginPixelLocalStorageANGLE',
  },
  'EndPixelLocalStorageANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'type': 'PUTn',
    'count': 1,
    'unit_test': False,
    'es3': True,
    'decoder_func': 'DoBeginPixelLocalStorageANGLE',
  },
  'PixelLocalStorageBarrierANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'unit_test': False,
    'es3': True,
    'decoder_func': 'DoPixelLocalStorageBarrierANGLE',
  },
  'FramebufferPixelLocalStorageInterruptANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'unit_test': False,
    'es3': True,
    'decoder_func': 'DoFramebufferPixelLocalStorageInterruptANGLE',
  },
  'FramebufferPixelLocalStorageRestoreANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'unit_test': False,
    'es3': True,
    'decoder_func': 'DoFramebufferPixelLocalStorageRestoreANGLE',
  },
  'GetFramebufferPixelLocalStorageParameterfvANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'type': 'GETn',
    'unit_test': False,
    'es3': True,
    'result': ['SizedResult<GLfloat>'],
    'decoder_func': 'DoGetFramebufferPixelLocalStorageParameterfvANGLE',
  },
  'GetFramebufferPixelLocalStorageParameterivANGLE': {
    'extension': 'ANGLE_shader_pixel_local_storage',
    'extension_flag': 'angle_shader_pixel_local_storage',
    'type': 'GETn',
    'unit_test': False,
    'es3': True,
    'result': ['SizedResult<GLint>'],
    'decoder_func': 'DoGetFramebufferPixelLocalStorageParameterivANGLE',
  },

}


def main(argv):
  """This is the main function."""
  parser = OptionParser()
  parser.add_option(
      "--output-dir",
      help="Output directory for generated files. Defaults to chromium root "
      "directory.")
  parser.add_option(
      "-v", "--verbose", action="store_true", help="Verbose logging output.")
  parser.add_option(
      "-c", "--check", action="store_true",
      help="Check if output files match generated files in chromium root "
      "directory.  Use this in PRESUBMIT scripts with --output-dir.")

  (options, _) = parser.parse_args(args=argv)

  # Add in states and capabilites to GLState
  gl_state_valid = _NAMED_TYPE_INFO['GLState']['valid']
  gl_state_valid_es3 = _NAMED_TYPE_INFO['GLState']['valid_es3']
  for state_name in sorted(build_cmd_buffer_lib._STATE_INFO):
    state = build_cmd_buffer_lib._STATE_INFO[state_name]
    if 'extension_flag' in state:
      continue
    if 'enum' in state:
      if not state['enum'] in gl_state_valid:
        gl_state_valid.append(state['enum'])
    else:
      for item in state['states']:
        if 'extension_flag' in item:
          continue
        if 'es3' in item:
          assert item['es3']
          if not item['enum'] in gl_state_valid_es3:
            gl_state_valid_es3.append(item['enum'])
        else:
          if not item['enum'] in gl_state_valid:
            gl_state_valid.append(item['enum'])
  for capability in build_cmd_buffer_lib._CAPABILITY_FLAGS:
    if 'extension_flag' in capability:
      continue
    valid_value = "GL_%s" % capability['name'].upper()
    if not valid_value in gl_state_valid:
      gl_state_valid.append(valid_value)

  # This script lives under src/gpu/command_buffer.
  script_dir = os.path.dirname(os.path.abspath(__file__))
  assert script_dir.endswith((os.path.normpath("src/gpu/command_buffer"),
                              os.path.normpath("chromium/gpu/command_buffer")))
  # os.path.join doesn't do the right thing with relative paths.
  chromium_root_dir = os.path.abspath(script_dir + "/../..")

  # Support generating files under gen/ and for PRESUBMIT.
  if options.output_dir:
    output_dir = options.output_dir
  else:
    output_dir = chromium_root_dir
  os.chdir(output_dir)

  build_cmd_buffer_lib.InitializePrefix("GLES2")
  gen = build_cmd_buffer_lib.GLGenerator(
      options.verbose, "2014", _FUNCTION_INFO, _NAMED_TYPE_INFO,
      chromium_root_dir)
  gen.ParseGLH("gpu/command_buffer/gles2_cmd_buffer_functions.txt")

  gen.WritePepperGLES2Interface("ppapi/api/ppb_opengles2.idl", False)
  gen.WritePepperGLES2Interface("ppapi/api/dev/ppb_opengles2ext_dev.idl", True)
  gen.WriteGLES2ToPPAPIBridge("ppapi/lib/gl/gles2/gles2.c")
  gen.WritePepperGLES2Implementation(
      "ppapi/shared_impl/ppb_opengles2_shared.cc")
  gen.WriteCommandIds("gpu/command_buffer/common/gles2_cmd_ids_autogen.h")
  gen.WriteFormat("gpu/command_buffer/common/gles2_cmd_format_autogen.h")
  gen.WriteFormatTest(
    "gpu/command_buffer/common/gles2_cmd_format_test_autogen.h")
  gen.WriteGLES2InterfaceHeader(
    "gpu/command_buffer/client/gles2_interface_autogen.h")
  gen.WriteGLES2InterfaceStub(
    "gpu/command_buffer/client/gles2_interface_stub_autogen.h")
  gen.WriteGLES2InterfaceStubImpl(
      "gpu/command_buffer/client/gles2_interface_stub_impl_autogen.h")
  gen.WriteGLES2ImplementationHeader(
    "gpu/command_buffer/client/gles2_implementation_autogen.h")
  gen.WriteGLES2Implementation(
    "gpu/command_buffer/client/gles2_implementation_impl_autogen.h")
  gen.WriteGLES2ImplementationUnitTests(
      "gpu/command_buffer/client/gles2_implementation_unittest_autogen.h")
  gen.WriteGLES2TraceImplementationHeader(
      "gpu/command_buffer/client/gles2_trace_implementation_autogen.h")
  gen.WriteGLES2TraceImplementation(
      "gpu/command_buffer/client/gles2_trace_implementation_impl_autogen.h")
  gen.WriteGLES2CLibImplementation(
    "gpu/command_buffer/client/gles2_c_lib_autogen.h")
  gen.WriteCmdHelperHeader(
    "gpu/command_buffer/client/gles2_cmd_helper_autogen.h")
  gen.WriteServiceImplementation(
    "gpu/command_buffer/service/gles2_cmd_decoder_autogen.h")
  gen.WritePassthroughServiceImplementation(
    "gpu/command_buffer/service/" +
    "gles2_cmd_decoder_passthrough_handlers_autogen.cc")
  gen.WriteServiceContextStateHeader(
    "gpu/command_buffer/service/context_state_autogen.h")
  gen.WriteServiceContextStateImpl(
    "gpu/command_buffer/service/context_state_impl_autogen.h")
  gen.WriteServiceContextStateTestHelpers(
    "gpu/command_buffer/service/context_state_test_helpers_autogen.h")
  gen.WriteClientContextStateHeader(
    "gpu/command_buffer/client/client_context_state_autogen.h")
  gen.WriteClientContextStateImpl(
      "gpu/command_buffer/client/client_context_state_impl_autogen.h")
  gen.WriteServiceUnitTests(
    "gpu/command_buffer/service/gles2_cmd_decoder_unittest_%d_autogen.h")
  gen.WriteServiceUnitTestsForExtensions(
    "gpu/command_buffer/service/"
    "gles2_cmd_decoder_unittest_extensions_autogen.h")
  gen.WriteServiceUtilsHeader(
    "gpu/command_buffer/service/gles2_cmd_validation_autogen.h")
  gen.WriteServiceUtilsImplementation(
    "gpu/command_buffer/service/"
    "gles2_cmd_validation_implementation_autogen.h")
  gen.WriteCommonUtilsHeader(
    "gpu/command_buffer/common/gles2_cmd_utils_autogen.h")
  gen.WriteCommonUtilsImpl(
    "gpu/command_buffer/common/gles2_cmd_utils_implementation_autogen.h")
  gen.WriteGLES2Header("gpu/GLES2/gl2chromium_autogen.h")

  build_cmd_buffer_lib.Format(gen.generated_cpp_filenames, output_dir,
                              chromium_root_dir)

  if gen.errors > 0:
    print("build_gles2_cmd_buffer.py: Failed with %d errors" % gen.errors)
    return 1

  check_failed_filenames = []
  if options.check:
    for filename in gen.generated_cpp_filenames:
      if not filecmp.cmp(os.path.join(output_dir, filename),
                         os.path.join(chromium_root_dir, filename)):
        check_failed_filenames.append(filename)

  if len(check_failed_filenames) > 0:
    print('Please run gpu/command_buffer/build_gles2_cmd_buffer.py')
    print('Failed check on autogenerated command buffer files:')
    for filename in check_failed_filenames:
      print(filename)
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
