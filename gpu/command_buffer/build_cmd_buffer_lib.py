# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common code generator for command buffers."""

import errno
import itertools
import os
import os.path
import re
import platform
from subprocess import call

_SIZE_OF_UINT32 = 4
_SIZE_OF_COMMAND_HEADER = 4
_FIRST_SPECIFIC_COMMAND_ID = 256

_LICENSE = """// Copyright %s The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"""

_DO_NOT_EDIT_WARNING = """// This file is auto-generated from
// gpu/command_buffer/build_%s_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

"""

# TODO(crbug.com/40285824): Remove this and generate code using safer
# constructs.
_ALLOW_UNSAFE_BUFFERS = """

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

"""
_allow_unsafe_buffers_filenames = [
    "gpu/command_buffer/client/gles2_implementation_impl_autogen.h",
    "gpu/command_buffer/client/gles2_implementation_unittest_autogen.h",
    "gpu/command_buffer/client/raster_implementation_impl_autogen.h",
    "gpu/command_buffer/client/raster_implementation_unittest_autogen.h",
    "gpu/command_buffer/common/gles2_cmd_format_autogen.h",
    "gpu/command_buffer/service/context_state_impl_autogen.h",
    "gpu/command_buffer/service/gles2_cmd_decoder_autogen.h",
    "gpu/command_buffer/service/gles2_cmd_decoder_unittest_2_autogen.h",
    "gpu/command_buffer/service/raster_decoder_autogen.h",
]

# This string is copied directly out of the gl2.h file from GLES2.0
#
# Edits:
#
# *) Any argument that is a resourceID has been changed to GLid<Type>.
#    (not pointer arguments) and if it's allowed to be zero it's GLidZero<Type>
#    If it's allowed to not exist it's GLidBind<Type>
#
# *) All GLenums have been changed to GLenumTypeOfEnum
#
_GL_TYPES = {
  'GLenum': 'unsigned int',
  'GLboolean': 'unsigned char',
  'GLbitfield': 'unsigned int',
  'GLbyte': 'signed char',
  'GLshort': 'short',
  'GLint': 'int',
  'GLsizei': 'int',
  'GLubyte': 'unsigned char',
  'GLushort': 'unsigned short',
  'GLuint': 'unsigned int',
  'GLfloat': 'float',
  'GLclampf': 'float',
  'GLvoid': 'void',
  'GLfixed': 'int',
  'GLclampx': 'int'
}

_GL_TYPES_32 = {
  'GLintptr': 'long int',
  'GLsizeiptr': 'long int'
}

_GL_TYPES_64 = {
  'GLintptr': 'long long int',
  'GLsizeiptr': 'long long int'
}

_ETC_COMPRESSED_TEXTURE_FORMATS = [
  'GL_COMPRESSED_R11_EAC',
  'GL_COMPRESSED_SIGNED_R11_EAC',
  'GL_COMPRESSED_RG11_EAC',
  'GL_COMPRESSED_SIGNED_RG11_EAC',
  'GL_COMPRESSED_RGB8_ETC2',
  'GL_COMPRESSED_SRGB8_ETC2',
  'GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2',
  'GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2',
  'GL_COMPRESSED_RGBA8_ETC2_EAC',
  'GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC',
]

# This table specifies the different pepper interfaces that are supported for
# GL commands. 'dev' is true if it's a dev interface.
_PEPPER_INTERFACES = [
  {'name': '', 'dev': False},
  {'name': 'InstancedArrays', 'dev': False},
  {'name': 'FramebufferBlit', 'dev': False},
  {'name': 'FramebufferMultisample', 'dev': False},
  {'name': 'ChromiumEnableFeature', 'dev': False},
  {'name': 'ChromiumMapSub', 'dev': False},
  {'name': 'Query', 'dev': False},
  {'name': 'VertexArrayObject', 'dev': False},
  {'name': 'DrawBuffers', 'dev': True},
]


# Capabilities selected with glEnable
# on_change:    string of C++ code that is executed when the state is changed.
_CAPABILITY_FLAGS = [
  {'name': 'blend'},
  {'name': 'cull_face'},
  {'name': 'depth_test',
    'on_change': 'framebuffer_state_.clear_state_dirty = true;'},
  {'name': 'dither', 'default': True},
  {'name': 'framebuffer_srgb_ext', 'default': True, 'no_init': True,
   'extension_flag': 'ext_srgb_write_control'},
  {'name': 'polygon_offset_fill'},
  {'name': 'sample_alpha_to_coverage'},
  {'name': 'sample_coverage'},
  {'name': 'scissor_test'},
  {'name': 'stencil_test',
    'on_change': '''state_.stencil_state_changed_since_validation = true;
                    framebuffer_state_.clear_state_dirty = true;'''},
  {'name': 'rasterizer_discard', 'es3': True},
  {'name': 'primitive_restart_fixed_index', 'es3': True},
  {'name': 'multisample_ext', 'default': True,
   'extension_flag': 'ext_multisample_compatibility'},
  {'name': 'sample_alpha_to_one_ext',
   'extension_flag': 'ext_multisample_compatibility'},
]

_STATE_INFO = {
  'ClearColor': {
    'type': 'Normal',
    'func': 'ClearColor',
    'enum': 'GL_COLOR_CLEAR_VALUE',
    'states': [
      {'name': 'color_clear_red', 'type': 'GLfloat', 'default': '0.0f'},
      {'name': 'color_clear_green', 'type': 'GLfloat', 'default': '0.0f'},
      {'name': 'color_clear_blue', 'type': 'GLfloat', 'default': '0.0f'},
      {'name': 'color_clear_alpha', 'type': 'GLfloat', 'default': '0.0f'},
    ],
  },
  'ClearDepthf': {
    'type': 'Normal',
    'func': 'ClearDepth',
    'enum': 'GL_DEPTH_CLEAR_VALUE',
    'states': [
      {'name': 'depth_clear', 'type': 'GLclampf', 'default': '1.0f'},
    ],
  },
  'ColorMask': {
    'type': 'Normal',
    'func': 'ColorMask',
    'enum': 'GL_COLOR_WRITEMASK',
    'states': [
      {
        'name': 'color_mask_red',
        'type': 'GLboolean',
        'default': 'true',
        'cached': True
      },
      {
        'name': 'color_mask_green',
        'type': 'GLboolean',
        'default': 'true',
        'cached': True
      },
      {
        'name': 'color_mask_blue',
        'type': 'GLboolean',
        'default': 'true',
        'cached': True
      },
      {
        'name': 'color_mask_alpha',
        'type': 'GLboolean',
        'default': 'true',
        'cached': True
      },
    ],
    'on_change': 'framebuffer_state_.clear_state_dirty = true;',
  },
  'ClearStencil': {
    'type': 'Normal',
    'func': 'ClearStencil',
    'enum': 'GL_STENCIL_CLEAR_VALUE',
    'states': [
      {'name': 'stencil_clear', 'type': 'GLint', 'default': '0'},
    ],
  },
  'BlendColor': {
    'type': 'Normal',
    'func': 'BlendColor',
    'enum': 'GL_BLEND_COLOR',
    'states': [
      {'name': 'blend_color_red', 'type': 'GLfloat', 'default': '0.0f'},
      {'name': 'blend_color_green', 'type': 'GLfloat', 'default': '0.0f'},
      {'name': 'blend_color_blue', 'type': 'GLfloat', 'default': '0.0f'},
      {'name': 'blend_color_alpha', 'type': 'GLfloat', 'default': '0.0f'},
    ],
  },
  'BlendEquation': {
    'type': 'SrcDst',
    'func': 'BlendEquationSeparate',
    'states': [
      {
        'name': 'blend_equation_rgb',
        'type': 'GLenum',
        'enum': 'GL_BLEND_EQUATION_RGB',
        'default': 'GL_FUNC_ADD',
      },
      {
        'name': 'blend_equation_alpha',
        'type': 'GLenum',
        'enum': 'GL_BLEND_EQUATION_ALPHA',
        'default': 'GL_FUNC_ADD',
      },
    ],
  },
  'BlendFunc': {
    'type': 'SrcDst',
    'func': 'BlendFuncSeparate',
    'states': [
      {
        'name': 'blend_source_rgb',
        'type': 'GLenum',
        'enum': 'GL_BLEND_SRC_RGB',
        'default': 'GL_ONE',
      },
      {
        'name': 'blend_dest_rgb',
        'type': 'GLenum',
        'enum': 'GL_BLEND_DST_RGB',
        'default': 'GL_ZERO',
      },
      {
        'name': 'blend_source_alpha',
        'type': 'GLenum',
        'enum': 'GL_BLEND_SRC_ALPHA',
        'default': 'GL_ONE',
      },
      {
        'name': 'blend_dest_alpha',
        'type': 'GLenum',
        'enum': 'GL_BLEND_DST_ALPHA',
        'default': 'GL_ZERO',
      },
    ],
  },
  'PolygonOffset': {
    'type': 'Normal',
    'func': 'PolygonOffset',
    'states': [
      {
        'name': 'polygon_offset_factor',
        'type': 'GLfloat',
        'enum': 'GL_POLYGON_OFFSET_FACTOR',
        'default': '0.0f',
      },
      {
        'name': 'polygon_offset_units',
        'type': 'GLfloat',
        'enum': 'GL_POLYGON_OFFSET_UNITS',
        'default': '0.0f',
      },
    ],
  },
  'CullFace':  {
    'type': 'Normal',
    'func': 'CullFace',
    'enum': 'GL_CULL_FACE_MODE',
    'states': [
      {
        'name': 'cull_mode',
        'type': 'GLenum',
        'default': 'GL_BACK',
      },
    ],
  },
  'FrontFace': {
    'type': 'Normal',
    'func': 'FrontFace',
    'enum': 'GL_FRONT_FACE',
    'states': [{'name': 'front_face', 'type': 'GLenum', 'default': 'GL_CCW'}],
  },
  'DepthFunc': {
    'type': 'Normal',
    'func': 'DepthFunc',
    'enum': 'GL_DEPTH_FUNC',
    'states': [{'name': 'depth_func', 'type': 'GLenum', 'default': 'GL_LESS'}],
  },
  'DepthRange': {
    'type': 'Normal',
    'func': 'DepthRange',
    'enum': 'GL_DEPTH_RANGE',
    'states': [
      {'name': 'z_near', 'type': 'GLclampf', 'default': '0.0f'},
      {'name': 'z_far', 'type': 'GLclampf', 'default': '1.0f'},
    ],
  },
  'SampleCoverage': {
    'type': 'Normal',
    'func': 'SampleCoverage',
    'states': [
      {
        'name': 'sample_coverage_value',
        'type': 'GLclampf',
        'enum': 'GL_SAMPLE_COVERAGE_VALUE',
        'default': '1.0f',
      },
      {
        'name': 'sample_coverage_invert',
        'type': 'GLboolean',
        'enum': 'GL_SAMPLE_COVERAGE_INVERT',
        'default': 'false',
      },
    ],
  },
  'StencilMask': {
    'type': 'FrontBack',
    'func': 'StencilMaskSeparate',
    'states': [
      {
        'name': 'stencil_front_writemask',
        'type': 'GLuint',
        'enum': 'GL_STENCIL_WRITEMASK',
        'default': '0xFFFFFFFFU',
        'cached': True,
      },
      {
        'name': 'stencil_back_writemask',
        'type': 'GLuint',
        'enum': 'GL_STENCIL_BACK_WRITEMASK',
        'default': '0xFFFFFFFFU',
        'cached': True,
      },
    ],
    'on_change': '''framebuffer_state_.clear_state_dirty = true;
                    state_.stencil_state_changed_since_validation = true;''',
  },
  'StencilOp': {
    'type': 'FrontBack',
    'func': 'StencilOpSeparate',
    'states': [
      {
        'name': 'stencil_front_fail_op',
        'type': 'GLenum',
        'enum': 'GL_STENCIL_FAIL',
        'default': 'GL_KEEP',
      },
      {
        'name': 'stencil_front_z_fail_op',
        'type': 'GLenum',
        'enum': 'GL_STENCIL_PASS_DEPTH_FAIL',
        'default': 'GL_KEEP',
      },
      {
        'name': 'stencil_front_z_pass_op',
        'type': 'GLenum',
        'enum': 'GL_STENCIL_PASS_DEPTH_PASS',
        'default': 'GL_KEEP',
      },
      {
        'name': 'stencil_back_fail_op',
        'type': 'GLenum',
        'enum': 'GL_STENCIL_BACK_FAIL',
        'default': 'GL_KEEP',
      },
      {
        'name': 'stencil_back_z_fail_op',
        'type': 'GLenum',
        'enum': 'GL_STENCIL_BACK_PASS_DEPTH_FAIL',
        'default': 'GL_KEEP',
      },
      {
        'name': 'stencil_back_z_pass_op',
        'type': 'GLenum',
        'enum': 'GL_STENCIL_BACK_PASS_DEPTH_PASS',
        'default': 'GL_KEEP',
      },
    ],
  },
  'StencilFunc': {
    'type': 'FrontBack',
    'func': 'StencilFuncSeparate',
    'states': [
      {
        'name': 'stencil_front_func',
        'type': 'GLenum',
        'enum': 'GL_STENCIL_FUNC',
        'default': 'GL_ALWAYS',
      },
      {
        'name': 'stencil_front_ref',
        'type': 'GLint',
        'enum': 'GL_STENCIL_REF',
        'default': '0',
      },
      {
        'name': 'stencil_front_mask',
        'type': 'GLuint',
        'enum': 'GL_STENCIL_VALUE_MASK',
        'default': '0xFFFFFFFFU',
      },
      {
        'name': 'stencil_back_func',
        'type': 'GLenum',
        'enum': 'GL_STENCIL_BACK_FUNC',
        'default': 'GL_ALWAYS',
      },
      {
        'name': 'stencil_back_ref',
        'type': 'GLint',
        'enum': 'GL_STENCIL_BACK_REF',
        'default': '0',
      },
      {
        'name': 'stencil_back_mask',
        'type': 'GLuint',
        'enum': 'GL_STENCIL_BACK_VALUE_MASK',
        'default': '0xFFFFFFFFU',
      },
    ],
    'on_change': 'state_.stencil_state_changed_since_validation = true;',
  },
  'Hint': {
    'type': 'NamedParameter',
    'func': 'Hint',
    'states': [
      {
        'name': 'hint_generate_mipmap',
        'type': 'GLenum',
        'enum': 'GL_GENERATE_MIPMAP_HINT',
        'default': 'GL_DONT_CARE',
      },
      {
        'name': 'hint_fragment_shader_derivative',
        'type': 'GLenum',
        'enum': 'GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES',
        'default': 'GL_DONT_CARE',
        'extension_flag': 'oes_standard_derivatives'
      }
    ],
  },
  'PixelStore': {
    'type': 'NamedParameter',
    'func': 'PixelStorei',
    'states': [
      {
        'name': 'pack_alignment',
        'type': 'GLint',
        'enum': 'GL_PACK_ALIGNMENT',
        'default': '4'
      },
      {
        'name': 'unpack_alignment',
        'type': 'GLint',
        'enum': 'GL_UNPACK_ALIGNMENT',
        'default': '4'
      },
      {
        'name': 'pack_row_length',
        'type': 'GLint',
        'enum': 'GL_PACK_ROW_LENGTH',
        'default': '0',
        'es3': True,
        'manual': True,
      },
      {
        'name': 'pack_skip_pixels',
        'type': 'GLint',
        'enum': 'GL_PACK_SKIP_PIXELS',
        'default': '0',
        'es3': True,
        'manual': True,
      },
      {
        'name': 'pack_skip_rows',
        'type': 'GLint',
        'enum': 'GL_PACK_SKIP_ROWS',
        'default': '0',
        'es3': True,
        'manual': True,
      },
      {
        'name': 'unpack_row_length',
        'type': 'GLint',
        'enum': 'GL_UNPACK_ROW_LENGTH',
        'default': '0',
        'es3': True,
        'manual': True,
      },
      {
        'name': 'unpack_image_height',
        'type': 'GLint',
        'enum': 'GL_UNPACK_IMAGE_HEIGHT',
        'default': '0',
        'es3': True,
        'manual': True,
      },
      {
        'name': 'unpack_skip_pixels',
        'type': 'GLint',
        'enum': 'GL_UNPACK_SKIP_PIXELS',
        'default': '0',
        'es3': True,
        'manual': True,
      },
      {
        'name': 'unpack_skip_rows',
        'type': 'GLint',
        'enum': 'GL_UNPACK_SKIP_ROWS',
        'default': '0',
        'es3': True,
        'manual': True,
      },
      {
        'name': 'unpack_skip_images',
        'type': 'GLint',
        'enum': 'GL_UNPACK_SKIP_IMAGES',
        'default': '0',
        'es3': True,
        'manual': True,
      }
    ],
  },
  # TODO: Consider implemenenting these states
  # GL_ACTIVE_TEXTURE
  'LineWidth': {
    'type': 'Normal',
    'custom_function' : True,
    'func': 'DoLineWidth',
    'enum': 'GL_LINE_WIDTH',
    'states': [
      {
        'name': 'line_width',
        'type': 'GLfloat',
        'default': '1.0f',
        'range_checks': [{'check': "<= 0.0f", 'test_value': "0.0f"}],
        'nan_check': True,
      }],
  },
  'DepthMask': {
    'type': 'Normal',
    'func': 'DepthMask',
    'enum': 'GL_DEPTH_WRITEMASK',
    'states': [
      {
        'name': 'depth_mask',
        'type': 'GLboolean',
        'default': 'true',
        'cached': True
      },
    ],
    'on_change': 'framebuffer_state_.clear_state_dirty = true;',
  },
  'Scissor': {
    'type': 'Normal',
    'func': 'Scissor',
    'enum': 'GL_SCISSOR_BOX',
    'states': [
      # NOTE: These defaults reset at GLES2DecoderImpl::Initialization.
      {
        'name': 'scissor_x',
        'type': 'GLint',
        'default': '0',
      },
      {
        'name': 'scissor_y',
        'type': 'GLint',
        'default': '0',
      },
      {
        'name': 'scissor_width',
        'type': 'GLsizei',
        'default': '1',
        'expected': 'initial_size.width()',
      },
      {
        'name': 'scissor_height',
        'type': 'GLsizei',
        'default': '1',
        'expected': 'initial_size.height()',
      },
    ],
  },
  'Viewport': {
    'type': 'Normal',
    'func': 'Viewport',
    'enum': 'GL_VIEWPORT',
    'states': [
      # NOTE: These defaults reset at GLES2DecoderImpl::Initialization.
      {
        'name': 'viewport_x',
        'type': 'GLint',
        'default': '0',
      },
      {
        'name': 'viewport_y',
        'type': 'GLint',
        'default': '0',
      },
      {
        'name': 'viewport_width',
        'type': 'GLsizei',
        'default': '1',
        'expected': 'initial_size.width()',
      },
      {
        'name': 'viewport_height',
        'type': 'GLsizei',
        'default': '1',
        'expected': 'initial_size.height()',
      },
    ],
  },
  'WindowRectanglesEXT': {
    'type': 'Normal',
    'func': 'WindowRectanglesEXT',
    'custom_function': True,
    'extension_flag': 'ext_window_rectangles',
    'no_init': True,
    'states': [
      {
        'name': 'window_rectangles_mode',
        'type': 'GLenum',
        'enum': 'GL_WINDOW_RECTANGLE_MODE_EXT',
        'default': 'GL_EXCLUSIVE_EXT',
      },
      {
        'name': 'num_window_rectangles',
        'type': 'GLint',
        'enum': 'GL_NUM_WINDOW_RECTANGLES_EXT',
        'default': '0',
      },
    ],
  },
}

_prefix = None
_upper_prefix = None
_lower_prefix = None
def InitializePrefix(mixed_case_prefix):
  """Initialize prefix used for autogenerated code.

  Must be called before autogenerating code. Prefixes are used by autogenerated
  code in many places: class names, filenames, namespaces, constants,
  defines. Given a single mixed case prefix suitable for a class name, we also
  initialize lower and upper case prefixes for other uses (e.g. filenames and
  #defines).
  """
  global _prefix
  if _prefix:
    raise AssertionError
  _prefix = mixed_case_prefix

  global _upper_prefix
  _upper_prefix = mixed_case_prefix.upper()

  global _lower_prefix
  _lower_prefix = mixed_case_prefix.lower()


def _Namespace():
  if _lower_prefix != 'gles2':
    return 'gles2::'
  return ''


def Grouper(n, iterable, fillvalue=None):
  """Collect data into fixed-length chunks or blocks"""
  args = [iter(iterable)] * n
  return itertools.zip_longest(fillvalue=fillvalue, *args)


def SplitWords(input_string):
  """Split by '_' if found, otherwise split at uppercase/numeric chars.

  Will split "some_TEXT" into ["some", "TEXT"], "CamelCase" into ["Camel",
  "Case"], and "Vector3" into ["Vector", "3"].
  """
  if input_string.find('_') > -1:
    # 'some_TEXT_' -> 'some TEXT'
    return input_string.replace('_', ' ').strip().split()

  input_string = input_string.replace('::', ' ')
  if re.search('[A-Z]', input_string) and re.search('[a-z]', input_string):
    # mixed case.
    # look for capitalization to cut input_strings
    # 'SomeText' -> 'Some Text'
    input_string = re.sub('([A-Z])', r' \1', input_string).strip()
    # 'Vector3' -> 'Vector 3'
    input_string = re.sub('([^0-9])([0-9])', r'\1 \2', input_string)
  return input_string.split()

def ToUnderscore(input_string):
  """converts CamelCase to camel_case."""
  words = SplitWords(input_string)
  return '_'.join([word.lower() for word in words])

def ValidatorClassName(type_name):
  """Converts some::namespace::TypeName to SomeNamespaceTypeNameValidator."""
  words = SplitWords(type_name)
  prefix = ''.join([word.title() for word in words])
  return '%sValidator' % prefix

def CachedStateName(item):
  if item.get('cached', False):
    return 'cached_' + item['name']
  return item['name']

def GuardState(state, operation, feature_info):
  if 'manual' in state:
    assert state['manual']
    return ""

  result = []
  result_end = []
  if 'es3' in state:
    assert state['es3']
    result.append("  if (%s->IsES3Capable()) {\n" % feature_info);
    result_end.append("  }\n")
  if 'extension_flag' in state:
    result.append("  if (%s->feature_flags().%s) {\n  " %
                     (feature_info, state['extension_flag']))
    result_end.append("  }\n")
  if 'gl_version_flag' in state:
    name = state['gl_version_flag']
    inverted = ''
    if name[0] == '!':
      inverted = '!'
      name = name[1:]
    result.append("  if (%s%s->gl_version_info().%s) {\n" %
                      (inverted, feature_info, name))
    result_end.append("  }\n")

  result.append(operation)
  return ''.join(result + result_end)

def ToGLExtensionString(extension_flag):
  """Returns GL-type extension string of a extension flag."""
  if extension_flag == "oes_compressed_etc1_rgb8_texture":
    return "OES_compressed_ETC1_RGB8_texture" # Fixup inconsitency with rgb8,
                                              # unfortunate.
  uppercase_words = [ 'img', 'ext', 'arb', 'chromium', 'oes', 'amd', 'bgra8888',
                      'egl', 'atc', 'etc1', 'angle']
  parts = extension_flag.split('_')
  return "_".join(
    [part.upper() if part in uppercase_words else part for part in parts])

def ToCamelCase(input_string):
  """converts ABC_underscore_case to ABCUnderscoreCase."""
  return ''.join(w[0].upper() + w[1:] for w in input_string.split('_'))

def EnumsConflict(a, b):
  """Returns true if the enums have different names (ignoring suffixes) and one
  of them is a Chromium enum."""
  if a == b:
    return False

  if b.endswith('_CHROMIUM'):
    a, b = b, a

  if not a.endswith('_CHROMIUM'):
    return False

  def removesuffix(string, suffix):
    if not string.endswith(suffix):
      return string
    return string[:-len(suffix)]
  b = removesuffix(b, "_NV")
  b = removesuffix(b, "_EXT")
  b = removesuffix(b, "_OES")
  return removesuffix(a, "_CHROMIUM") != b

def GetGLGetTypeConversion(result_type, value_type, value):
  """Makes a gl compatible type conversion string for accessing state variables.

   Useful when accessing state variables through glGetXXX calls.
   glGet documetation (for example, the manual pages):
   [...] If glGetIntegerv is called, [...] most floating-point values are
   rounded to the nearest integer value. [...]

  Args:
   result_type: the gl type to be obtained
   value_type: the GL type of the state variable
   value: the name of the state variable

  Returns:
   String that converts the state variable to desired GL type according to GL
   rules.
  """

  if result_type == 'GLint':
    if value_type == 'GLfloat':
      return 'static_cast<GLint>(round(%s))' % value
  return 'static_cast<%s>(%s)' % (result_type, value)


class CWriter():
  """Context manager that creates a C source file.

  To be used with the `with` statement. Returns a normal `file` type, open only
  for writing - any existing files with that name will be overwritten. It will
  automatically write the contents of `_LICENSE`, `_DO_NOT_EDIT_WARNING` and
  `_ALLOW_UNSAFE_BUFFERS` at the beginning.

  Example:
    with CWriter("file.cpp") as myfile:
      myfile.write("hello")
      # type(myfile) == file
  """
  def __init__(self, filename, year):
    self.filename = filename
    self._ENTER_MSG = _LICENSE % year + _DO_NOT_EDIT_WARNING % _lower_prefix
    if (filename in _allow_unsafe_buffers_filenames):
        self._ENTER_MSG += _ALLOW_UNSAFE_BUFFERS
    self._EXIT_MSG = ""
    try:
      os.makedirs(os.path.dirname(filename))
    except OSError as e:
      if e.errno == errno.EEXIST:
        pass
    self._file = open(filename, 'w', newline='')

  def __enter__(self):
    self._file.write(self._ENTER_MSG)
    return self._file

  def __exit__(self, exc_type, exc_value, traceback):
    self._file.write(self._EXIT_MSG)
    self._file.close()


class CHeaderWriter(CWriter):
  """Context manager that creates a C header file.

  Works the same way as CWriter, except it will also add the #ifdef guard
  around it. If `file_comment` is set, it will write that before the #ifdef
  guard.
  """
  def __init__(self, filename, year, file_comment=None):
    super().__init__(filename, year)
    guard = self._get_guard()
    if file_comment is None:
      file_comment = ""
    self._ENTER_MSG = self._ENTER_MSG + file_comment \
                    + "#ifndef %s\n#define %s\n\n" % (guard, guard)
    self._EXIT_MSG = self._EXIT_MSG + "#endif  // %s\n" % guard

  def _get_guard(self):
    non_alnum_re = re.compile(r'[^a-zA-Z0-9]')
    assert self.filename.startswith("gpu/")
    return non_alnum_re.sub('_', self.filename).upper() + '_'


class TypeHandler():
  """This class emits code for a particular type of function."""

  _remove_expected_call_re = re.compile(r'  EXPECT_CALL.*?;\n', re.S)

  def InitFunction(self, func):
    """Add or adjust anything type specific for this function."""
    if func.GetInfo('needs_size') and not func.name.endswith('Bucket'):
      func.AddCmdArg(DataSizeArgument('data_size'))

  def NeedsDataTransferFunction(self, func):
    """Overriden from TypeHandler."""
    return func.num_pointer_args >= 1

  def WriteStruct(self, func, f):
    """Writes a structure that matches the arguments to a function."""
    comment = func.GetInfo('cmd_comment')
    if not comment == None:
      f.write(comment)
    f.write("struct %s {\n" % func.name)
    f.write("  typedef %s ValueType;\n" % func.name)
    f.write("  static const CommandId kCmdId = k%s;\n" % func.name)
    func.WriteCmdArgFlag(f)
    func.WriteCmdFlag(f)
    f.write("\n")
    result = func.GetInfo('result')
    if not result == None:
      if len(result) == 1:
        f.write("  typedef %s Result;\n\n" % result[0])
      else:
        f.write("  struct Result {\n")
        for line in result:
          f.write("    %s;\n" % line)
        f.write("  };\n\n")

    func.WriteCmdComputeSize(f)
    func.WriteCmdSetHeader(f)
    func.WriteCmdInit(f)
    func.WriteCmdSet(f)
    func.WriteArgAccessors(f)

    f.write("  gpu::CommandHeader header;\n")
    total_args = 0
    args = func.GetCmdArgs()
    for arg in args:
      for cmd_type, name in arg.GetArgDecls():
        f.write("  %s %s;\n" % (cmd_type, name))
        total_args += 1
    trace_queue = func.GetInfo('trace_queueing_flow', False)
    if trace_queue:
      f.write("  uint32_t trace_id;\n")
      total_args += 1

    consts = func.GetCmdConstants()
    for const in consts:
      const_decls = const.GetArgDecls()
      assert(len(const_decls) == 1)
      const_cmd_type, const_name = const_decls[0]
      f.write("  static const %s %s = %s;\n" %
                 (const_cmd_type, const_name, const.GetConstantValue()))

    f.write("};\n")
    f.write("\n")

    size = total_args * _SIZE_OF_UINT32 + _SIZE_OF_COMMAND_HEADER
    f.write("static_assert(sizeof(%s) == %d,\n" % (func.name, size))
    f.write("              \"size of %s should be %d\");\n" %
            (func.name, size))
    f.write("static_assert(offsetof(%s, header) == 0,\n" % func.name)
    f.write("              \"offset of %s header should be 0\");\n" %
            func.name)
    offset = _SIZE_OF_COMMAND_HEADER
    for arg in args:
      for _, name in arg.GetArgDecls():
        f.write("static_assert(offsetof(%s, %s) == %d,\n" %
                (func.name, name, offset))
        f.write("              \"offset of %s %s should be %d\");\n" %
                (func.name, name, offset))
        offset += _SIZE_OF_UINT32
    if not result == None and len(result) > 1:
      offset = 0;
      for line in result:
        parts = line.split()
        name = parts[-1]
        check = """
static_assert(offsetof(%(cmd_name)s::Result, %(field_name)s) == %(offset)d,
              "offset of %(cmd_name)s Result %(field_name)s should be "
              "%(offset)d");
"""
        f.write((check.strip() + "\n") % {
              'cmd_name': func.name,
              'field_name': name,
              'offset': offset,
            })
        offset += _SIZE_OF_UINT32
    f.write("\n")

  def WriteHandlerImplementation(self, func, f):
    """Writes the handler implementation for this command."""
    args = []
    for arg in func.GetOriginalArgs():
      if arg.name.endswith("size") and arg.type == "GLsizei":
        args.append("num_%s" % func.GetLastOriginalArg().name)
      elif arg.name == "length":
        args.append("nullptr")
      else:
        args.append(arg.name)

    if func.GetInfo('type') == 'GETn' and func.name != 'GetSynciv':
      args.append('num_values')

    f.write("  %s(%s);\n" %
               (func.GetGLFunctionName(), ", ".join(args)))

  def WriteCmdSizeTest(self, _func, f):
    """Writes the size test for a command."""
    f.write("  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);\n")

  def WriteFormatTest(self, func, f):
    """Writes a format test for a command."""
    f.write("TEST_F(%sFormatTest, %s) {\n" % (_prefix, func.name))
    f.write("  cmds::%s& cmd = *GetBufferAs<cmds::%s>();\n" %
               (func.name, func.name))
    f.write("  void* next_cmd = cmd.Set(\n")
    f.write("      &cmd")
    args = func.GetCmdArgs()
    for value, arg in enumerate(args):
      f.write(",\n      static_cast<%s>(%d)" % (arg.type, value + 11))
    f.write(");\n")
    f.write("  EXPECT_EQ(static_cast<uint32_t>(cmds::%s::kCmdId),\n" %
               func.name)
    f.write("            cmd.header.command);\n")
    func.type_handler.WriteCmdSizeTest(func, f)
    for value, arg in enumerate(args):
      f.write("  EXPECT_EQ(static_cast<%s>(%d), %s);\n" %
                 (arg.type, value + 11, arg.GetArgAccessor('cmd')))
    f.write("  CheckBytesWrittenMatchesExpectedSize(\n")
    f.write("      next_cmd, sizeof(cmd));\n")
    f.write("}\n")
    f.write("\n")

  def WriteImmediateFormatTest(self, func, f):
    """Writes a format test for an immediate version of a command."""

  def WriteGetDataSizeCode(self, func, arg, f):
    """Writes the code to set data_size used in validation"""

  def WriteImmediateHandlerImplementation (self, func, f):
    """Writes the handler impl for the immediate version of a command."""
    f.write("  %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))

  def WriteBucketHandlerImplementation (self, func, f):
    """Writes the handler impl for the bucket version of a command."""
    f.write("  %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))

  def WriteServiceHandlerFunctionHeader(self, func, f):
    """Writes function header for service implementation handlers."""
    f.write("""error::Error %(prefix)sDecoderImpl::Handle%(name)s(
        uint32_t immediate_data_size, const volatile void* cmd_data) {
      """ % {'name': func.name, 'prefix' : _prefix})
    if func.IsES3():
      f.write("""if (!feature_info_->IsWebGL2OrES3OrHigherContext())
          return error::kUnknownCommand;
        """)
    if func.IsES31():
      f.write("""return error::kUnknownCommand;
        }

        """)
      return
    if func.GetCmdArgs():
      f.write("""const volatile %(prefix)s::cmds::%(name)s& c =
            *static_cast<const volatile %(prefix)s::cmds::%(name)s*>(cmd_data);
        """ % {'name': func.name, 'prefix': _lower_prefix})

  def WriteServiceHandlerArgGetCode(self, func, f):
    """Writes the argument unpack code for service handlers."""
    if len(func.GetOriginalArgs()) > 0:
      for arg in func.GetOriginalArgs():
        if not arg.IsPointer():
          arg.WriteGetCode(f)

      # Write pointer arguments second. Sizes may be dependent on other args
      for arg in func.GetOriginalArgs():
        if arg.IsPointer():
          self.WriteGetDataSizeCode(func, arg, f)
          arg.WriteGetCode(f)

  def WriteImmediateServiceHandlerArgGetCode(self, func, f):
    """Writes the argument unpack code for immediate service handlers."""
    for arg in func.GetOriginalArgs():
      if arg.IsPointer():
        self.WriteGetDataSizeCode(func, arg, f)
      arg.WriteGetCode(f)

  def WriteBucketServiceHandlerArgGetCode(self, func, f):
    """Writes the argument unpack code for bucket service handlers."""
    for arg in func.GetCmdArgs():
      arg.WriteGetCode(f)
    for arg in func.GetOriginalArgs():
      if arg.IsConstant():
        arg.WriteGetCode(f)
    self.WriteGetDataSizeCode(func, arg, f)

  def WriteServiceImplementation(self, func, f):
    """Writes the service implementation for a command."""
    self.WriteServiceHandlerFunctionHeader(func, f)
    if func.IsES31():
      return
    self.WriteHandlerExtensionCheck(func, f)
    self.WriteServiceHandlerArgGetCode(func, f)
    func.WriteHandlerValidation(f)
    func.WriteQueueTraceEvent(f)
    func.WriteHandlerImplementation(f)
    f.write("  return error::kNoError;\n")
    f.write("}\n")
    f.write("\n")

  def WriteImmediateServiceImplementation(self, func, f):
    """Writes the service implementation for an immediate version of command."""
    self.WriteServiceHandlerFunctionHeader(func, f)
    if func.IsES31():
      return
    self.WriteHandlerExtensionCheck(func, f)
    self.WriteImmediateServiceHandlerArgGetCode(func, f)
    func.WriteHandlerValidation(f)
    func.WriteQueueTraceEvent(f)
    func.WriteHandlerImplementation(f)
    f.write("  return error::kNoError;\n")
    f.write("}\n")
    f.write("\n")

  def WriteBucketServiceImplementation(self, func, f):
    """Writes the service implementation for a bucket version of command."""
    self.WriteServiceHandlerFunctionHeader(func, f)
    if func.IsES31():
      return
    self.WriteHandlerExtensionCheck(func, f)
    self.WriteBucketServiceHandlerArgGetCode(func, f)
    func.WriteHandlerValidation(f)
    func.WriteQueueTraceEvent(f)
    func.WriteHandlerImplementation(f)
    f.write("  return error::kNoError;\n")
    f.write("}\n")
    f.write("\n")

  def WritePassthroughServiceFunctionHeader(self, func, f):
    """Writes function header for service passthrough handlers."""
    f.write("""error::Error GLES2DecoderPassthroughImpl::Handle%(name)s(
        uint32_t immediate_data_size, const volatile void* cmd_data) {
      """ % {'name': func.name})
    if func.IsES3():
      f.write("""if (!feature_info_->IsWebGL2OrES3OrHigherContext())
          return error::kUnknownCommand;
        """)
    if func.IsES31():
      f.write("""if (!feature_info_->IsES31ForTestingContext())
          return error::kUnknownCommand;
        """)
    if func.GetCmdArgs():
      f.write("""const volatile gles2::cmds::%(name)s& c =
            *static_cast<const volatile gles2::cmds::%(name)s*>(cmd_data);
        """ % {'name': func.name})

  def WritePassthroughServiceFunctionDoerCall(self, func, f):
    """Writes the function call to the passthrough service doer."""
    f.write("""  error::Error error = Do%(name)s(%(args)s);
  if (error != error::kNoError) {
    return error;
  }""" % {'name': func.original_name,
          'args': func.MakePassthroughServiceDoerArgString("")})

  def WritePassthroughServiceImplementation(self, func, f):
    """Writes the service implementation for a command."""
    self.WritePassthroughServiceFunctionHeader(func, f)
    self.WriteHandlerExtensionCheck(func, f)
    self.WriteServiceHandlerArgGetCode(func, f)
    func.WritePassthroughHandlerValidation(f)
    self.WritePassthroughServiceFunctionDoerCall(func, f)
    f.write("  return error::kNoError;\n")
    f.write("}\n")
    f.write("\n")

  def WritePassthroughImmediateServiceImplementation(self, func, f):
    """Writes the service implementation for a command."""
    self.WritePassthroughServiceFunctionHeader(func, f)
    self.WriteHandlerExtensionCheck(func, f)
    self.WriteImmediateServiceHandlerArgGetCode(func, f)
    func.WritePassthroughHandlerValidation(f)
    self.WritePassthroughServiceFunctionDoerCall(func, f)
    f.write("  return error::kNoError;\n")
    f.write("}\n")
    f.write("\n")

  def WritePassthroughBucketServiceImplementation(self, func, f):
    """Writes the service implementation for a command."""
    self.WritePassthroughServiceFunctionHeader(func, f)
    self.WriteHandlerExtensionCheck(func, f)
    self.WriteBucketServiceHandlerArgGetCode(func, f)
    func.WritePassthroughHandlerValidation(f)
    self.WritePassthroughServiceFunctionDoerCall(func, f)
    f.write("  return error::kNoError;\n")
    f.write("}\n")
    f.write("\n")

  def WriteHandlerExtensionCheck(self, func, f):
    if func.GetInfo('extension_flag'):
      f.write("  if (!features().%s) {\n" % func.GetInfo('extension_flag'))
      f.write("    return error::kUnknownCommand;")
      f.write("  }\n\n")

  def WriteValidUnitTest(self, func, f, test, *extras):
    """Writes a valid unit test for the service implementation."""
    if not func.GetInfo('expectation', True):
      test = self._remove_expected_call_re.sub('', test)
    name = func.name
    arg_strings = [
      arg.GetValidArg(func) \
      for arg in func.GetOriginalArgs() if not arg.IsConstant()
    ]
    gl_arg_strings = [
      arg.GetValidGLArg(func) \
      for arg in func.GetOriginalArgs()
    ]
    gl_func_name = func.GetGLTestFunctionName()
    varz = {
      'name': name,
      'gl_func_name': gl_func_name,
      'args': ", ".join(arg_strings),
      'gl_args': ", ".join(gl_arg_strings),
    }
    for extra in extras:
      varz.update(extra)
    old_test = ""
    while (old_test != test):
      old_test = test
      test = test % varz
    f.write(test % varz)

  def WriteInvalidUnitTest(self, func, f, test, *extras):
    """Writes an invalid unit test for the service implementation."""
    if func.IsES3():
      return
    for invalid_arg_index, invalid_arg in enumerate(func.GetOriginalArgs()):
      # Service implementation does not test constants, as they are not part of
      # the call in the service side.
      if invalid_arg.IsConstant():
        continue

      num_invalid_values = invalid_arg.GetNumInvalidValues(func)
      for value_index in range(0, num_invalid_values):
        arg_strings = []
        parse_result = "kNoError"
        gl_error = None
        for arg in func.GetOriginalArgs():
          if arg.IsConstant():
            continue
          if invalid_arg is arg:
            (arg_string, parse_result, gl_error) = arg.GetInvalidArg(
                value_index)
          else:
            arg_string = arg.GetValidArg(func)
          arg_strings.append(arg_string)
        gl_arg_strings = []
        for arg in func.GetOriginalArgs():
          gl_arg_strings.append("_")
        gl_func_name = func.GetGLTestFunctionName()
        gl_error_test = ''
        if not gl_error == None:
          gl_error_test = '\n  EXPECT_EQ(%s, GetGLError());' % gl_error

        varz = {
          'name': func.name,
          'arg_index': invalid_arg_index,
          'value_index': value_index,
          'gl_func_name': gl_func_name,
          'args': ", ".join(arg_strings),
          'all_but_last_args': ", ".join(arg_strings[:-1]),
          'gl_args': ", ".join(gl_arg_strings),
          'parse_result': parse_result,
          'gl_error_test': gl_error_test,
        }
        for extra in extras:
          varz.update(extra)
        f.write(test % varz)

  def WriteServiceUnitTest(self, func, f, *extras):
    """Writes the service unit test for a command."""

    if func.name == 'Enable':
      valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  SetupExpectationsForEnableDisable(%(gl_args)s, true);
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);"""
    elif func.name == 'Disable':
      valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  SetupExpectationsForEnableDisable(%(gl_args)s, false);
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);"""
    else:
      valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);"""
    valid_test += """
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    self.WriteValidUnitTest(func, f, valid_test, *extras)

    if not func.IsES3():
      invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
      self.WriteInvalidUnitTest(func, f, invalid_test, *extras)

  def WriteImmediateServiceUnitTest(self, func, f, *extras):
    """Writes the service unit test for an immediate command."""

  def WriteImmediateValidationCode(self, func, f):
    """Writes the validation code for an immediate version of a command."""

  def WriteBucketServiceUnitTest(self, func, f, *extras):
    """Writes the service unit test for a bucket command."""

  def WriteGLES2ImplementationDeclaration(self, func, f):
    """Writes the GLES2 Implemention declaration."""
    f.write("%s %s(%s) override;\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("", add_default = True)))
    f.write("\n")

  def WriteGLES2CLibImplementation(self, func, f):
    f.write("%s GL_APIENTRY GLES2%s(%s) {\n" %
               (func.return_type, func.name,
                func.MakeTypedOriginalArgString("")))
    result_string = "return "
    if func.return_type == "void":
      result_string = ""
    f.write("  %sgles2::GetGLContext()->%s(%s);\n" %
               (result_string, func.original_name,
                func.MakeOriginalArgString("")))
    f.write("}\n")

  def WriteGLES2Header(self, func, f):
    """Writes a re-write macro for GLES"""
    f.write("#define gl%s GLES2_GET_FUN(%s)\n" %(func.name, func.name))

  def WriteClientGLCallLog(self, func, f):
    """Writes a logging macro for the client side code."""
    comma = ""
    if len(func.GetOriginalArgs()):
      comma = " << "
    f.write(
        '  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] %s("%s%s << ")");\n' %
        (func.prefixed_name, comma, func.MakeLogArgString()))

  def WriteClientGLReturnLog(self, func, f):
    """Writes the return value logging code."""
    if func.return_type != "void":
      f.write('  GPU_CLIENT_LOG("return:" << result)\n')

  def WriteGLES2ImplementationHeader(self, func, f):
    """Writes the GLES2 Implemention."""
    self.WriteGLES2ImplementationDeclaration(func, f)

  def WriteGLES2TraceImplementationHeader(self, func, f):
    """Writes the GLES2 Trace Implemention header."""
    f.write("%s %s(%s) override;\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))

  def WriteGLES2TraceImplementation(self, func, f):
    """Writes the GLES2 Trace Implemention."""
    f.write("%s GLES2TraceImplementation::%s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    result_string = "return "
    if func.return_type == "void":
      result_string = ""
    f.write('  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::%s");\n' %
               func.name)
    f.write("  %sgl_->%s(%s);\n" %
               (result_string, func.name, func.MakeOriginalArgString("")))
    f.write("}\n")
    f.write("\n")

  def WriteGLES2Implementation(self, func, f):
    """Writes the GLES2 Implemention."""
    impl_func = func.GetInfo('impl_func', True)
    if func.can_auto_generate and impl_func:
      f.write("%s %sImplementation::%s(%s) {\n" %
                 (func.return_type, _prefix,  func.original_name,
                  func.MakeTypedOriginalArgString("")))
      f.write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
      self.WriteClientGLCallLog(func, f)
      func.WriteDestinationInitalizationValidation(f)
      for arg in func.GetOriginalArgs():
        arg.WriteClientSideValidationCode(f, func)
      f.write("  helper_->%s(%s);\n" %
                 (func.name, func.MakeHelperArgString("")))
      if _prefix != 'WebGPU':
        f.write("  CheckGLError();\n")
      self.WriteClientGLReturnLog(func, f)
      f.write("}\n")
      f.write("\n")

  def WriteGLES2InterfaceHeader(self, func, f):
    """Writes the GLES2 Interface."""
    f.write("virtual %s %s(%s) = 0;\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("", add_default = True)))

  def WriteGLES2InterfaceStub(self, func, f):
    """Writes the GLES2 Interface stub declaration."""
    f.write("%s %s(%s) override;\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))

  def WriteGLES2InterfaceStubImpl(self, func, f):
    """Writes the GLES2 Interface stub declaration."""
    args = func.GetOriginalArgs()
    arg_string = ", ".join(
        ["%s /* %s */" % (arg.type, arg.name) for arg in args])
    f.write("%s %sInterfaceStub::%s(%s) {\n" %
               (func.return_type, _prefix, func.original_name, arg_string))
    if func.return_type != "void":
      f.write("  return 0;\n")
    f.write("}\n")

  def WriteGLES2ImplementationUnitTest(self, func, f):
    """Writes the GLES2 Implemention unit test."""
    client_test = func.GetInfo('client_test', True)
    if func.can_auto_generate and client_test:
      code = """
TEST_F(%(prefix)sImplementationTest, %(name)s) {
  struct Cmds {
    cmds::%(name)s cmd;
  };
  Cmds expected;
  expected.cmd.Init(%(cmd_args)s);

  gl_->%(name)s(%(args)s);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
"""
      cmd_arg_strings = [
        arg.GetValidClientSideCmdArg(func) for arg in func.GetCmdArgs()
      ]

      gl_arg_strings = [
        arg.GetValidClientSideArg(func) for arg in func.GetOriginalArgs()
      ]

      f.write(code % {
            'prefix' : _prefix,
            'name': func.name,
            'args': ", ".join(gl_arg_strings),
            'cmd_args': ", ".join(cmd_arg_strings),
          })

      # Test constants for invalid values, as they are not tested by the
      # service.
      constants = [arg for arg in func.GetOriginalArgs() if arg.IsConstant()]
      if constants:
        code = """
TEST_F(%(prefix)sImplementationTest,
    %(name)sInvalidConstantArg%(invalid_index)d) {
  gl_->%(name)s(%(args)s);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(%(gl_error)s, CheckError());
}
"""
        for invalid_arg in constants:
          gl_arg_strings = []
          invalid = invalid_arg.GetInvalidArg(0)
          for arg in func.GetOriginalArgs():
            if arg is invalid_arg:
              gl_arg_strings.append(invalid[0])
            else:
              gl_arg_strings.append(arg.GetValidClientSideArg(func))

          f.write(code % {
            'prefix' : _prefix,
            'name': func.name,
            'invalid_index': func.GetOriginalArgs().index(invalid_arg),
            'args': ", ".join(gl_arg_strings),
            'gl_error': invalid[2],
          })

  def WriteDestinationInitalizationValidation(self, func, f):
    """Writes the client side destintion initialization validation."""
    for arg in func.GetOriginalArgs():
      arg.WriteDestinationInitalizationValidation(f, func)

  def WriteTraceEvent(self, func, f):
    f.write('  TRACE_EVENT0("gpu", "%sImplementation::%s");\n' %
               (_prefix, func.original_name))

  def WriteImmediateCmdComputeSize(self, _func, f):
    """Writes the size computation code for the immediate version of a cmd."""
    f.write("  static uint32_t ComputeSize(uint32_t size_in_bytes) {\n")
    f.write("    return static_cast<uint32_t>(\n")
    f.write("        sizeof(ValueType) +  // NOLINT\n")
    f.write("        RoundSizeToMultipleOfEntries(size_in_bytes));\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdSetHeader(self, _func, f):
    """Writes the SetHeader function for the immediate version of a cmd."""
    f.write("  void SetHeader(uint32_t size_in_bytes) {\n")
    f.write("    header.SetCmdByTotalSize<ValueType>(size_in_bytes);\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdInit(self, func, f):
    """Writes the Init function for the immediate version of a command."""
    raise NotImplementedError(func.name)

  def WriteImmediateCmdSet(self, func, f):
    """Writes the Set function for the immediate version of a command."""
    raise NotImplementedError(func.name)

  def WriteCmdHelper(self, func, f):
    """Writes the cmd helper definition for a cmd."""
    code = """  void %(name)s(%(typed_args)s) {
    %(lp)s::cmds::%(name)s* c = GetCmdSpace<%(lp)s::cmds::%(name)s>();
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    f.write(code % {
          "lp" : _lower_prefix,
          "name": func.name,
          "typed_args": func.MakeTypedCmdArgString(""),
          "args": func.MakeCmdArgString(""),
        })

  def WriteImmediateCmdHelper(self, func, f):
    """Writes the cmd helper definition for the immediate version of a cmd."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32_t s = 0;
    %(lp)s::cmds::%(name)s* c =
        GetImmediateCmdSpaceTotalSize<%(lp)s::cmds::%(name)s>(s);
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    f.write(code % {
           "lp" : _lower_prefix,
           "name": func.name,
           "typed_args": func.MakeTypedCmdArgString(""),
           "args": func.MakeCmdArgString(""),
        })


class StateSetHandler(TypeHandler):
  """Handler for commands that simply set state."""

  def WriteHandlerImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    state_name = func.GetInfo('state')
    state = _STATE_INFO[state_name]
    states = state['states']
    args = func.GetOriginalArgs()
    for ndx,item in enumerate(states):
      code = []
      if 'range_checks' in item:
        for range_check in item['range_checks']:
          code.append("%s %s" % (args[ndx].name, range_check['check']))
      if 'nan_check' in item:
        # Drivers might generate an INVALID_VALUE error when a value is set
        # to NaN. This is allowed behavior under GLES 3.0 section 2.1.1 or
        # OpenGL 4.5 section 2.3.4.1 - providing NaN allows undefined results.
        # Make this behavior consistent within Chromium, and avoid leaking GL
        # errors by generating the error in the command buffer instead of
        # letting the GL driver generate it.
        code.append("std::isnan(%s)" % args[ndx].name)
      if code:
        f.write("  if (%s) {\n" % " ||\n      ".join(code))
        f.write(
          '    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,'
          ' "%s", "%s out of range");\n' %
          (func.name, args[ndx].name))
        f.write("    return error::kNoError;\n")
        f.write("  }\n")
    code = []
    for ndx,item in enumerate(states):
      code.append("state_.%s != %s" % (item['name'], args[ndx].name))
    f.write("  if (%s) {\n" % " ||\n      ".join(code))
    for ndx,item in enumerate(states):
      f.write("    state_.%s = %s;\n" % (item['name'], args[ndx].name))
    if 'on_change' in state:
      f.write("    %s\n" % state['on_change'])
    if not func.GetInfo("no_gl"):
      for ndx,item in enumerate(states):
        if item.get('cached', False):
          f.write("    state_.%s = %s;\n" %
                     (CachedStateName(item), args[ndx].name))
      f.write("    %s(%s);\n" %
                 (func.GetGLFunctionName(), func.MakeOriginalArgString("")))
    f.write("  }\n")

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""
    TypeHandler.WriteServiceUnitTest(self, func, f, *extras)
    state_name = func.GetInfo('state')
    state = _STATE_INFO[state_name]
    states = state['states']
    for ndx,item in enumerate(states):
      if 'range_checks' in item:
        for check_ndx, range_check in enumerate(item['range_checks']):
          valid_test = """
TEST_P(%(test_name)s, %(name)sInvalidValue%(ndx)d_%(check_ndx)d) {
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}
"""
          name = func.name
          arg_strings = [
            arg.GetValidArg(func) \
            for arg in func.GetOriginalArgs() if not arg.IsConstant()
          ]

          arg_strings[ndx] = range_check['test_value']
          varz = {
            'name': name,
            'ndx': ndx,
            'check_ndx': check_ndx,
            'args': ", ".join(arg_strings),
          }
          for extra in extras:
            varz.update(extra)
          f.write(valid_test % varz)
      if 'nan_check' in item:
        valid_test = """
TEST_P(%(test_name)s, %(name)sNaNValue%(ndx)d) {
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}
"""
        name = func.name
        arg_strings = [
          arg.GetValidArg(func) \
          for arg in func.GetOriginalArgs() if not arg.IsConstant()
        ]

        arg_strings[ndx] = 'nanf("")'
        varz = {
          'name': name,
          'ndx': ndx,
          'args': ", ".join(arg_strings),
        }
        for extra in extras:
          varz.update(extra)
        f.write(valid_test % varz)

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class StateSetRGBAlphaHandler(TypeHandler):
  """Handler for commands that simply set state that have rgb/alpha."""

  def WriteHandlerImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    state_name = func.GetInfo('state')
    state = _STATE_INFO[state_name]
    states = state['states']
    args = func.GetOriginalArgs()
    num_args = len(args)
    code = []
    for ndx,item in enumerate(states):
      code.append("state_.%s != %s" % (item['name'], args[ndx % num_args].name))
    f.write("  if (%s) {\n" % " ||\n      ".join(code))
    for ndx, item in enumerate(states):
      f.write("    state_.%s = %s;\n" %
                 (item['name'], args[ndx % num_args].name))
    if 'on_change' in state:
      f.write("    %s\n" % state['on_change'])
    if not func.GetInfo("no_gl"):
      f.write("    %s(%s);\n" %
                 (func.GetGLFunctionName(), func.MakeOriginalArgString("")))
      f.write("  }\n")

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class StateSetFrontBackSeparateHandler(TypeHandler):
  """Handler for commands that simply set state that have front/back."""

  def WriteHandlerImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    state_name = func.GetInfo('state')
    state = _STATE_INFO[state_name]
    states = state['states']
    args = func.GetOriginalArgs()
    face = args[0].name
    num_args = len(args)
    f.write("  bool changed = false;\n")
    for group_ndx, group in enumerate(Grouper(num_args - 1, states)):
      f.write("  if (%s == %s || %s == GL_FRONT_AND_BACK) {\n" %
                 (face, ('GL_FRONT', 'GL_BACK')[group_ndx], face))
      code = []
      for ndx, item in enumerate(group):
        code.append("state_.%s != %s" % (item['name'], args[ndx + 1].name))
      f.write("    changed |= %s;\n" % " ||\n        ".join(code))
      f.write("  }\n")
    f.write("  if (changed) {\n")
    for group_ndx, group in enumerate(Grouper(num_args - 1, states)):
      f.write("    if (%s == %s || %s == GL_FRONT_AND_BACK) {\n" %
                 (face, ('GL_FRONT', 'GL_BACK')[group_ndx], face))
      for ndx, item in enumerate(group):
        f.write("      state_.%s = %s;\n" %
                   (item['name'], args[ndx + 1].name))
      f.write("    }\n")
    if 'on_change' in state:
      f.write("    %s\n" % state['on_change'])
    if not func.GetInfo("no_gl"):
      f.write("    %s(%s);\n" %
                 (func.GetGLFunctionName(), func.MakeOriginalArgString("")))
    f.write("  }\n")

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class StateSetFrontBackHandler(TypeHandler):
  """Handler for commands that simply set state that set both front/back."""

  def WriteHandlerImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    state_name = func.GetInfo('state')
    state = _STATE_INFO[state_name]
    states = state['states']
    args = func.GetOriginalArgs()
    num_args = len(args)
    code = []
    for group in Grouper(num_args, states):
      for ndx, item in enumerate(group):
        code.append("state_.%s != %s" % (item['name'], args[ndx].name))
    f.write("  if (%s) {\n" % " ||\n      ".join(code))
    for group in Grouper(num_args, states):
      for ndx, item in enumerate(group):
        f.write("    state_.%s = %s;\n" % (item['name'], args[ndx].name))
    if 'on_change' in state:
      f.write("    %s\n" % state['on_change'])
    if not func.GetInfo("no_gl"):
      f.write("    %s(%s);\n" %
                 (func.GetGLFunctionName(), func.MakeOriginalArgString("")))
    f.write("  }\n")

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class StateSetNamedParameter(TypeHandler):
  """Handler for commands that set a state chosen with an enum parameter."""

  def WriteHandlerImplementation(self, func, f):
    """Overridden from TypeHandler."""
    state_name = func.GetInfo('state')
    state = _STATE_INFO[state_name]
    states = state['states']
    args = func.GetOriginalArgs()
    num_args = len(args)
    assert num_args == 2
    f.write("  switch (%s) {\n" % args[0].name)
    for state in states:
      f.write("    case %s:\n" % state['enum'])
      f.write("      if (state_.%s != %s) {\n" %
                 (state['name'], args[1].name))
      f.write("        state_.%s = %s;\n" % (state['name'], args[1].name))
      if not func.GetInfo("no_gl"):
        operation = "        %s(%s);\n" % \
                    (func.GetGLFunctionName(), func.MakeOriginalArgString(""))
        f.write(GuardState(state, operation, "feature_info_"))
      f.write("      }\n")
      f.write("      break;\n")
    f.write("    default:\n")
    f.write("      NOTREACHED_IN_MIGRATION();\n")
    f.write("  }\n")

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class CustomHandler(TypeHandler):
  """Handler for commands that are auto-generated but require minor tweaks."""

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    if (func.name.startswith('CompressedTex') and func.name.endswith('Bucket')):
      # Remove imageSize argument, take the size from the bucket instead.
      func.cmd_args = [arg for arg in func.cmd_args if arg.name != 'imageSize']
      func.AddCmdArg(Argument('bucket_id', 'GLuint'))
    else:
      TypeHandler.InitFunction(self, func)

  def WriteServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    if func.IsES31():
      TypeHandler.WriteServiceImplementation(self, func, f)

  def WriteImmediateServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    if func.IsES31():
      TypeHandler.WriteImmediateServiceImplementation(self, func, f)

  def WriteBucketServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    if func.IsES31():
      TypeHandler.WriteBucketServiceImplementation(self, func, f)

  def WritePassthroughServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""

  def WritePassthroughImmediateServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""

  def WritePassthroughBucketServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""

  def WriteImmediateServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdGetTotalSize(self, _func, f):
    """Overrriden from TypeHandler."""
    f.write(
        "    uint32_t total_size = 0;  // WARNING: compute correct size.\n")

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("  void Init(%s) {\n" % func.MakeTypedCmdArgString("_"))
    self.WriteImmediateCmdGetTotalSize(func, f)
    f.write("    SetHeader(total_size);\n")
    args = func.GetCmdArgs()
    for arg in args:
      arg.WriteSetCode(f, 4, '_%s' % arg.name)
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""
    copy_args = func.MakeCmdArgString("_", False)
    f.write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedCmdArgString("_", True))
    self.WriteImmediateCmdGetTotalSize(func, f)
    f.write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    f.write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, total_size);\n")
    f.write("  }\n")
    f.write("\n")


class NoCommandHandler(CustomHandler):
  """Handler for functions that don't use commands"""

  def WriteGLES2Implementation(self, func, f):
    pass

  def WriteGLES2ImplementationUnitTest(self, func, f):
    pass


class DataHandler(TypeHandler):
  """
  Handler for glBufferData, glBufferSubData, glTex{Sub}Image*D.
  """

  def WriteGetDataSizeCode(self, func, arg, f):
    """Overrriden from TypeHandler."""
    # TODO: Move this data to _FUNCTION_INFO?
    name = func.name
    if name.endswith("Immediate"):
      name = name[0:-9]
    if arg.name in func.size_args:
      size = func.size_args[arg.name]
      f.write("  uint32_t %s = %s;\n" % (arg.GetReservedSizeId(), size))
    else:
      f.write("// uint32_t %s = 0;  // WARNING: compute correct size.\n" % (
              arg.GetReservedSizeId()))

  def WriteImmediateCmdGetTotalSize(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("  void Init(%s) {\n" % func.MakeTypedCmdArgString("_"))
    self.WriteImmediateCmdGetTotalSize(func, f)
    f.write("    SetHeader(total_size);\n")
    args = func.GetCmdArgs()
    for arg in args:
      f.write("    %s = _%s;\n" % (arg.name, arg.name))
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""
    copy_args = func.MakeCmdArgString("_", False)
    f.write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedCmdArgString("_", True))
    self.WriteImmediateCmdGetTotalSize(func, f)
    f.write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    f.write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, total_size);\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateFormatTest(self, func, f):
    """Overrriden from TypeHandler."""
    # TODO: Remove this exception.
    return

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""

  def WriteImmediateServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class BindHandler(TypeHandler):
  """Handler for glBind___ type functions."""

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""

    if len(func.GetOriginalArgs()) == 1:
      valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
      if func.GetInfo("gen_func"):
          valid_test += """
TEST_P(%(test_name)s, %(name)sValidArgsNewId) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(kNewServiceId));
  EXPECT_CALL(*gl_, %(gl_gen_func_name)s(1, _))
     .WillOnce(SetArgPointee<1>(kNewServiceId));
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(Get%(resource_type)s(kNewClientId) != nullptr);
}
"""
      self.WriteValidUnitTest(func, f, valid_test, {
          'resource_type': func.GetOriginalArgs()[0].resource_type,
          'gl_gen_func_name': func.GetInfo("gen_func"),
      }, *extras)
    else:
      valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
      if func.GetInfo("gen_func"):
        valid_test += """
TEST_P(%(test_name)s, %(name)sValidArgsNewId) {
  EXPECT_CALL(*gl_,
              %(gl_func_name)s(%(gl_args_with_new_id)s));
  EXPECT_CALL(*gl_, %(gl_gen_func_name)s(1, _))
     .WillOnce(SetArgPointee<1>(kNewServiceId));
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args_with_new_id)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(Get%(resource_type)s(kNewClientId) != nullptr);
}
"""

      gl_args_with_new_id = []
      args_with_new_id = []
      for arg in func.GetOriginalArgs():
        if hasattr(arg, 'resource_type'):
          gl_args_with_new_id.append('kNewServiceId')
          args_with_new_id.append('kNewClientId')
        else:
          gl_args_with_new_id.append(arg.GetValidGLArg(func))
          args_with_new_id.append(arg.GetValidArg(func))
      self.WriteValidUnitTest(func, f, valid_test, {
          'args_with_new_id': ", ".join(args_with_new_id),
          'gl_args_with_new_id': ", ".join(gl_args_with_new_id),
          'resource_type': func.GetResourceIdArg().resource_type,
          'gl_gen_func_name': func.GetInfo("gen_func"),
      }, *extras)

    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, f, invalid_test, *extras)

  def WriteGLES2Implementation(self, func, f):
    """Writes the GLES2 Implemention."""

    impl_func = func.GetInfo('impl_func', True)
    if func.can_auto_generate and impl_func:
      f.write("%s %sImplementation::%s(%s) {\n" %
                 (func.return_type, _prefix, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      f.write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
      func.WriteDestinationInitalizationValidation(f)
      self.WriteClientGLCallLog(func, f)
      for arg in func.GetOriginalArgs():
        arg.WriteClientSideValidationCode(f, func)

      code = """  if (Is%(type)sReservedId(%(id)s)) {
    SetGLError(GL_INVALID_OPERATION, "%(name)s\", \"%(id)s reserved id");
    return;
  }
  %(name)sHelper(%(arg_string)s);
  CheckGLError();
}

"""
      name_arg = func.GetResourceIdArg()
      f.write(code % {
          'name': func.name,
          'arg_string': func.MakeOriginalArgString(""),
          'id': name_arg.name,
          'type': name_arg.resource_type,
          'lc_type': name_arg.resource_type.lower(),
        })

  def WriteGLES2ImplementationUnitTest(self, func, f):
    """Overrriden from TypeHandler."""
    client_test = func.GetInfo('client_test', True)
    if not client_test:
      return
    code = """
TEST_F(%(prefix)sImplementationTest, %(name)s) {
  struct Cmds {
    cmds::%(name)s cmd;
  };
  Cmds expected;
  expected.cmd.Init(%(cmd_args)s);

  gl_->%(name)s(%(args)s);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));"""
    if not func.IsES3():
      code += """
  ClearCommands();
  gl_->%(name)s(%(args)s);
  EXPECT_TRUE(NoCommandsWritten());"""
    code += """
}
"""
    cmd_arg_strings = [
      arg.GetValidClientSideCmdArg(func) for arg in func.GetCmdArgs()
    ]
    gl_arg_strings = [
      arg.GetValidClientSideArg(func) for arg in func.GetOriginalArgs()
    ]

    f.write(code % {
          'prefix' : _prefix,
          'name': func.name,
          'args': ", ".join(gl_arg_strings),
          'cmd_args': ", ".join(cmd_arg_strings),
        })

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class GENnHandler(TypeHandler):
  """Handler for glGen___ type functions."""

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""

  def WriteGetDataSizeCode(self, func, arg, f):
    """Overrriden from TypeHandler."""
    code = """  uint32_t %(data_size)s;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&%(data_size)s)) {
    return error::kOutOfBounds;
  }
""" % {'data_size': arg.GetReservedSizeId()}
    f.write(code)

  def WriteHandlerImplementation (self, func, f):
    """Overrriden from TypeHandler."""
    raise NotImplementedError("GENn functions are immediate")

  def WriteImmediateHandlerImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    param_name = func.GetLastOriginalArg().name
    f.write("  auto %(name)s_copy = std::make_unique<GLuint[]>(n);\n"
            "  GLuint* %(name)s_safe = %(name)s_copy.get();\n"
            "  std::copy(%(name)s, %(name)s + n, %(name)s_safe);\n"
            "  if (!%(ns)sCheckUniqueAndNonNullIds(n, %(name)s_safe) ||\n"
            "      !%(func)sHelper(n, %(name)s_safe)) {\n"
            "    return error::kInvalidArguments;\n"
            "  }\n" % {'name': param_name,
                       'func': func.original_name,
                       'ns': _Namespace()})

  def WriteGLES2Implementation(self, func, f):
    """Overrriden from TypeHandler."""
    log_code = ("""  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << %s[i]);
    }
  });""" % func.GetOriginalArgs()[1].name)
    args = {
        'log_code': log_code,
        'return_type': func.return_type,
        'prefix' : _prefix,
        'name': func.original_name,
        'typed_args': func.MakeTypedOriginalArgString(""),
        'args': func.MakeOriginalArgString(""),
        'resource_types': func.GetInfo('resource_types'),
        'count_name': func.GetOriginalArgs()[0].name,
      }
    f.write(
        "%(return_type)s %(prefix)sImplementation::"
        "%(name)s(%(typed_args)s) {\n" %
        args)
    func.WriteDestinationInitalizationValidation(f)
    self.WriteClientGLCallLog(func, f)
    for arg in func.GetOriginalArgs():
      arg.WriteClientSideValidationCode(f, func)
    not_shared = func.GetInfo('not_shared')
    if not_shared:
      alloc_code = ("""\
      IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::k%s);
      for (GLsizei ii = 0; ii < n; ++ii)
      %s[ii] = id_allocator->AllocateID();""" %
      (func.GetInfo('resource_types'), func.GetOriginalArgs()[1].name))
    else:
      alloc_code = ("""\
      GetIdHandler(SharedIdNamespaces::k%(resource_types)s)->
      MakeIds(this, 0, %(args)s);""" % args)
    args['alloc_code'] = alloc_code

    code = """\
    GPU_CLIENT_SINGLE_THREAD_CHECK();
    %(alloc_code)s
    %(name)sHelper(%(args)s);
    helper_->%(name)sImmediate(%(args)s);
    """
    if not not_shared:
      code += """\
      if (share_group_->bind_generates_resource())
      helper_->CommandBufferHelper::Flush();
      """
    code += """\
    %(log_code)s
    CheckGLError();
    }

    """
    f.write(code % args)

  def WriteGLES2ImplementationUnitTest(self, func, f):
    """Overrriden from TypeHandler."""
    code = """
TEST_F(%(prefix)sImplementationTest, %(name)s) {
  GLuint ids[2] = { 0, };
  struct Cmds {
    cmds::%(name)sImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(std::size(ids), &ids[0]);
  expected.data[0] = k%(types)sStartId;
  expected.data[1] = k%(types)sStartId + 1;
  gl_->%(name)s(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(k%(types)sStartId, ids[0]);
  EXPECT_EQ(k%(types)sStartId + 1, ids[1]);
}
"""
    f.write(code % {
          'prefix' : _prefix,
          'name': func.name,
          'types': func.GetInfo('resource_types'),
        })

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""
    raise NotImplementedError("GENn functions are immediate")

  def WriteImmediateServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId));
  cmds::%(name)s* cmd = GetImmediateAs<cmds::%(name)s>();
  GLuint temp = kNewClientId;
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmd->Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(*cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(Get%(resource_name)s(kNewClientId) != nullptr);
}
"""
    self.WriteValidUnitTest(func, f, valid_test, {
        'resource_name': func.GetInfo('resource_type'),
      }, *extras)
    duplicate_id_test = """
TEST_P(%(test_name)s, %(name)sDuplicateOrNullIds) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(_, _)).Times(0);
  cmds::%(name)s* cmd = GetImmediateAs<cmds::%(name)s>();
  GLuint temp[3] = {kNewClientId, kNewClientId + 1, kNewClientId};
  SpecializedSetup<cmds::%(name)s, 1>(true);
  cmd->Init(3, temp);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(*cmd, sizeof(temp)));
  EXPECT_TRUE(Get%(resource_name)s(kNewClientId) == nullptr);
  EXPECT_TRUE(Get%(resource_name)s(kNewClientId + 1) == nullptr);
  GLuint null_id[2] = {kNewClientId, 0};
  cmd->Init(2, null_id);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(*cmd, sizeof(temp)));
  EXPECT_TRUE(Get%(resource_name)s(kNewClientId) == nullptr);
}
    """
    self.WriteValidUnitTest(func, f, duplicate_id_test, {
        'resource_name': func.GetInfo('resource_type'),
      }, *extras)
    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(_, _)).Times(0);
  cmds::%(name)s* cmd = GetImmediateAs<cmds::%(name)s>();
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmd->Init(1, &client_%(resource_name)s_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(*cmd, sizeof(&client_%(resource_name)s_id_)));
}
"""
    self.WriteValidUnitTest(func, f, invalid_test, {
          'resource_name': func.GetInfo('resource_type').lower(),
        }, *extras)

  def WriteImmediateCmdComputeSize(self, _func, f):
    """Overrriden from TypeHandler."""
    f.write("  static uint32_t ComputeDataSize(GLsizei _n) {\n")
    f.write(
        "    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT\n")
    f.write("  }\n")
    f.write("\n")
    f.write("  static uint32_t ComputeSize(GLsizei _n) {\n")
    f.write("    return static_cast<uint32_t>(\n")
    f.write("        sizeof(ValueType) + ComputeDataSize(_n));  // NOLINT\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdSetHeader(self, _func, f):
    """Overrriden from TypeHandler."""
    f.write("  void SetHeader(GLsizei _n) {\n")
    f.write("    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    f.write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    f.write("    SetHeader(_n);\n")
    args = func.GetCmdArgs()
    for arg in args:
      f.write("    %s = _%s;\n" % (arg.name, arg.name))
    f.write("    memcpy(ImmediateDataAddress(this),\n")
    f.write("           _%s, ComputeDataSize(_n));\n" % last_arg.name)
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    f.write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    f.write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    f.write("    const uint32_t size = ComputeSize(_n);\n")
    f.write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdHelper(self, func, f):
    """Overrriden from TypeHandler."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32_t size = %(lp)s::cmds::%(name)s::ComputeSize(n);
    %(lp)s::cmds::%(name)s* c =
        GetImmediateCmdSpaceTotalSize<%(lp)s::cmds::%(name)s>(size);
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    f.write(code % {
          "lp" : _lower_prefix,
          "name": func.name,
          "typed_args": func.MakeTypedOriginalArgString(""),
          "args": func.MakeOriginalArgString(""),
        })

  def WriteImmediateFormatTest(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("TEST_F(%sFormatTest, %s) {\n" % (_prefix, func.name))
    f.write("  static GLuint ids[] = { 12, 23, 34, };\n")
    f.write("  cmds::%s& cmd = *GetBufferAs<cmds::%s>();\n" %
               (func.name, func.name))
    f.write("  void* next_cmd = cmd.Set(\n")
    f.write("      &cmd, static_cast<GLsizei>(std::size(ids)), ids);\n")
    f.write("  EXPECT_EQ(static_cast<uint32_t>(cmds::%s::kCmdId),\n" %
               func.name)
    f.write("            cmd.header.command);\n")
    f.write("  EXPECT_EQ(sizeof(cmd) +\n")
    f.write("            RoundSizeToMultipleOfEntries(cmd.n * 4u),\n")
    f.write("            cmd.header.size * 4u);\n")
    f.write("  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);\n");
    f.write("  CheckBytesWrittenMatchesExpectedSize(\n")
    f.write("      next_cmd, sizeof(cmd) +\n")
    f.write("      RoundSizeToMultipleOfEntries(std::size(ids) * 4u));\n")
    f.write("  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd),\n")
    f.write("                      sizeof(ids)));\n")
    f.write("}\n")
    f.write("\n")


class CreateHandler(TypeHandler):
  """Handler for glCreate___ type functions."""

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    func.AddCmdArg(Argument("client_id", 'uint32_t'))

  def __GetResourceType(self, func):
    if func.return_type == "GLsync":
      return "Sync"
    return func.name[6:]  # Create*

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  %(id_type_cast)sEXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s))
      .WillOnce(Return(%(const_service_id)s));
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s%(comma)skNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());"""
    if func.IsES3():
      valid_test += """
  %(return_type)s service_id = 0;
  EXPECT_TRUE(Get%(resource_type)sServiceId(kNewClientId, &service_id));
  EXPECT_EQ(%(const_service_id)s, service_id);
}
"""
    else:
      valid_test += """
  EXPECT_TRUE(Get%(resource_type)s(kNewClientId));
}
"""
    comma = ""
    cmd_arg_count = 0
    for arg in func.GetOriginalArgs():
      if not arg.IsConstant():
        cmd_arg_count += 1
    if cmd_arg_count:
      comma = ", "
    if func.return_type == 'GLsync':
      id_type_cast = ("const GLsync kNewServiceIdGLuint = reinterpret_cast"
                      "<GLsync>(kNewServiceId);\n  ")
      const_service_id = "kNewServiceIdGLuint"
    else:
      id_type_cast = ""
      const_service_id = "kNewServiceId"
    self.WriteValidUnitTest(func, f, valid_test, {
          'comma': comma,
          'resource_type': self.__GetResourceType(func),
          'return_type': func.return_type,
          'id_type_cast': id_type_cast,
          'const_service_id': const_service_id,
        }, *extras)
    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s%(comma)skNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, f, invalid_test, {
          'comma': comma,
        }, *extras)

  def WriteHandlerImplementation (self, func, f):
    """Overrriden from TypeHandler."""
    if func.IsES3():
      code = """  uint32_t client_id = c.client_id;
  %(return_type)s service_id = 0;
  if (group_->Get%(resource_name)sServiceId(client_id, &service_id)) {
    return error::kInvalidArguments;
  }
  service_id = %(gl_func_name)s(%(gl_args)s);
  if (service_id) {
    group_->Add%(resource_name)sId(client_id, service_id);
  }
"""
    else:
      code = """  uint32_t client_id = c.client_id;
  if (Get%(resource_name)s(client_id)) {
    return error::kInvalidArguments;
  }
  %(return_type)s service_id = %(gl_func_name)s(%(gl_args)s);
  if (service_id) {
    Create%(resource_name)s(client_id, service_id%(gl_args_with_comma)s);
  }
"""
    f.write(code % {
        'resource_name': self.__GetResourceType(func),
        'return_type': func.return_type,
        'gl_func_name': func.GetGLFunctionName(),
        'gl_args': func.MakeOriginalArgString(""),
        'gl_args_with_comma': func.MakeOriginalArgString("", True) })

  def WriteGLES2Implementation(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("%s %sImplementation::%s(%s) {\n" %
               (func.return_type, _prefix, func.original_name,
                func.MakeTypedOriginalArgString("")))
    f.write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
    func.WriteDestinationInitalizationValidation(f)
    self.WriteClientGLCallLog(func, f)
    for arg in func.GetOriginalArgs():
      arg.WriteClientSideValidationCode(f, func)
    f.write("  GLuint client_id;\n")
    not_shared = func.GetInfo('not_shared')
    if not_shared:
      f.write('IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::k%s);' %
              func.GetInfo('resource_types'))
      f.write('client_id = id_allocator->AllocateID();')
    else:
      if func.return_type == "GLsync":
        f.write(
            "  GetIdHandler(SharedIdNamespaces::kSyncs)->\n")
      else:
        f.write(
            "  GetIdHandler(SharedIdNamespaces::kProgramsAndShaders)->\n")
      f.write("      MakeIds(this, 0, 1, &client_id);\n")
    f.write("  helper_->%s(%s);\n" %
               (func.name, func.MakeCmdArgString("")))
    f.write('  GPU_CLIENT_LOG("returned " << client_id);\n')
    f.write("  CheckGLError();\n")
    if func.return_type == "GLsync":
      f.write("  return reinterpret_cast<GLsync>(client_id);\n")
    else:
      f.write("  return client_id;\n")
    f.write("}\n")
    f.write("\n")

  def WritePassthroughServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class DeleteHandler(TypeHandler):
  """Handler for glDelete___ single resource type functions."""

  def WriteServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    if func.IsES3() or func.IsES31():
      TypeHandler.WriteServiceImplementation(self, func, f)
    # HandleDeleteShader and HandleDeleteProgram are manually written.

  def WriteGLES2Implementation(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("%s %sImplementation::%s(%s) {\n" %
               (func.return_type, _prefix, func.original_name,
                func.MakeTypedOriginalArgString("")))
    f.write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
    func.WriteDestinationInitalizationValidation(f)
    self.WriteClientGLCallLog(func, f)
    for arg in func.GetOriginalArgs():
      arg.WriteClientSideValidationCode(f, func)
    f.write(
        "  if (%s == 0)\n    return;" % func.GetOriginalArgs()[-1].name);
    f.write("  %sHelper(%s);\n" %
               (func.original_name, func.GetOriginalArgs()[-1].name))
    f.write("  CheckGLError();\n")
    f.write("}\n")
    f.write("\n")

  def WriteHandlerImplementation (self, func, f):
    """Overrriden from TypeHandler."""
    assert len(func.GetOriginalArgs()) == 1
    arg = func.GetOriginalArgs()[0]
    f.write("  %sHelper(%s);\n" % (func.original_name, arg.name))

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class DELnHandler(TypeHandler):
  """Handler for glDelete___ type functions."""

  def WriteGetDataSizeCode(self, func, arg, f):
    """Overrriden from TypeHandler."""
    code = """  uint32_t %(data_size)s;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&%(data_size)s)) {
    return error::kOutOfBounds;
  }
""" % {'data_size': arg.GetReservedSizeId()}
    f.write(code)

  def WriteGLES2ImplementationUnitTest(self, func, f):
    """Overrriden from TypeHandler."""
    code = """
TEST_F(%(prefix)sImplementationTest, %(name)s) {
  GLuint ids[2] = { k%(types)sStartId, k%(types)sStartId + 1 };
  struct Cmds {
    cmds::%(name)sImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(std::size(ids), &ids[0]);
  expected.data[0] = k%(types)sStartId;
  expected.data[1] = k%(types)sStartId + 1;
  gl_->%(name)s(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
"""
    f.write(code % {
          'prefix' : _prefix,
          'name': func.name,
          'types': func.GetInfo('resource_types'),
        })

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(1, Pointee(kService%(upper_resource_name)sId)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = client_%(resource_name)s_id_;
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      Get%(upper_resource_name)s(client_%(resource_name)s_id_) == nullptr);
}
"""
    self.WriteValidUnitTest(func, f, valid_test, {
          'resource_name': func.GetInfo('resource_type').lower(),
          'upper_resource_name': func.GetInfo('resource_type'),
        }, *extras)
    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs) {
  GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
"""
    self.WriteValidUnitTest(func, f, invalid_test, *extras)

  def WriteImmediateServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(1, Pointee(kService%(upper_resource_name)sId)))
      .Times(1);
  cmds::%(name)s& cmd = *GetImmediateAs<cmds::%(name)s>();
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmd.Init(1, &client_%(resource_name)s_id_);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(client_%(resource_name)s_id_)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      Get%(upper_resource_name)s(client_%(resource_name)s_id_) == nullptr);
}
"""
    self.WriteValidUnitTest(func, f, valid_test, {
          'resource_name': func.GetInfo('resource_type').lower(),
          'upper_resource_name': func.GetInfo('resource_type'),
        }, *extras)
    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs) {
  cmds::%(name)s& cmd = *GetImmediateAs<cmds::%(name)s>();
  SpecializedSetup<cmds::%(name)s, 0>(false);
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
"""
    self.WriteValidUnitTest(func, f, invalid_test, *extras)

  def WriteHandlerImplementation (self, func, f):
    """Overrriden from TypeHandler."""
    f.write("  %sHelper(n, %s);\n" %
               (func.name, func.GetLastOriginalArg().name))

  def WriteImmediateHandlerImplementation (self, func, f):
    """Overrriden from TypeHandler."""
    f.write("  %sHelper(n, %s);\n" %
            (func.original_name, func.GetLastOriginalArg().name))

  def WriteGLES2Implementation(self, func, f):
    """Overrriden from TypeHandler."""
    impl_func = func.GetInfo('impl_func', True)
    if impl_func:
      args = {
          'return_type': func.return_type,
          'prefix' : _prefix,
          'name': func.original_name,
          'typed_args': func.MakeTypedOriginalArgString(""),
          'args': func.MakeOriginalArgString(""),
          'resource_type': func.GetInfo('resource_type').lower(),
          'count_name': func.GetOriginalArgs()[0].name,
        }
      f.write(
          "%(return_type)s %(prefix)sImplementation::"
          "%(name)s(%(typed_args)s) {\n" %
          args)
      f.write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
      func.WriteDestinationInitalizationValidation(f)
      self.WriteClientGLCallLog(func, f)
      f.write("""  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << %s[i]);
    }
  });
""" % func.GetOriginalArgs()[1].name)
      f.write("""  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      DCHECK(%s[i] != 0);
    }
  });
""" % func.GetOriginalArgs()[1].name)
      for arg in func.GetOriginalArgs():
        arg.WriteClientSideValidationCode(f, func)
      code = """  %(name)sHelper(%(args)s);
  CheckGLError();
}

"""
      f.write(code % args)

  def WriteImmediateCmdComputeSize(self, _func, f):
    """Overrriden from TypeHandler."""
    f.write("  static uint32_t ComputeDataSize(GLsizei _n) {\n")
    f.write(
        "    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT\n")
    f.write("  }\n")
    f.write("\n")
    f.write("  static uint32_t ComputeSize(GLsizei _n) {\n")
    f.write("    return static_cast<uint32_t>(\n")
    f.write("        sizeof(ValueType) + ComputeDataSize(_n));  // NOLINT\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdSetHeader(self, _func, f):
    """Overrriden from TypeHandler."""
    f.write("  void SetHeader(GLsizei _n) {\n")
    f.write("    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    f.write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    f.write("    SetHeader(_n);\n")
    args = func.GetCmdArgs()
    for arg in args:
      f.write("    %s = _%s;\n" % (arg.name, arg.name))
    f.write("    memcpy(ImmediateDataAddress(this),\n")
    f.write("           _%s, ComputeDataSize(_n));\n" % last_arg.name)
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    f.write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    f.write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    f.write("    const uint32_t size = ComputeSize(_n);\n")
    f.write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdHelper(self, func, f):
    """Overrriden from TypeHandler."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32_t size = %(lp)s::cmds::%(name)s::ComputeSize(n);
    %(lp)s::cmds::%(name)s* c =
        GetImmediateCmdSpaceTotalSize<%(lp)s::cmds::%(name)s>(size);
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    f.write(code % {
          "lp" : _lower_prefix,
          "name": func.name,
          "typed_args": func.MakeTypedOriginalArgString(""),
          "args": func.MakeOriginalArgString(""),
        })

  def WriteImmediateFormatTest(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("TEST_F(%sFormatTest, %s) {\n" % (_prefix, func.name))
    f.write("  static GLuint ids[] = { 12, 23, 34, };\n")
    f.write("  cmds::%s& cmd = *GetBufferAs<cmds::%s>();\n" %
               (func.name, func.name))
    f.write("  void* next_cmd = cmd.Set(\n")
    f.write("      &cmd, static_cast<GLsizei>(std::size(ids)), ids);\n")
    f.write("  EXPECT_EQ(static_cast<uint32_t>(cmds::%s::kCmdId),\n" %
               func.name)
    f.write("            cmd.header.command);\n")
    f.write("  EXPECT_EQ(sizeof(cmd) +\n")
    f.write("            RoundSizeToMultipleOfEntries(cmd.n * 4u),\n")
    f.write("            cmd.header.size * 4u);\n")
    f.write("  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);\n");
    f.write("  CheckBytesWrittenMatchesExpectedSize(\n")
    f.write("      next_cmd, sizeof(cmd) +\n")
    f.write("      RoundSizeToMultipleOfEntries(std::size(ids) * 4u));\n")
    f.write("  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd),\n")
    f.write("                      sizeof(ids)));\n")
    f.write("}\n")
    f.write("\n")


class GETnHandler(TypeHandler):
  """Handler for GETn for glGetBooleanv, glGetFloatv, ... type functions."""

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    TypeHandler.InitFunction(self, func)

    if func.name == 'GetSynciv':
      return

    arg_insert_point = len(func.passthrough_service_doer_args) - 1;
    func.passthrough_service_doer_args.insert(
        arg_insert_point, Argument('length', 'GLsizei*'))
    func.passthrough_service_doer_args.insert(
        arg_insert_point, Argument('bufsize', 'GLsizei'))

  def NeedsDataTransferFunction(self, func):
    """Overriden from TypeHandler."""
    return False

  def WriteServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    self.WriteServiceHandlerFunctionHeader(func, f)
    if func.IsES31():
      return
    last_arg = func.GetLastOriginalArg()
    # All except shm_id and shm_offset.
    all_but_last_args = func.GetCmdArgs()[:-2]
    for arg in all_but_last_args:
      arg.WriteGetCode(f)

    code = """  typedef cmds::%(func_name)s::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":%(func_name)s", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.%(last_arg_name)s_shm_id, c.%(last_arg_name)s_shm_offset,
      checked_size);
  %(last_arg_type)s %(last_arg_name)s = result ? result->GetData() : nullptr;
"""
    f.write(code % {
        'last_arg_type': last_arg.type,
        'last_arg_name': last_arg.name,
        'func_name': func.name,
      })
    func.WriteHandlerValidation(f)
    code = """  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
"""
    shadowed = func.GetInfo('shadowed')
    if not shadowed:
      f.write('  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("%s");\n' % func.name)
    f.write(code)
    func.WriteHandlerImplementation(f)
    if shadowed:
      code = """  result->SetNumResults(num_values);
  return error::kNoError;
}
"""
    else:
     code = """  GLenum error = LOCAL_PEEK_GL_ERROR("%(func_name)s");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

"""
    f.write(code % {'func_name': func.name})

  def WritePassthroughServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    self.WritePassthroughServiceFunctionHeader(func, f)
    last_arg = func.GetLastOriginalArg()
    # All except shm_id and shm_offset.
    all_but_last_args = func.GetCmdArgs()[:-2]
    for arg in all_but_last_args:
      arg.WriteGetCode(f)

    code = """  unsigned int buffer_size = 0;
  typedef cmds::%(func_name)s::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.%(last_arg_name)s_shm_id, c.%(last_arg_name)s_shm_offset,
      sizeof(Result), &buffer_size);
  %(last_arg_type)s %(last_arg_name)s = result ? result->GetData() : nullptr;
  if (%(last_arg_name)s == nullptr) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
"""
    f.write(code % {
        'last_arg_type': last_arg.type,
        'last_arg_name': last_arg.name,
        'func_name': func.name,
      })

    self.WritePassthroughServiceFunctionDoerCall(func, f)

    code = """  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

"""
    f.write(code % {'func_name': func.name})

  def WriteGLES2Implementation(self, func, f):
    """Overrriden from TypeHandler."""
    impl_func = func.GetInfo('impl_func', True)
    if impl_func:
      f.write("%s %sImplementation::%s(%s) {\n" %
                 (func.return_type, _prefix, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      f.write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
      func.WriteDestinationInitalizationValidation(f)
      self.WriteClientGLCallLog(func, f)
      for arg in func.GetOriginalArgs():
        arg.WriteClientSideValidationCode(f, func)
      all_but_last_args = func.GetOriginalArgs()[:-1]
      args = []
      has_length_arg = False
      for arg in all_but_last_args:
        if arg.type == 'GLsync':
          args.append('ToGLuint(%s)' % arg.name)
        elif arg.name.endswith('size') and arg.type == 'GLsizei':
          continue
        elif arg.name == 'length':
          has_length_arg = True
          continue
        else:
          args.append(arg.name)
      arg_string = ", ".join(args)
      all_arg_string = (
          ", ".join([
            "%s" % arg.name
              for arg in func.GetOriginalArgs() if not arg.IsConstant()]))
      self.WriteTraceEvent(func, f)
      code = """  if (%(func_name)sHelper(%(all_arg_string)s)) {
    return;
  }
  typedef cmds::%(func_name)s::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->%(func_name)s(%(arg_string)s,
      GetResultShmId(), result.offset());
  if (!WaitForCmd()) {
    return;
  }
  result->CopyResult(%(last_arg_name)s);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });"""
      if has_length_arg:
        code += """
  if (length) {
    *length = result->GetNumResults();
  }"""
      code += """
  CheckGLError();
}
"""
      f.write(code % {
          'func_name': func.name,
          'arg_string': arg_string,
          'all_arg_string': all_arg_string,
          'last_arg_name': func.GetLastOriginalArg().name,
        })

  def WriteGLES2ImplementationUnitTest(self, func, f):
    """Writes the GLES2 Implemention unit test."""
    code = """
TEST_F(%(prefix)sImplementationTest, %(name)s) {
  struct Cmds {
    cmds::%(name)s cmd;
  };
  typedef cmds::%(name)s::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(
      sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(%(cmd_args)s, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->%(name)s(%(args)s, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}
"""
    first_cmd_arg = func.GetCmdArgs()[0].GetValidNonCachedClientSideCmdArg(func)
    if not first_cmd_arg:
      return

    first_gl_arg = func.GetOriginalArgs()[0].GetValidNonCachedClientSideArg(
        func)

    cmd_arg_strings = [first_cmd_arg]
    for arg in func.GetCmdArgs()[1:-2]:
      cmd_arg_strings.append(arg.GetValidClientSideCmdArg(func))
    gl_arg_strings = [first_gl_arg]
    for arg in func.GetOriginalArgs()[1:-1]:
      gl_arg_strings.append(arg.GetValidClientSideArg(func))

    f.write(code % {
          'prefix' : _prefix,
          'name': func.name,
          'args': ", ".join(gl_arg_strings),
          'cmd_args': ", ".join(cmd_arg_strings),
        })

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillRepeatedly(Return(GL_NO_ERROR));
  SpecializedSetup<cmds::%(name)s, 0>(true);
  typedef cmds::%(name)s::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(local_gl_args)s));
  result->size = 0;
  cmds::%(name)s cmd;
  cmd.Init(%(cmd_args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                %(valid_pname)s),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    gl_arg_strings = []
    cmd_arg_strings = []
    valid_pname = ''
    for arg in func.GetOriginalArgs()[:-1]:
      if arg.name == 'length':
        gl_arg_value = 'nullptr'
      elif arg.name.endswith('size'):
        gl_arg_value = ("decoder_->GetGLES2Util()->GLGetNumValuesReturned(%s)" %
            valid_pname)
      elif arg.type == 'GLsync':
        gl_arg_value = 'reinterpret_cast<GLsync>(kServiceSyncId)'
      else:
        gl_arg_value = arg.GetValidGLArg(func)
      gl_arg_strings.append(gl_arg_value)
      if arg.name == 'pname':
        valid_pname = gl_arg_value
      if arg.name.endswith('size') or arg.name == 'length':
        continue
      if arg.type == 'GLsync':
        arg_value = 'client_sync_id_'
      else:
        arg_value = arg.GetValidArg(func)
      cmd_arg_strings.append(arg_value)
    if func.GetInfo('gl_test_func') == 'glGetIntegerv':
      gl_arg_strings.append("_")
    else:
      gl_arg_strings.append("result->GetData()")
    cmd_arg_strings.append("shared_memory_id_")
    cmd_arg_strings.append("shared_memory_offset_")

    self.WriteValidUnitTest(func, f, valid_test, {
        'local_gl_args': ", ".join(gl_arg_strings),
        'cmd_args': ", ".join(cmd_arg_strings),
        'valid_pname': valid_pname,
      }, *extras)

    if not func.IsES3():
      invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmds::%(name)s::Result* result =
      static_cast<cmds::%(name)s::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);%(gl_error_test)s
}
"""
      self.WriteInvalidUnitTest(func, f, invalid_test, *extras)

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class ArrayArgTypeHandler(TypeHandler):
  """Base class for type handlers that handle args that are arrays"""

  def GetArrayType(self, func):
    """Returns the type of the element in the element array being PUT to."""
    for arg in func.GetOriginalArgs():
      if arg.IsPointer():
        element_type = arg.GetPointedType()
        return element_type

    # Special case: array type handler is used for a function that is forwarded
    # to the actual array type implementation
    element_type = func.GetOriginalArgs()[-1].type
    assert all(arg.type == element_type \
               for arg in func.GetOriginalArgs()[-self.GetArrayCount(func):])
    return element_type

  def GetArrayCount(self, func):
    """Returns the count of the elements in the array being PUT to."""
    return func.GetInfo('count')

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class PUTHandler(ArrayArgTypeHandler):
  """Handler for glTexParameter_v, glVertexAttrib_v functions."""

  def WriteServiceUnitTest(self, func, f, *extras):
    """Writes the service unit test for a command."""
    expected_call = "EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));"
    if func.GetInfo("first_element_only"):
      gl_arg_strings = [
        arg.GetValidGLArg(func) for arg in func.GetOriginalArgs()
      ]
      gl_arg_strings[-1] = "*" + gl_arg_strings[-1]
      expected_call = ("EXPECT_CALL(*gl_, %%(gl_func_name)s(%s));" %
          ", ".join(gl_arg_strings))
    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  GetSharedMemoryAs<%(data_type)s*>()[0] = %(data_value)s;
  %(expected_call)s
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    extra = {
      'data_type': self.GetArrayType(func),
      'data_value': func.GetInfo('data_value') or '0',
      'expected_call': expected_call,
    }
    self.WriteValidUnitTest(func, f, valid_test, extra, *extras)

    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  GetSharedMemoryAs<%(data_type)s*>()[0] = %(data_value)s;
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, f, invalid_test, extra, *extras)

  def WriteImmediateServiceUnitTest(self, func, f, *extras):
    """Writes the service unit test for a command."""
    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  cmds::%(name)s& cmd = *GetImmediateAs<cmds::%(name)s>();
  SpecializedSetup<cmds::%(name)s, 0>(true);
  %(data_type)s temp[%(data_count)s] = { %(data_value)s, };
  cmd.Init(%(gl_client_args)s, &temp[0]);
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(%(gl_args)s, %(expectation)s));
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    gl_client_arg_strings = [
      arg.GetValidArg(func) for arg in func.GetOriginalArgs()[0:-1]
    ]
    gl_arg_strings = [
      arg.GetValidGLArg(func) for arg in func.GetOriginalArgs()[0:-1]
    ]
    gl_any_strings = ["_"] * len(gl_arg_strings)
    data_count = self.GetArrayCount(func)
    if func.GetInfo('first_element_only'):
      expectation = "temp[0]"
    else:
      expectation = "PointsToArray(temp, %s)" % data_count

    extra = {
      'expectation': expectation,
      'data_type': self.GetArrayType(func),
      'data_count': data_count,
      'data_value': func.GetInfo('data_value') or '0',
      'gl_client_args': ", ".join(gl_client_arg_strings),
      'gl_args': ", ".join(gl_arg_strings),
      'gl_any_args': ", ".join(gl_any_strings),
    }
    self.WriteValidUnitTest(func, f, valid_test, extra, *extras)

    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  cmds::%(name)s& cmd = *GetImmediateAs<cmds::%(name)s>();"""
    if func.IsES3():
      invalid_test += """
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_any_args)s, _)).Times(1);
"""
    else:
      invalid_test += """
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_any_args)s, _)).Times(0);
"""
    invalid_test += """
  SpecializedSetup<cmds::%(name)s, 0>(false);
  %(data_type)s temp[%(data_count)s] = { %(data_value)s, };
  cmd.Init(%(all_but_last_args)s, &temp[0]);
  EXPECT_EQ(error::%(parse_result)s,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  %(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, f, invalid_test, extra, *extras)

  def WriteGetDataSizeCode(self, func, arg, f):
    """Overrriden from TypeHandler."""
    code = ("""  uint32_t %(data_size)s;
  if (!%(namespace)sGLES2Util::""" +
"""ComputeDataSize<%(arrayType)s, %(arrayCount)d>(1, &%(data_size)s)) {
    return error::kOutOfBounds;
  }
""")
    f.write(code % {'data_size': arg.GetReservedSizeId(),
                    'namespace': _Namespace(),
                    'arrayType': self.GetArrayType(func),
                    'arrayCount': self.GetArrayCount(func)})
    if func.IsImmediate():
      f.write("  if (%s > immediate_data_size) {\n" % arg.GetReservedSizeId())
      f.write("    return error::kOutOfBounds;\n")
      f.write("  }\n")

  def __NeedsToCalcDataCount(self, func):
    use_count_func = func.GetInfo('use_count_func')
    return use_count_func not in (None, False)

  def WriteGLES2Implementation(self, func, f):
    """Overrriden from TypeHandler."""
    impl_func = func.GetInfo('impl_func')
    if impl_func not in (None, True):
      return;
    f.write("%s %sImplementation::%s(%s) {\n" %
               (func.return_type, _prefix, func.original_name,
                func.MakeTypedOriginalArgString("")))
    f.write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
    func.WriteDestinationInitalizationValidation(f)
    self.WriteClientGLCallLog(func, f)

    if self.__NeedsToCalcDataCount(func):
      f.write("  uint32_t count = %sGLES2Util::Calc%sDataCount(%s);\n" %
                 (_Namespace(), func.name, func.GetOriginalArgs()[0].name))
      f.write("  DCHECK_LE(count, %du);\n" % self.GetArrayCount(func))
      f.write("  if (count == 0) {\n")
      f.write("    SetGLErrorInvalidEnum(\"%s\", %s, \"%s\");\n" %
                 (func.prefixed_name, func.GetOriginalArgs()[0].name,
                  func.GetOriginalArgs()[0].name))
      f.write("    return;\n")
      f.write("  }\n")
    else:
      f.write("  uint32_t count = %d;" % self.GetArrayCount(func))
    f.write("  for (uint32_t ii = 0; ii < count; ++ii) {\n")
    f.write('    GPU_CLIENT_LOG("value[" << ii << "]: " << %s[ii]);\n' %
               func.GetLastOriginalArg().name)
    f.write("  }\n")
    for arg in func.GetOriginalArgs():
      arg.WriteClientSideValidationCode(f, func)
    f.write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    f.write("  CheckGLError();\n")
    f.write("}\n")
    f.write("\n")

  def WriteGLES2ImplementationUnitTest(self, func, f):
    """Writes the GLES2 Implemention unit test."""
    client_test = func.GetInfo('client_test', True)
    if not client_test:
      return;
    code = """
TEST_F(%(prefix)sImplementationTest, %(name)s) {
  %(type)s data[%(count)d] = {0};
  struct Cmds {
    cmds::%(name)sImmediate cmd;
    %(type)s data[%(count)d];
  };

  for (int jj = 0; jj < %(count)d; ++jj) {
    data[jj] = static_cast<%(type)s>(jj);
  }
  Cmds expected;
  expected.cmd.Init(%(cmd_args)s, &data[0]);
  gl_->%(name)s(%(args)s, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
"""
    cmd_arg_strings = [
      arg.GetValidClientSideCmdArg(func) for arg in func.GetCmdArgs()[0:-2]
    ]
    gl_arg_strings = [
      arg.GetValidClientSideArg(func) for arg in func.GetOriginalArgs()[0:-1]
    ]

    f.write(code % {
          'prefix' : _prefix,
          'name': func.name,
          'type': self.GetArrayType(func),
          'count': self.GetArrayCount(func),
          'args': ", ".join(gl_arg_strings),
          'cmd_args': ", ".join(cmd_arg_strings),
        })

  def WriteImmediateCmdComputeSize(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("  static uint32_t ComputeDataSize() {\n")
    f.write("    return static_cast<uint32_t>(\n")
    f.write("        sizeof(%s) * %d);\n" %
               (self.GetArrayType(func), self.GetArrayCount(func)))
    f.write("  }\n")
    f.write("\n")
    if self.__NeedsToCalcDataCount(func):
      f.write("  static uint32_t ComputeEffectiveDataSize(%s %s) {\n" %
                 (func.GetOriginalArgs()[0].type,
                  func.GetOriginalArgs()[0].name))
      f.write("    return static_cast<uint32_t>(\n")
      f.write("        sizeof(%s) * %sGLES2Util::Calc%sDataCount(%s));\n" %
                 (self.GetArrayType(func), _Namespace(), func.original_name,
                  func.GetOriginalArgs()[0].name))
      f.write("  }\n")
      f.write("\n")
    f.write("  static uint32_t ComputeSize() {\n")
    f.write("    return static_cast<uint32_t>(\n")
    f.write(
        "        sizeof(ValueType) + ComputeDataSize());\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdSetHeader(self, _func, f):
    """Overrriden from TypeHandler."""
    f.write("  void SetHeader() {\n")
    f.write(
        "    header.SetCmdByTotalSize<ValueType>(ComputeSize());\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    f.write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    f.write("    SetHeader();\n")
    args = func.GetCmdArgs()
    for arg in args:
      arg.WriteSetCode(f, 4, "_%s" % arg.name)
    f.write("    memcpy(ImmediateDataAddress(this),\n")
    if self.__NeedsToCalcDataCount(func):
      f.write("           _%s, ComputeEffectiveDataSize(%s));" %
                 (last_arg.name, func.GetOriginalArgs()[0].name))
      f.write("""
    DCHECK_GE(ComputeDataSize(), ComputeEffectiveDataSize(%(arg)s));
    char* pointer = reinterpret_cast<char*>(ImmediateDataAddress(this)) +
        ComputeEffectiveDataSize(%(arg)s);
    memset(pointer, 0, ComputeDataSize() - ComputeEffectiveDataSize(%(arg)s));
""" % { 'arg': func.GetOriginalArgs()[0].name, })
    else:
      f.write("           _%s, ComputeDataSize());\n" % last_arg.name)
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    f.write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    f.write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    f.write("    const uint32_t size = ComputeSize();\n")
    f.write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdHelper(self, func, f):
    """Overrriden from TypeHandler."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32_t size = %(lp)s::cmds::%(name)s::ComputeSize();
    %(lp)s::cmds::%(name)s* c =
        GetImmediateCmdSpaceTotalSize<%(lp)s::cmds::%(name)s>(size);
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    f.write(code % {
          "lp" : _lower_prefix,
          "name": func.name,
          "typed_args": func.MakeTypedOriginalArgString(""),
          "args": func.MakeOriginalArgString(""),
        })

  def WriteImmediateFormatTest(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("TEST_F(%sFormatTest, %s) {\n" % (_prefix, func.name))
    f.write("  const int kSomeBaseValueToTestWith = 51;\n")
    f.write("  static %s data[] = {\n" % self.GetArrayType(func))
    for v in range(0, self.GetArrayCount(func)):
      f.write("    static_cast<%s>(kSomeBaseValueToTestWith + %d),\n" %
                 (self.GetArrayType(func), v))
    f.write("  };\n")
    f.write("  cmds::%s& cmd = *GetBufferAs<cmds::%s>();\n" %
               (func.name, func.name))
    f.write("  void* next_cmd = cmd.Set(\n")
    f.write("      &cmd")
    args = func.GetCmdArgs()
    for value, arg in enumerate(args):
      f.write(",\n      static_cast<%s>(%d)" % (arg.type, value + 11))
    f.write(",\n      data);\n")
    args = func.GetCmdArgs()
    f.write("  EXPECT_EQ(static_cast<uint32_t>(cmds::%s::kCmdId),\n"
               % func.name)
    f.write("            cmd.header.command);\n")
    f.write("  EXPECT_EQ(sizeof(cmd) +\n")
    f.write("            RoundSizeToMultipleOfEntries(sizeof(data)),\n")
    f.write("            cmd.header.size * 4u);\n")
    for value, arg in enumerate(args):
      f.write("  EXPECT_EQ(static_cast<%s>(%d), %s);\n" %
                 (arg.type, value + 11, arg.GetArgAccessor('cmd')))
    f.write("  CheckBytesWrittenMatchesExpectedSize(\n")
    f.write("      next_cmd, sizeof(cmd) +\n")
    f.write("      RoundSizeToMultipleOfEntries(sizeof(data)));\n")
    # TODO: Check that data was inserted
    f.write("}\n")
    f.write("\n")


class PUTnHandler(ArrayArgTypeHandler):
  """Handler for PUTn 'glUniform__v' type functions."""

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overridden from TypeHandler."""
    ArrayArgTypeHandler.WriteServiceUnitTest(self, func, f, *extras)

    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgsCountTooLarge) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    gl_arg_strings = []
    arg_strings = []
    for count, arg in enumerate(func.GetOriginalArgs()):
      # hardcoded to match unit tests.
      if count == 0:
        # the location of the second element of the 2nd uniform.
        # defined in GLES2DecoderBase::SetupShaderForUniform
        gl_arg_strings.append("3")
        arg_strings.append("ProgramManager::MakeFakeLocation(1, 1)")
      elif count == 1:
        # the number of elements that gl will be called with.
        gl_arg_strings.append("3")
        # the number of elements requested in the command.
        arg_strings.append("5")
      else:
        gl_arg_strings.append(arg.GetValidGLArg(func))
        if not arg.IsConstant():
          arg_strings.append(arg.GetValidArg(func))
    extra = {
      'gl_args': ", ".join(gl_arg_strings),
      'args': ", ".join(arg_strings),
    }
    self.WriteValidUnitTest(func, f, valid_test, extra, *extras)

  def WriteImmediateServiceUnitTest(self, func, f, *extras):
    """Overridden from TypeHandler."""
    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  cmds::%(name)s& cmd = *GetImmediateAs<cmds::%(name)s>();
  SpecializedSetup<cmds::%(name)s, 0>(true);
  %(data_type)s temp[%(data_count)s * 2] = { 0, };
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(%(gl_args)s,
          PointsToArray(temp, %(data_count)s)));
  cmd.Init(%(args)s, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    gl_arg_strings = []
    gl_any_strings = []
    arg_strings = []
    for arg in func.GetOriginalArgs()[0:-1]:
      gl_arg_strings.append(arg.GetValidGLArg(func))
      gl_any_strings.append("_")
      if not arg.IsConstant():
        arg_strings.append(arg.GetValidArg(func))
    extra = {
      'data_type': self.GetArrayType(func),
      'data_count': self.GetArrayCount(func),
      'args': ", ".join(arg_strings),
      'gl_args': ", ".join(gl_arg_strings),
      'gl_any_args': ", ".join(gl_any_strings),
    }
    self.WriteValidUnitTest(func, f, valid_test, extra, *extras)

    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  cmds::%(name)s& cmd = *GetImmediateAs<cmds::%(name)s>();
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_any_args)s, _)).Times(0);
  SpecializedSetup<cmds::%(name)s, 0>(false);
  %(data_type)s temp[%(data_count)s * 2] = { 0, };
  cmd.Init(%(all_but_last_args)s, &temp[0]);
  EXPECT_EQ(error::%(parse_result)s,
            ExecuteImmediateCmd(cmd, sizeof(temp)));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, f, invalid_test, extra, *extras)

  def WriteGetDataSizeCode(self, func, arg, f):
    """Overrriden from TypeHandler."""
    code = ("""  uint32_t %(data_size)s = 0;
  if (count >= 0 && !%(namespace)sGLES2Util::""" +
"""ComputeDataSize<%(arrayType)s, %(arrayCount)d>(count, &%(data_size)s)) {
    return error::kOutOfBounds;
  }
""")
    f.write(code % {'data_size': arg.GetReservedSizeId(),
                    'namespace': _Namespace(),
                    'arrayType': self.GetArrayType(func),
                    'arrayCount': self.GetArrayCount(func)})
    if func.IsImmediate():
      f.write("  if (%s > immediate_data_size) {\n" % arg.GetReservedSizeId())
      f.write("    return error::kOutOfBounds;\n")
      f.write("  }\n")

  def WriteGLES2Implementation(self, func, f):
    """Overrriden from TypeHandler."""
    impl_func = func.GetInfo('impl_func')
    if impl_func not in (None, True):
      return;
    f.write("%s %sImplementation::%s(%s) {\n" %
               (func.return_type, _prefix, func.original_name,
                func.MakeTypedOriginalArgString("")))
    f.write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
    func.WriteDestinationInitalizationValidation(f)
    self.WriteClientGLCallLog(func, f)
    last_pointer_name = func.GetLastOriginalPointerArg().name
    f.write("""  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
""")
    values_str = ' << ", " << '.join(
        ["%s[%d + i * %d]" % (
            last_pointer_name, ndx, self.GetArrayCount(func)) for ndx in range(
                0, self.GetArrayCount(func))])
    f.write('       GPU_CLIENT_LOG("  " << i << ": " << %s);\n' % values_str)
    f.write("    }\n  });\n")
    for arg in func.GetOriginalArgs():
      arg.WriteClientSideValidationCode(f, func)
    f.write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeInitString("")))
    f.write("  CheckGLError();\n")
    f.write("}\n")
    f.write("\n")

  def WriteGLES2ImplementationUnitTest(self, func, f):
    """Writes the GLES2 Implemention unit test."""
    client_test = func.GetInfo('client_test', True)
    if not client_test:
      return;

    code = """
TEST_F(%(prefix)sImplementationTest, %(name)s) {
  %(type)s data[%(count_param)d][%(count)d] = {{0}};
  struct Cmds {
    cmds::%(name)sImmediate cmd;
    %(type)s data[%(count_param)d][%(count)d];
  };

  Cmds expected;
  for (int ii = 0; ii < %(count_param)d; ++ii) {
    for (int jj = 0; jj < %(count)d; ++jj) {
      data[ii][jj] = static_cast<%(type)s>(ii * %(count)d + jj);
    }
  }
  expected.cmd.Init(%(cmd_args)s);
  gl_->%(name)s(%(args)s);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
"""
    cmd_arg_strings = []
    for arg in func.GetCmdArgs():
      if arg.name.endswith("_shm_id"):
        cmd_arg_strings.append("&data[0][0]")
      elif arg.name.endswith("_shm_offset"):
        continue
      else:
        cmd_arg_strings.append(arg.GetValidClientSideCmdArg(func))
    gl_arg_strings = []
    count_param = 0
    for arg in func.GetOriginalArgs():
      if arg.IsPointer():
        valid_value = "&data[0][0]"
      else:
        valid_value = arg.GetValidClientSideArg(func)
      gl_arg_strings.append(valid_value)
      if arg.name == "count":
        count_param = int(valid_value)
    f.write(code % {
          'prefix' : _prefix,
          'name': func.name,
          'type': self.GetArrayType(func),
          'count': self.GetArrayCount(func),
          'args': ", ".join(gl_arg_strings),
          'cmd_args': ", ".join(cmd_arg_strings),
          'count_param': count_param,
        })

    # Test constants for invalid values, as they are not tested by the
    # service.
    constants = [
      arg for arg in func.GetOriginalArgs()[0:-1] if arg.IsConstant()
    ]
    if not constants:
      return

    code = """
TEST_F(%(prefix)sImplementationTest,
       %(name)sInvalidConstantArg%(invalid_index)d) {
  %(type)s data[%(count_param)d][%(count)d] = {{0}};
  for (int ii = 0; ii < %(count_param)d; ++ii) {
    for (int jj = 0; jj < %(count)d; ++jj) {
      data[ii][jj] = static_cast<%(type)s>(ii * %(count)d + jj);
    }
  }
  gl_->%(name)s(%(args)s);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(%(gl_error)s, CheckError());
}
"""
    for invalid_arg in constants:
      gl_arg_strings = []
      invalid = invalid_arg.GetInvalidArg(func)
      for arg in func.GetOriginalArgs():
        if arg is invalid_arg:
          gl_arg_strings.append(invalid[0])
        elif arg.IsPointer():
          gl_arg_strings.append("&data[0][0]")
        else:
          valid_value = arg.GetValidClientSideArg(func)
          gl_arg_strings.append(valid_value)
          if arg.name == "count":
            count_param = int(valid_value)

      f.write(code % {
        'prefix' : _prefix,
        'name': func.name,
        'invalid_index': func.GetOriginalArgs().index(invalid_arg),
        'type': self.GetArrayType(func),
        'count': self.GetArrayCount(func),
        'args': ", ".join(gl_arg_strings),
        'gl_error': invalid[2],
        'count_param': count_param,
      })


  def WriteImmediateCmdComputeSize(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("  static uint32_t ComputeDataSize(GLsizei _n) {\n")
    f.write("    return static_cast<uint32_t>(\n")
    f.write("        sizeof(%s) * %d * _n);  // NOLINT\n" %
               (self.GetArrayType(func), self.GetArrayCount(func)))
    f.write("  }\n")
    f.write("\n")
    f.write("  static uint32_t ComputeSize(GLsizei _n) {\n")
    f.write("    return static_cast<uint32_t>(\n")
    f.write(
        "        sizeof(ValueType) + ComputeDataSize(_n));  // NOLINT\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdSetHeader(self, _func, f):
    """Overrriden from TypeHandler."""
    f.write("  void SetHeader(GLsizei _n) {\n")
    f.write(
        "    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("  void Init(%s) {\n" %
               func.MakeTypedInitString("_"))
    f.write("    SetHeader(_count);\n")
    args = func.GetCmdArgs()
    for arg in args:
      arg.WriteSetCode(f, 4, "_%s" % arg.name)
    f.write("    memcpy(ImmediateDataAddress(this),\n")
    pointer_arg = func.GetLastOriginalPointerArg()
    f.write("           _%s, ComputeDataSize(_count));\n" % pointer_arg.name)
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedInitString("_", True))
    f.write("    static_cast<ValueType*>(cmd)->Init(%s);\n" %
               func.MakeInitString("_"))
    f.write("    const uint32_t size = ComputeSize(_count);\n")
    f.write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdHelper(self, func, f):
    """Overrriden from TypeHandler."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32_t size = %(lp)s::cmds::%(name)s::ComputeSize(count);
    %(lp)s::cmds::%(name)s* c =
        GetImmediateCmdSpaceTotalSize<%(lp)s::cmds::%(name)s>(size);
    if (c) {
      c->Init(%(args)s);
    }
  }

"""
    f.write(code % {
          "lp" : _lower_prefix,
          "name": func.name,
          "typed_args": func.MakeTypedInitString(""),
          "args": func.MakeInitString("")
        })

  def WriteImmediateFormatTest(self, func, f):
    """Overrriden from TypeHandler."""
    args = func.GetOriginalArgs()
    count_param = 0
    for arg in args:
      if arg.name == "count":
        count_param = int(arg.GetValidClientSideCmdArg(func))
    f.write("TEST_F(%sFormatTest, %s) {\n" % (_prefix, func.name))
    f.write("  const int kSomeBaseValueToTestWith = 51;\n")
    f.write("  static %s data[] = {\n" % self.GetArrayType(func))
    for v in range(0, self.GetArrayCount(func) * count_param):
      f.write("    static_cast<%s>(kSomeBaseValueToTestWith + %d),\n" %
                 (self.GetArrayType(func), v))
    f.write("  };\n")
    f.write("  cmds::%s& cmd = *GetBufferAs<cmds::%s>();\n" %
               (func.name, func.name))
    f.write("  const GLsizei kNumElements = %d;\n" % count_param)
    f.write("  const size_t kExpectedCmdSize =\n")
    f.write("      sizeof(cmd) + kNumElements * sizeof(%s) * %d;\n" %
               (self.GetArrayType(func), self.GetArrayCount(func)))
    f.write("  void* next_cmd = cmd.Set(\n")
    f.write("      &cmd")
    for value, arg in enumerate(args):
      if arg.IsPointer():
        f.write(",\n      data")
      elif arg.IsConstant():
        continue
      else:
        f.write(",\n      static_cast<%s>(%d)" % (arg.type, value + 1))
    f.write(");\n")
    f.write("  EXPECT_EQ(static_cast<uint32_t>(cmds::%s::kCmdId),\n" %
               func.name)
    f.write("            cmd.header.command);\n")
    f.write("  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);\n")
    for value, arg in enumerate(args):
      if arg.IsPointer() or arg.IsConstant():
        continue
      f.write("  EXPECT_EQ(static_cast<%s>(%d), %s);\n" %
                 (arg.type, value + 1, arg.GetArgAccessor('cmd')))
    f.write("  CheckBytesWrittenMatchesExpectedSize(\n")
    f.write("      next_cmd, sizeof(cmd) +\n")
    f.write("      RoundSizeToMultipleOfEntries(sizeof(data)));\n")
    # TODO: Check that data was inserted
    f.write("}\n")
    f.write("\n")

class PUTSTRHandler(ArrayArgTypeHandler):
  """Handler for functions that pass a string array."""

  def __GetDataArg(self, func):
    """Return the argument that points to the 2D char arrays"""
    for arg in func.GetOriginalArgs():
      if arg.IsPointer2D():
        return arg
    return None

  def __GetLengthArg(self, func):
    """Return the argument that holds length for each char array"""
    for arg in func.GetOriginalArgs():
      if arg.IsPointer() and not arg.IsPointer2D():
        return arg
    return None

  def WriteGLES2Implementation(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("%s %sImplementation::%s(%s) {\n" %
               (func.return_type, _prefix, func.original_name,
                func.MakeTypedOriginalArgString("")))
    f.write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
    func.WriteDestinationInitalizationValidation(f)
    self.WriteClientGLCallLog(func, f)
    data_arg = self.__GetDataArg(func)
    length_arg = self.__GetLengthArg(func)
    log_code_block = """  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei ii = 0; ii < count; ++ii) {
      if (%(data)s[ii]) {"""
    if length_arg == None:
      log_code_block += """
        GPU_CLIENT_LOG("  " << ii << ": ---\\n" << %(data)s[ii] << "\\n---");"""
    else:
      log_code_block += """
        if (%(length)s && %(length)s[ii] >= 0) {
          const std::string my_str(%(data)s[ii], %(length)s[ii]);
          GPU_CLIENT_LOG("  " << ii << ": ---\\n" << my_str << "\\n---");
        } else {
          GPU_CLIENT_LOG("  " << ii << ": ---\\n" << %(data)s[ii] << "\\n---");
        }"""
    log_code_block += """
      } else {
        GPU_CLIENT_LOG("  " << ii << ": NULL");
      }
    }
  });
"""
    f.write(log_code_block % {
          'data': data_arg.name,
          'length': length_arg.name if not length_arg == None else ''
      })
    for arg in func.GetOriginalArgs():
      arg.WriteClientSideValidationCode(f, func)

    bucket_args = []
    for arg in func.GetOriginalArgs():
      if arg.name == 'count' or arg == self.__GetLengthArg(func):
        continue
      if arg == self.__GetDataArg(func):
        bucket_args.append('kResultBucketId')
      else:
        bucket_args.append(arg.name)
    code_block = """
  if (!PackStringsToBucket(count, %(data)s, %(length)s, "gl%(func_name)s")) {
    return;
  }
  helper_->%(func_name)sBucket(%(bucket_args)s);
  helper_->SetBucketSize(kResultBucketId, 0);
  CheckGLError();
}

"""
    f.write(code_block % {
        'data': data_arg.name,
        'length': length_arg.name if not length_arg == None else 'nullptr',
        'func_name': func.name,
        'bucket_args': ', '.join(bucket_args),
      })

  def WriteGLES2ImplementationUnitTest(self, func, f):
    """Overrriden from TypeHandler."""
    code = """
TEST_F(%(prefix)sImplementationTest, %(name)s) {
  const uint32_t kBucketId = %(prefix)sImplementation::kResultBucketId;
  const char* kString1 = "happy";
  const char* kString2 = "ending";
  const size_t kString1Size = ::strlen(kString1) + 1;
  const size_t kString2Size = ::strlen(kString2) + 1;
  const size_t kHeaderSize = sizeof(GLint) * 3;
  const size_t kSourceSize = kHeaderSize + kString1Size + kString2Size;
  const size_t kPaddedHeaderSize =
      transfer_buffer_->RoundToAlignment(kHeaderSize);
  const size_t kPaddedString1Size =
      transfer_buffer_->RoundToAlignment(kString1Size);
  const size_t kPaddedString2Size =
      transfer_buffer_->RoundToAlignment(kString2Size);
  struct Cmds {
    cmd::SetBucketSize set_bucket_size;
    cmd::SetBucketData set_bucket_header;
    cmd::SetToken set_token1;
    cmd::SetBucketData set_bucket_data1;
    cmd::SetToken set_token2;
    cmd::SetBucketData set_bucket_data2;
    cmd::SetToken set_token3;
    cmds::%(name)sBucket cmd_bucket;
    cmd::SetBucketSize clear_bucket_size;
  };

  ExpectedMemoryInfo mem0 = GetExpectedMemory(kPaddedHeaderSize);
  ExpectedMemoryInfo mem1 = GetExpectedMemory(kPaddedString1Size);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kPaddedString2Size);

  Cmds expected;
  expected.set_bucket_size.Init(kBucketId, kSourceSize);
  expected.set_bucket_header.Init(
      kBucketId, 0, kHeaderSize, mem0.id, mem0.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_data1.Init(
      kBucketId, kHeaderSize, kString1Size, mem1.id, mem1.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_bucket_data2.Init(
      kBucketId, kHeaderSize + kString1Size, kString2Size, mem2.id,
      mem2.offset);
  expected.set_token3.Init(GetNextToken());
  expected.cmd_bucket.Init(%(bucket_args)s);
  expected.clear_bucket_size.Init(kBucketId, 0);
  const char* kStrings[] = { kString1, kString2 };
  gl_->%(name)s(%(gl_args)s);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
"""
    gl_args = []
    bucket_args = []
    for arg in func.GetOriginalArgs():
      if arg == self.__GetDataArg(func):
        gl_args.append('kStrings')
        bucket_args.append('kBucketId')
      elif arg == self.__GetLengthArg(func):
        gl_args.append('nullptr')
      elif arg.name == 'count':
        gl_args.append('2')
      else:
        gl_args.append(arg.GetValidClientSideArg(func))
        bucket_args.append(arg.GetValidClientSideArg(func))
    f.write(code % {
        'prefix' : _prefix,
        'name': func.name,
        'gl_args': ", ".join(gl_args),
        'bucket_args': ", ".join(bucket_args),
      })

    if self.__GetLengthArg(func) == None:
      return
    code = """
TEST_F(%(prefix)sImplementationTest, %(name)sWithLength) {
  const uint32_t kBucketId = %(prefix)sImplementation::kResultBucketId;
  const char* kString = "foobar******";
  const size_t kStringSize = 6;  // We only need "foobar".
  const size_t kHeaderSize = sizeof(GLint) * 2;
  const size_t kSourceSize = kHeaderSize + kStringSize + 1;
  const size_t kPaddedHeaderSize =
      transfer_buffer_->RoundToAlignment(kHeaderSize);
  const size_t kPaddedStringSize =
      transfer_buffer_->RoundToAlignment(kStringSize + 1);
  struct Cmds {
    cmd::SetBucketSize set_bucket_size;
    cmd::SetBucketData set_bucket_header;
    cmd::SetToken set_token1;
    cmd::SetBucketData set_bucket_data;
    cmd::SetToken set_token2;
    cmds::ShaderSourceBucket shader_source_bucket;
    cmd::SetBucketSize clear_bucket_size;
  };

  ExpectedMemoryInfo mem0 = GetExpectedMemory(kPaddedHeaderSize);
  ExpectedMemoryInfo mem1 = GetExpectedMemory(kPaddedStringSize);

  Cmds expected;
  expected.set_bucket_size.Init(kBucketId, kSourceSize);
  expected.set_bucket_header.Init(
      kBucketId, 0, kHeaderSize, mem0.id, mem0.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_data.Init(
      kBucketId, kHeaderSize, kStringSize + 1, mem1.id, mem1.offset);
  expected.set_token2.Init(GetNextToken());
  expected.shader_source_bucket.Init(%(bucket_args)s);
  expected.clear_bucket_size.Init(kBucketId, 0);
  const char* kStrings[] = { kString };
  const GLint kLength[] = { kStringSize };
  gl_->%(name)s(%(gl_args)s);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
"""
    gl_args = []
    for arg in func.GetOriginalArgs():
      if arg == self.__GetDataArg(func):
        gl_args.append('kStrings')
      elif arg == self.__GetLengthArg(func):
        gl_args.append('kLength')
      elif arg.name == 'count':
        gl_args.append('1')
      else:
        gl_args.append(arg.GetValidClientSideArg(func))
    f.write(code % {
        'prefix' : _prefix,
        'name': func.name,
        'gl_args': ", ".join(gl_args),
        'bucket_args': ", ".join(bucket_args),
      })

  def WriteBucketServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""
    cmd_args = []
    cmd_args_with_invalid_id = []
    gl_args = []
    for index, arg in enumerate(func.GetOriginalArgs()):
      if arg == self.__GetLengthArg(func):
        gl_args.append('_')
      elif arg.name == 'count':
        gl_args.append('1')
      elif arg == self.__GetDataArg(func):
        cmd_args.append('kBucketId')
        cmd_args_with_invalid_id.append('kBucketId')
        gl_args.append('_')
      elif index == 0:  # Resource ID arg
        cmd_args.append(arg.GetValidArg(func))
        cmd_args_with_invalid_id.append('kInvalidClientId')
        gl_args.append(arg.GetValidGLArg(func))
      else:
        cmd_args.append(arg.GetValidArg(func))
        cmd_args_with_invalid_id.append(arg.GetValidArg(func))
        gl_args.append(arg.GetValidGLArg(func))

    test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = { kSource0 };
  const char kValidStrEnd = 0;
  SetBucketAsCStrings(kBucketId, 1, kSource, 1, kValidStrEnd);
  cmds::%(name)s cmd;
  cmd.Init(%(cmd_args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));"""
    test += """
}
"""
    self.WriteValidUnitTest(func, f, test, {
        'cmd_args': ", ".join(cmd_args),
        'gl_args': ", ".join(gl_args),
      }, *extras)

    test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = { kSource0 };
  const char kValidStrEnd = 0;
  cmds::%(name)s cmd;
  // Test no bucket.
  cmd.Init(%(cmd_args)s);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  // Test invalid client.
  SetBucketAsCStrings(kBucketId, 1, kSource, 1, kValidStrEnd);
  cmd.Init(%(cmd_args_with_invalid_id)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}
"""
    self.WriteValidUnitTest(func, f, test, {
        'cmd_args': ", ".join(cmd_args),
        'cmd_args_with_invalid_id': ", ".join(cmd_args_with_invalid_id),
      }, *extras)

    test = """
TEST_P(%(test_name)s, %(name)sInvalidHeader) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = { kSource0 };
  const char kValidStrEnd = 0;
  const GLsizei kCount = static_cast<GLsizei>(std::size(kSource));
  const GLsizei kTests[] = {
      kCount + 1,
      0,
      std::numeric_limits<GLsizei>::max(),
      -1,
  };
  for (size_t ii = 0; ii < std::size(kTests); ++ii) {
    SetBucketAsCStrings(kBucketId, 1, kSource, kTests[ii], kValidStrEnd);
    cmds::%(name)s cmd;
    cmd.Init(%(cmd_args)s);
    EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  }
}
"""
    self.WriteValidUnitTest(func, f, test, {
        'cmd_args': ", ".join(cmd_args),
      }, *extras)

    test = """
TEST_P(%(test_name)s, %(name)sInvalidStringEnding) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = { kSource0 };
  const char kInvalidStrEnd = '*';
  SetBucketAsCStrings(kBucketId, 1, kSource, 1, kInvalidStrEnd);
  cmds::%(name)s cmd;
  cmd.Init(%(cmd_args)s);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}
"""
    self.WriteValidUnitTest(func, f, test, {
        'cmd_args': ", ".join(cmd_args),
      }, *extras)


class PUTXnHandler(ArrayArgTypeHandler):
  """Handler for glUniform?f functions."""

  def WriteHandlerImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    code = """  %(type)s temp[%(count)s] = { %(values)s};
  Do%(name)sv(%(location)s, 1, &temp[0]);
"""
    values = ""
    args = func.GetOriginalArgs()
    count = int(self.GetArrayCount(func))
    for ii in range(count):
      values += "%s, " % args[len(args) - count + ii].name

    f.write(code % {
        'name': func.name,
        'count': self.GetArrayCount(func),
        'type': self.GetArrayType(func),
        'location': args[0].name,
        'args': func.MakeOriginalArgString(""),
        'values': values,
      })

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(name)sv(%(local_args)s));
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    args = func.GetOriginalArgs()
    local_args = "%s, 1, _" % args[0].GetValidGLArg(func)
    self.WriteValidUnitTest(func, f, valid_test, {
        'name': func.name,
        'count': self.GetArrayCount(func),
        'local_args': local_args,
      }, *extras)

    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(name)sv(_, _, _).Times(0);
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, f, invalid_test, {
        'name': func.GetInfo('name'),
        'count': self.GetArrayCount(func),
      })


class GLcharHandler(CustomHandler):
  """Handler for functions that pass a single string ."""

  def WriteImmediateCmdComputeSize(self, _func, f):
    """Overrriden from TypeHandler."""
    f.write("  static uint32_t ComputeSize(uint32_t data_size) {\n")
    f.write("    return static_cast<uint32_t>(\n")
    f.write("        sizeof(ValueType) + data_size);  // NOLINT\n")
    f.write("  }\n")

  def WriteImmediateCmdSetHeader(self, _func, f):
    """Overrriden from TypeHandler."""
    code = """
  void SetHeader(uint32_t data_size) {
    header.SetCmdBySize<ValueType>(data_size);
  }
"""
    f.write(code)

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    args = func.GetCmdArgs()
    code = """
  void Init(%s, uint32_t _data_size) {
    SetHeader(_data_size);
"""
    f.write(code % func.MakeTypedArgString("_"))
    for arg in args:
      arg.WriteSetCode(f, 4, "_%s" % arg.name)
    code = """
    memcpy(ImmediateDataAddress(this), _%s, _data_size);
  }

"""
    f.write(code % last_arg.name)

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""
    f.write("  void* Set(void* cmd%s, uint32_t _data_size) {\n" %
               func.MakeTypedCmdArgString("_", True))
    f.write("    static_cast<ValueType*>(cmd)->Init(%s, _data_size);\n" %
               func.MakeCmdArgString("_"))
    f.write("    return NextImmediateCmdAddress<ValueType>("
               "cmd, _data_size);\n")
    f.write("  }\n")
    f.write("\n")

  def WriteImmediateCmdHelper(self, func, f):
    """Overrriden from TypeHandler."""
    code = """  void %(name)s(%(typed_args)s) {
    const uint32_t data_size = strlen(name);
    %(lp)s::cmds::%(name)s* c =
        GetImmediateCmdSpace<%(lp)s::cmds::%(name)s>(data_size);
    if (c) {
      c->Init(%(args)s, data_size);
    }
  }

"""
    f.write(code % {
          "lp" : _lower_prefix,
          "name": func.name,
          "typed_args": func.MakeTypedOriginalArgString(""),
          "args": func.MakeOriginalArgString(""),
        })


  def WriteImmediateFormatTest(self, func, f):
    """Overrriden from TypeHandler."""
    init_code = []
    check_code = []
    all_but_last_arg = func.GetCmdArgs()[:-1]
    for value, arg in enumerate(all_but_last_arg):
      init_code.append("      static_cast<%s>(%d)," % (arg.type, value + 11))
    for value, arg in enumerate(all_but_last_arg):
      check_code.append("  EXPECT_EQ(static_cast<%s>(%d), %s);" %
                        (arg.type, value + 11, arg.GetArgAccessor('cmd')))
    code = """
TEST_F(%(prefix)sFormatTest, %(func_name)s) {
  cmds::%(func_name)s& cmd = *GetBufferAs<cmds::%(func_name)s>();
  static const char* const test_str = \"test string\";
  void* next_cmd = cmd.Set(
      &cmd,
%(init_code)s
      test_str,
      strlen(test_str));
  EXPECT_EQ(static_cast<uint32_t>(cmds::%(func_name)s::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(strlen(test_str)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(strlen(test_str)));
%(check_code)s
  EXPECT_EQ(static_cast<uint32_t>(strlen(test_str)), cmd.data_size);
  EXPECT_EQ(0, memcmp(test_str, ImmediateDataAddress(&cmd), strlen(test_str)));
  CheckBytesWritten(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(strlen(test_str)),
      sizeof(cmd) + strlen(test_str));
}

"""
    f.write(code % {
          'prefix': _prefix,
          'func_name': func.name,
          'init_code': "\n".join(init_code),
          'check_code': "\n".join(check_code),
        })


class GLcharNHandler(CustomHandler):
  """Handler for functions that pass a single string with an optional len."""

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    func.cmd_args = []
    func.AddCmdArg(Argument('bucket_id', 'GLuint'))

  def NeedsDataTransferFunction(self, func):
    """Overriden from TypeHandler."""
    return False

  def WriteServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    self.WriteServiceHandlerFunctionHeader(func, f)
    if func.IsES31():
      return
    f.write("""
  GLuint bucket_id = static_cast<GLuint>(c.%(bucket_id)s);
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string str;
  if (!bucket->GetAsString(&str)) {
    return error::kInvalidArguments;
  }
  %(gl_func_name)s(0, str.c_str());
  return error::kNoError;
}

""" % {
    'gl_func_name': func.GetGLFunctionName(),
    'bucket_id': func.cmd_args[0].name,
  })


class IsHandler(TypeHandler):
  """Handler for glIs____ type and glGetError functions."""

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    func.AddCmdArg(Argument("result_shm_id", 'uint32_t'))
    func.AddCmdArg(Argument("result_shm_offset", 'uint32_t'))
    if func.GetInfo('result') == None:
      func.AddInfo('result', ['uint32_t'])
    func.passthrough_service_doer_args.append(Argument('result', 'uint32_t*'))

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<cmds::%(name)s, 0>(true);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s%(comma)sshared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    comma = ""
    if len(func.GetOriginalArgs()):
      comma =", "
    self.WriteValidUnitTest(func, f, valid_test, {
          'comma': comma,
        }, *extras)

    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s%(comma)sshared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));%(gl_error_test)s
}
"""
    self.WriteInvalidUnitTest(func, f, invalid_test, {
          'comma': comma,
        }, *extras)

    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgsBadSharedMemoryId) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<cmds::%(name)s, 0>(false);
  cmds::%(name)s cmd;
  cmd.Init(%(args)s%(comma)skInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(%(args)s%(comma)sshared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}
"""
    self.WriteValidUnitTest(func, f, invalid_test, {
          'comma': comma,
        }, *extras)

  def WriteServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    self.WriteServiceHandlerFunctionHeader(func, f)
    if func.IsES31():
      return
    self.WriteHandlerExtensionCheck(func, f)
    args = func.GetOriginalArgs()
    for arg in args:
      arg.WriteGetCode(f)

    code = """  typedef cmds::%(func_name)s::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
"""
    f.write(code % {'func_name': func.name})
    func.WriteHandlerValidation(f)
    f.write("  *result_dst = %s(%s);\n" %
            (func.GetGLFunctionName(), func.MakeOriginalArgString("")))
    f.write("  return error::kNoError;\n")
    f.write("}\n")
    f.write("\n")

  def WritePassthroughServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    self.WritePassthroughServiceFunctionHeader(func, f)
    self.WriteHandlerExtensionCheck(func, f)
    self.WriteServiceHandlerArgGetCode(func, f)

    code = """  typedef cmds::%(func_name)s::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
"""
    f.write(code % {'func_name': func.name})
    self.WritePassthroughServiceFunctionDoerCall(func, f)
    f.write("  return error::kNoError;\n")
    f.write("}\n")
    f.write("\n")

  def WriteGLES2Implementation(self, func, f):
    """Overrriden from TypeHandler."""
    impl_func = func.GetInfo('impl_func', True)
    if impl_func:
      error_value = func.GetInfo("error_value") or "GL_FALSE"
      f.write("%s %sImplementation::%s(%s) {\n" %
                 (func.return_type, _prefix, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      f.write("  GPU_CLIENT_SINGLE_THREAD_CHECK();\n")
      self.WriteTraceEvent(func, f)
      func.WriteDestinationInitalizationValidation(f)
      self.WriteClientGLCallLog(func, f)
      f.write("  typedef cmds::%s::Result Result;\n" % func.name)
      f.write("  ScopedResultPtr<Result> result = GetResultAs<Result>();\n")
      f.write("  if (!result) {\n")
      f.write("    return %s;\n" % error_value)
      f.write("  }\n")
      f.write("  *result = 0;\n")
      assert len(func.GetOriginalArgs()) == 1
      id_arg = func.GetOriginalArgs()[0]
      if id_arg.type == 'GLsync':
        arg_string = "ToGLuint(%s)" % func.MakeOriginalArgString("")
      else:
        arg_string = func.MakeOriginalArgString("")
      f.write(
          "  helper_->%s(%s, GetResultShmId(), result.offset());\n" %
              (func.name, arg_string))
      f.write("  if (!WaitForCmd()) {\n")
      f.write("    return %s; \n" % error_value)
      f.write("  }\n")
      f.write("  %s result_value = *result" % func.return_type)
      if func.return_type == "GLboolean":
        f.write(" != 0")
      f.write(';\n  GPU_CLIENT_LOG("returned " << result_value);\n')
      f.write("  CheckGLError();\n")
      f.write("  return result_value;\n")
      f.write("}\n")
      f.write("\n")

  def WriteGLES2ImplementationUnitTest(self, func, f):
    """Overrriden from TypeHandler."""
    client_test = func.GetInfo('client_test', True)
    if client_test:
      code = """
TEST_F(%(prefix)sImplementationTest, %(name)s) {
  struct Cmds {
    cmds::%(name)s cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::%(name)s::Result));
  expected.cmd.Init(%(cmd_id_value)s, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->%(name)s(%(gl_id_value)s);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}
"""
      args = func.GetOriginalArgs()
      assert len(args) == 1
      f.write(code % {
          'prefix' : _prefix,
          'name': func.name,
          'cmd_id_value': args[0].GetValidClientSideCmdArg(func),
          'gl_id_value': args[0].GetValidClientSideArg(func) })

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class STRnHandler(TypeHandler):
  """Handler for GetProgramInfoLog, GetShaderInfoLog, GetShaderSource, and
  GetTranslatedShaderSourceANGLE."""

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    # remove all but the first cmd args.
    cmd_args = func.GetCmdArgs()
    func.ClearCmdArgs()
    func.AddCmdArg(cmd_args[0])
    # add on a bucket id.
    func.AddCmdArg(Argument('bucket_id', 'uint32_t'))

  def WriteGLES2Implementation(self, func, f):
    """Overrriden from TypeHandler."""
    code_1 = """%(return_type)s %(prefix)sImplementation::%(func_name)s(
    %(args)s) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
"""
    code_2 = """  GPU_CLIENT_LOG("[" << GetLogPrefix()
      << "] gl%(func_name)s" << "("
      << %(arg0)s << ", "
      << %(arg1)s << ", "
      << static_cast<void*>(%(arg2)s) << ", "
      << static_cast<void*>(%(arg3)s) << ")");
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->%(func_name)s(%(id_name)s, kResultBucketId);
  std::string str;
  GLsizei max_size = 0;
  if (GetBucketAsString(kResultBucketId, &str)) {
    if (bufsize > 0) {
      max_size =
          std::min(static_cast<size_t>(%(bufsize_name)s) - 1, str.size());
      memcpy(%(dest_name)s, str.c_str(), max_size);
      %(dest_name)s[max_size] = '\\0';
      GPU_CLIENT_LOG("------\\n" << %(dest_name)s << "\\n------");
    }
  }
  if (%(length_name)s != nullptr) {
    *%(length_name)s = max_size;
  }
  CheckGLError();
}
"""
    args = func.GetOriginalArgs()
    str_args = {
      'prefix' : _prefix,
      'return_type': func.return_type,
      'func_name': func.original_name,
      'args': func.MakeTypedOriginalArgString(""),
      'id_name': args[0].name,
      'bufsize_name': args[1].name,
      'length_name': args[2].name,
      'dest_name': args[3].name,
      'arg0': args[0].name,
      'arg1': args[1].name,
      'arg2': args[2].name,
      'arg3': args[3].name,
    }
    f.write(code_1 % str_args)
    func.WriteDestinationInitalizationValidation(f)
    f.write(code_2 % str_args)

  def WriteServiceUnitTest(self, func, f, *extras):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_P(%(test_name)s, %(name)sValidArgs) {
  const char* kInfo = "hello";
  const uint32_t kBucketId = 123;
  SpecializedSetup<cmds::%(name)s, 0>(true);
%(expect_len_code)s
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s))
      .WillOnce(DoAll(SetArgPointee<2>(strlen(kInfo)),
                      SetArrayArgument<3>(kInfo, kInfo + strlen(kInfo) + 1)));
  cmds::%(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(strlen(kInfo) + 1, bucket->size());
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kInfo,
                      bucket->size()));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
"""
    args = func.GetOriginalArgs()
    id_name = args[0].GetValidGLArg(func)
    get_len_func = func.GetInfo('get_len_func')
    get_len_enum = func.GetInfo('get_len_enum')
    sub = {
        'id_name': id_name,
        'get_len_func': get_len_func,
        'get_len_enum': get_len_enum,
        'gl_args': '%s, strlen(kInfo) + 1, _, _' %
             args[0].GetValidGLArg(func),
        'args': '%s, kBucketId' % args[0].GetValidArg(func),
        'expect_len_code': '',
    }
    if get_len_func and get_len_func[0:2] == 'gl':
      sub['expect_len_code'] = (
        "  EXPECT_CALL(*gl_, %s(%s, %s, _))\n"
        "      .WillOnce(SetArgPointee<2>(strlen(kInfo) + 1));") % (
            get_len_func[2:], id_name, get_len_enum)
    self.WriteValidUnitTest(func, f, valid_test, sub, *extras)

    invalid_test = """
TEST_P(%(test_name)s, %(name)sInvalidArgs) {
  const uint32_t kBucketId = 123;
  EXPECT_CALL(*gl_, %(gl_func_name)s(_, _, _, _))
      .Times(0);
  cmds::%(name)s cmd;
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}
"""
    self.WriteValidUnitTest(func, f, invalid_test, *extras)

  def WriteServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""
    if func.IsES31():
      TypeHandler.WriteServiceImplementation(self, func, f)

  def WritePassthroughServiceImplementation(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdInit(self, func, f):
    """Overrriden from TypeHandler."""

  def WriteImmediateCmdSet(self, func, f):
    """Overrriden from TypeHandler."""


class NamedType():
  """A class that represents a type of an argument in a client function.

  A type of an argument that is to be passed through in the command buffer
  command. Currently used only for the arguments that are specificly named in
  the 'cmd_buffer_functions.txt' f, mostly enums.
  """

  def __init__(self, info):
    assert not 'is_complete' in info or info['is_complete'] == True
    self.info = info
    self.valid = info['valid']
    if 'invalid' in info:
      self.invalid = info['invalid']
    else:
      self.invalid = []
    if 'valid_es3' in info:
      self.valid_es3 = info['valid_es3']
    else:
      self.valid_es3 = []
    if 'deprecated_es3' in info:
      self.deprecated_es3 = info['deprecated_es3']
    else:
      self.deprecated_es3 = []
    self.create_validator = info.get('validator', True)
    self.is_complete = info.get('is_complete', False)

  def GetType(self):
    return self.info['type']

  def GetInvalidValues(self):
    return self.invalid

  def GetValidValues(self):
    return self.valid

  def GetValidValuesES3(self):
    return self.valid_es3

  def GetDeprecatedValuesES3(self):
    return self.deprecated_es3

  def HasES3Values(self):
    return self.valid_es3 or self.deprecated_es3

  def IsConstant(self):
    return self.is_complete and len(self.GetValidValues()) == 1

  def IsComplete(self):
    return self.is_complete

  def CreateValidator(self):
    return self.create_validator and not self.IsConstant()

  def GetConstantValue(self):
    return self.GetValidValues()[0]

class Argument():
  """A class that represents a function argument."""

  cmd_type_map_ = {
    'GLenum': ['uint32_t'],
    'GLint': ['int32_t'],
    'GLintptr': ['int32_t'],
    'GLsizei': ['int32_t'],
    'GLsizeiptr': ['int32_t'],
    'GLfloat': ['float'],
    'GLclampf': ['float'],
    'GLuint64': ['uint32_t', 'uint32_t'],
  }
  need_validation_ = ['GLsizei*', 'GLboolean*', 'GLenum*', 'GLint*']

  def __init__(self, name, arg_type, arg_default = None):
    self.name = name
    self.optional = arg_type.endswith("Optional*")
    if self.optional:
      arg_type = arg_type[:-len("Optional*")] + "*"
    self.type = arg_type
    self.default = arg_default

    if arg_type in self.cmd_type_map_:
      self.cmd_type = self.cmd_type_map_[arg_type]
    else:
      self.cmd_type = ['uint32_t']

  def IsPointer(self):
    """Returns true if argument is a pointer."""
    return False

  def IsPointer2D(self):
    """Returns true if argument is a 2D pointer."""
    return False

  def IsConstant(self):
    """Returns true if the argument has only one valid value."""
    return False

  def AddCmdArgs(self, args):
    """Adds command arguments for this argument to the given list."""
    if not self.IsConstant():
      args.append(self)

  def AddInitArgs(self, args):
    """Adds init arguments for this argument to the given list."""
    if not self.IsConstant():
      args.append(self)

  def GetValidArg(self, func):
    """Gets a valid value for this argument."""
    valid_arg = func.GetValidArg(self)
    if valid_arg != None:
      return valid_arg

    index = func.GetOriginalArgs().index(self)
    return str(index + 1)

  def GetArgDecls(self):
    if len(self.cmd_type) == 1:
      return [(self.cmd_type[0], self.name)]
    return [(cmd_type, self.name + '_%d' % i)
            for i, cmd_type
            in enumerate(self.cmd_type)]

  def GetReservedSizeId(self):
    """Gets a special identifier name for the data size of this argument"""
    return "%s_size" % self.name

  def GetValidClientSideArg(self, func):
    """Gets a valid value for this argument."""
    valid_arg = func.GetValidArg(self)
    if valid_arg != None:
      return valid_arg

    if self.IsPointer():
      return 'nullptr'
    index = func.GetOriginalArgs().index(self)
    if self.type == 'GLsync':
      return ("reinterpret_cast<GLsync>(%d)" % (index + 1))
    return str(index + 1)

  def GetValidClientSideCmdArg(self, func):
    """Gets a valid value for this argument."""
    valid_arg = func.GetValidArg(self)
    if valid_arg != None:
      return valid_arg
    try:
      index = func.GetOriginalArgs().index(self)
      return str(index + 1)
    except ValueError:
      pass
    index = func.GetCmdArgs().index(self)
    return str(index + 1)

  def GetValidGLArg(self, func):
    """Gets a valid GL value for this argument."""
    value = self.GetValidArg(func)
    if self.type == 'GLsync':
      return ("reinterpret_cast<GLsync>(%s)" % value)
    return value

  def GetValidNonCachedClientSideArg(self, _func):
    """Returns a valid value for this argument in a GL call.
    Using the value will produce a command buffer service invocation.
    Returns None if there is no such value."""
    value = '123'
    if self.type == 'GLsync':
      return ("reinterpret_cast<GLsync>(%s)" % value)
    return value

  def GetValidNonCachedClientSideCmdArg(self, _func):
    """Returns a valid value for this argument in a command buffer command.
    Calling the GL function with the value returned by
    GetValidNonCachedClientSideArg will result in a command buffer command
    that contains the value returned by this function. """
    return '123'

  def GetNumInvalidValues(self, _func):
    """returns the number of invalid values to be tested."""
    return 0

  def GetInvalidArg(self, _index):
    """returns an invalid value and expected parse result by index."""
    return ("---ERROR0---", "---ERROR2---", None)

  def GetArgAccessor(self, cmd_struct_name):
    """Returns the name of the accessor for the argument within the struct."""
    return '%s.%s' % (cmd_struct_name, self.name)

  def GetLogArg(self):
    """Get argument appropriate for LOG macro."""
    if self.type == 'GLboolean':
      return '%sGLES2Util::GetStringBool(%s)' % (_Namespace(), self.name)
    if self.type == 'GLenum':
      return '%sGLES2Util::GetStringEnum(%s)' % (_Namespace(), self.name)
    return self.name

  def WriteGetCode(self, f):
    """Writes the code to get an argument from a command structure."""
    if self.type == 'GLsync':
      my_type = 'GLuint'
    else:
      my_type = self.type
    f.write("  %s %s = static_cast<%s>(c.%s);\n" %
               (my_type, self.name, my_type, self.name))

  def WriteSetCode(self, f, indent, var):
    f.write("%s%s = %s;\n" % (' ' * indent, self.name, var))

  def WriteArgAccessor(self, f):
    """Writes specialized accessor for argument."""

  def WriteValidationCode(self, f, func):
    """Writes the validation code for an argument."""

  def WritePassthroughValidationCode(self, f, func):
    """Writes the passthrough validation code for an argument."""

  def WriteClientSideValidationCode(self, f, func):
    """Writes the validation code for an argument."""

  def WriteDestinationInitalizationValidation(self, f, func):
    """Writes the client side destintion initialization validation."""

  def WriteDestinationInitalizationValidatationIfNeeded(self, f, _func):
    """Writes the client side destintion initialization validation if needed."""
    parts = self.type.split(" ")
    if len(parts) > 1:
      return
    if parts[0] in self.need_validation_:
      f.write(
          "  GPU_CLIENT_VALIDATE_DESTINATION_%sINITALIZATION(%s, %s);\n" %
          ("OPTIONAL_" if self.optional else "", self.type[:-1], self.name))

  def GetImmediateVersion(self):
    """Gets the immediate version of this argument."""
    return self

  def GetBucketVersion(self):
    """Gets the bucket version of this argument."""
    return self


class BoolArgument(Argument):
  """class for C++ bool"""

  def __init__(self, name, _type, arg_default):
    Argument.__init__(self, name, _type, arg_default)

  def GetValidArg(self, func):
    """Gets a valid value for this argument."""
    return 'true'

  def GetValidClientSideArg(self, func):
    """Gets a valid value for this argument."""
    return 'true'

  def GetValidClientSideCmdArg(self, func):
    """Gets a valid value for this argument."""
    return 'true'

  def GetValidGLArg(self, func):
    """Gets a valid GL value for this argument."""
    return 'true'

  def GetArgAccessor(self, cmd_struct_name):
    """Returns the name of the accessor for the argument within the struct."""
    return 'static_cast<bool>(%s.%s)' % (struct_name, self.name)


class GLBooleanArgument(Argument):
  """class for GLboolean"""

  def __init__(self, name, _type, arg_default):
    Argument.__init__(self, name, 'GLboolean', arg_default)

  def GetValidArg(self, func):
    """Gets a valid value for this argument."""
    return 'true'

  def GetValidClientSideArg(self, func):
    """Gets a valid value for this argument."""
    return 'true'

  def GetValidClientSideCmdArg(self, func):
    """Gets a valid value for this argument."""
    return 'true'

  def GetValidGLArg(self, func):
    """Gets a valid GL value for this argument."""
    return 'true'


class UniformLocationArgument(Argument):
  """class for uniform locations."""

  def __init__(self, name, arg_default):
    Argument.__init__(self, name, "GLint", arg_default)

  def WriteGetCode(self, f):
    """Writes the code to get an argument from a command structure."""
    code = """  %s %s = static_cast<%s>(c.%s);
"""
    f.write(code % (self.type, self.name, self.type, self.name))

class DataSizeArgument(Argument):
  """class for data_size which Bucket commands do not need."""

  def __init__(self, name):
    Argument.__init__(self, name, "uint32_t")

  def GetBucketVersion(self):
    return None


class SizeArgument(Argument):
  """class for GLsizei and GLsizeiptr."""

  def GetNumInvalidValues(self, func):
    """overridden from Argument."""
    if func.IsImmediate():
      return 0
    return 1

  def GetInvalidArg(self, _index):
    """overridden from Argument."""
    return ("-1", "kNoError", "GL_INVALID_VALUE")

  def WriteValidationCode(self, f, func):
    """overridden from Argument."""
    code = """  if (%(var_name)s < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "gl%(func_name)s", "%(var_name)s < 0");
    return error::kNoError;
  }
"""
    f.write(code % {
        "var_name": self.name,
        "func_name": func.original_name,
      })

  def WriteClientSideValidationCode(self, f, func):
    """overridden from Argument."""
    code = """  if (%(var_name)s < 0) {
    SetGLError(GL_INVALID_VALUE, "gl%(func_name)s", "%(var_name)s < 0");
    return;
  }
"""
    f.write(code % {
        "var_name": self.name,
        "func_name": func.original_name,
      })


class SizeNotNegativeArgument(SizeArgument):
  """class for GLsizeiNotNegative. It's NEVER allowed to be negative"""

  def GetInvalidArg(self, _index):
    """overridden from SizeArgument."""
    return ("-1", "kOutOfBounds", "GL_NO_ERROR")

  def WriteValidationCode(self, f, func):
    """overridden from SizeArgument."""


class EnumBaseArgument(Argument):
  """Base class for EnumArgument, IntArgument, and BitfieldArgument."""

  def __init__(self, name, gl_type, type_name, arg_type, gl_error,
               named_type_info, arg_default):
    Argument.__init__(self, name, gl_type, arg_default)

    self.gl_error = gl_error
    self.type_name = type_name
    self.named_type = NamedType(named_type_info[type_name])

  def IsConstant(self):
    return self.named_type.IsConstant()

  def GetConstantValue(self):
    return self.named_type.GetConstantValue()

  def WriteValidationCode(self, f, func):
    if self.named_type.IsConstant():
      return
    f.write("  if (!validators_->%s.IsValid(%s)) {\n" %
               (ToUnderscore(self.type_name), self.name))
    if self.gl_error == "GL_INVALID_ENUM":
      f.write(
          "    LOCAL_SET_GL_ERROR_INVALID_ENUM(\"gl%s\", %s, \"%s\");\n" %
          (func.original_name, self.name, self.name))
    else:
      f.write(
          "    LOCAL_SET_GL_ERROR(%s, \"gl%s\", \"%s %s\");\n" %
          (self.gl_error, func.original_name, self.name, self.gl_error))
    f.write("    return error::kNoError;\n")
    f.write("  }\n")

  def WriteClientSideValidationCode(self, f, func):
    if not self.named_type.IsConstant():
      return
    f.write("  if (%s != %s) {" % (self.name,
                                      self.GetConstantValue()))
    f.write(
      "    SetGLError(%s, \"gl%s\", \"%s %s\");\n" %
      (self.gl_error, func.original_name, self.name, self.gl_error))
    if func.return_type == "void":
      f.write("    return;\n")
    else:
      f.write("    return %s;\n" % func.GetErrorReturnString())
    f.write("  }\n")

  def GetValidArg(self, func):
    valid_arg = func.GetValidArg(self)
    if valid_arg != None:
      return valid_arg
    valid = self.named_type.GetValidValues()
    if valid:
      return valid[0]

    index = func.GetOriginalArgs().index(self)
    return str(index + 1)

  def GetValidClientSideArg(self, func):
    """Gets a valid value for this argument."""
    return self.GetValidArg(func)

  def GetValidClientSideCmdArg(self, func):
    """Gets a valid value for this argument."""
    valid_arg = func.GetValidArg(self)
    if valid_arg != None:
      return valid_arg

    valid = self.named_type.GetValidValues()
    if valid:
      return valid[0]

    try:
      index = func.GetOriginalArgs().index(self)
      return str(index + 1)
    except ValueError:
      pass
    index = func.GetCmdArgs().index(self)
    return str(index + 1)

  def GetValidGLArg(self, func):
    """Gets a valid value for this argument."""
    return self.GetValidArg(func)

  def GetNumInvalidValues(self, _func):
    """returns the number of invalid values to be tested."""
    return len(self.named_type.GetInvalidValues())

  def GetInvalidArg(self, index):
    """returns an invalid value by index."""
    invalid = self.named_type.GetInvalidValues()
    if invalid:
      num_invalid = len(invalid)
      if index >= num_invalid:
        index = num_invalid - 1
      return (invalid[index], "kNoError", self.gl_error)
    return ("---ERROR1---", "kNoError", self.gl_error)


class EnumArgument(EnumBaseArgument):
  """A class that represents a GLenum argument"""

  def __init__(self, name, arg_type, named_type_info, arg_default):
    EnumBaseArgument.__init__(self, name, "GLenum", arg_type[len("GLenum"):],
                              arg_type, "GL_INVALID_ENUM", named_type_info,
                              arg_default)

  def GetLogArg(self):
    """Overridden from Argument."""
    return ("GLES2Util::GetString%s(%s)" %
            (self.type_name, self.name))


class EnumClassArgument(EnumBaseArgument):
  """A class that represents a C++ enum argument encoded as uint32_t"""

  def __init__(self, name, arg_type, named_type_info, arg_default):
    type_name = arg_type[len("EnumClass"):]
    EnumBaseArgument.__init__(self, name, type_name, type_name, arg_type,
                              "GL_INVALID_ENUM", named_type_info, arg_default)

  def GetArgAccessor(self, cmd_struct_name):
    """Returns the name of the accessor for the argument within the struct."""
    return 'static_cast<%s>(%s.%s)' % (self.type_name, struct_name, self.name)

  def WriteSetCode(self, f, indent, var):
    f.write("%s%s = static_cast<uint32_t>(%s);\n" %
            (' ' * indent, self.name, var))

  def GetLogArg(self):
    return 'static_cast<uint32_t>(%s)' % self.name


class IntArgument(EnumBaseArgument):
  """A class for a GLint argument that can only accept specific values.

  For example glTexImage2D takes a GLint for its internalformat
  argument instead of a GLenum.
  """

  def __init__(self, name, arg_type, named_type_info, arg_default):
    EnumBaseArgument.__init__(self, name, "GLint", arg_type[len("GLint"):],
                              arg_type, "GL_INVALID_VALUE", named_type_info,
                              arg_default)


class BitFieldArgument(EnumBaseArgument):
  """A class for a GLbitfield argument that can only accept specific values.

  For example glFenceSync takes a GLbitfield for its flags argument bit it
  must be 0.
  """

  def __init__(self, name, arg_type, named_type_info, arg_default):
    EnumBaseArgument.__init__(self, name, "GLbitfield",
                              arg_type[len("GLbitfield"):], arg_type,
                              "GL_INVALID_VALUE", named_type_info, arg_default)


class ImmediatePointerArgument(Argument):
  """A class that represents an immediate argument to a function.

  An immediate argument is one where the data follows the command.
  """

  def IsPointer(self):
    return True

  def GetPointedType(self):
    match = re.match('(const\s+)?(?P<element_type>[\w]+)\s*\*', self.type)
    assert match
    return match.groupdict()['element_type']

  def AddCmdArgs(self, args):
    """Overridden from Argument."""

  def WriteGetCode(self, f):
    """Overridden from Argument."""
    f.write("  volatile %s %s = %sGetImmediateDataAs<volatile %s>(\n" %
            (self.type, self.name, _Namespace(), self.type))
    f.write("      c, %s, immediate_data_size);\n" %
            self.GetReservedSizeId())

  def WriteValidationCode(self, f, func):
    """Overridden from Argument."""
    if self.optional:
      return
    f.write("  if (%s == nullptr) {\n" % self.name)
    f.write("    return error::kOutOfBounds;\n")
    f.write("  }\n")

  def WritePassthroughValidationCode(self, f, func):
    """Overridden from Argument."""
    if self.optional:
      return
    f.write("  if (%s == nullptr) {\n" % self.name)
    f.write("    return error::kOutOfBounds;\n")
    f.write("  }\n")

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return None

  def WriteDestinationInitalizationValidation(self, f, func):
    """Overridden from Argument."""
    self.WriteDestinationInitalizationValidatationIfNeeded(f, func)

  def GetLogArg(self):
    """Overridden from Argument."""
    return "static_cast<const void*>(%s)" % self.name


class PointerArgument(Argument):
  """A class that represents a pointer argument to a function."""

  def IsPointer(self):
    """Overridden from Argument."""
    return True

  def IsPointer2D(self):
    """Overridden from Argument."""
    return self.type.count('*') == 2

  def GetPointedType(self):
    match = re.match('(const\s+)?(?P<element_type>[\w]+)\s*\*', self.type)
    assert match
    return match.groupdict()['element_type']

  def GetValidArg(self, func):
    """Overridden from Argument."""
    return "shared_memory_id_, shared_memory_offset_"

  def GetValidGLArg(self, func):
    """Overridden from Argument."""
    return "reinterpret_cast<%s>(shared_memory_address_)" % self.type

  def GetNumInvalidValues(self, _func):
    """Overridden from Argument."""
    return 2

  def GetInvalidArg(self, index):
    """Overridden from Argument."""
    if index == 0:
      return ("kInvalidSharedMemoryId, 0", "kOutOfBounds", None)
    return ("shared_memory_id_, kInvalidSharedMemoryOffset",
            "kOutOfBounds", None)

  def GetLogArg(self):
    """Overridden from Argument."""
    return "static_cast<const void*>(%s)" % self.name

  def AddCmdArgs(self, args):
    """Overridden from Argument."""
    args.append(Argument("%s_shm_id" % self.name, 'uint32_t'))
    args.append(Argument("%s_shm_offset" % self.name, 'uint32_t'))

  def WriteGetCode(self, f):
    """Overridden from Argument."""
    f.write(
        "  %s %s = GetSharedMemoryAs<%s>(\n" %
        (self.type, self.name, self.type))
    f.write(
        "      c.%s_shm_id, c.%s_shm_offset, %s);\n" %
        (self.name, self.name, self.GetReservedSizeId()))

  def WriteValidationCode(self, f, func):
    """Overridden from Argument."""
    if self.optional:
      return
    f.write("  if (%s == nullptr) {\n" % self.name)
    f.write("    return error::kOutOfBounds;\n")
    f.write("  }\n")

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return ImmediatePointerArgument(self.name, self.type)

  def GetBucketVersion(self):
    """Overridden from Argument."""
    if self.type.find('char') >= 0:
      if self.IsPointer2D():
        return InputStringArrayBucketArgument(self.name, self.type)
      return InputStringBucketArgument(self.name, self.type)
    return BucketPointerArgument(self.name, self.type)

  def WriteDestinationInitalizationValidation(self, f, func):
    """Overridden from Argument."""
    self.WriteDestinationInitalizationValidatationIfNeeded(f, func)


class BucketPointerArgument(PointerArgument):
  """A class that represents an bucket argument to a function."""

  def AddCmdArgs(self, args):
    """Overridden from Argument."""

  def WriteGetCode(self, f):
    """Overridden from Argument."""
    f.write(
      "  %s %s = bucket->GetData(0, %s);\n" %
      (self.type, self.name, self.GetReservedSizeId()))

  def WriteValidationCode(self, f, func):
    """Overridden from Argument."""

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return None

  def WriteDestinationInitalizationValidation(self, f, func):
    """Overridden from Argument."""
    self.WriteDestinationInitalizationValidatationIfNeeded(f, func)

  def GetLogArg(self):
    """Overridden from Argument."""
    return "static_cast<const void*>(%s)" % self.name


class InputStringBucketArgument(Argument):
  """A string input argument where the string is passed in a bucket."""

  def __init__(self, name, _type):
    Argument.__init__(self, name + "_bucket_id", "uint32_t")

  def IsPointer(self):
    """Overridden from Argument."""
    return True

  def IsPointer2D(self):
    """Overridden from Argument."""
    return False


class InputStringArrayBucketArgument(Argument):
  """A string array input argument where the strings are passed in a bucket."""

  def __init__(self, name, _type):
    Argument.__init__(self, name + "_bucket_id", "uint32_t")
    self._original_name = name

  def WriteGetCode(self, f):
    """Overridden from Argument."""
    code = """
  Bucket* bucket = GetBucket(c.%(name)s);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  std::vector<char*> strs;
  std::vector<GLint> len;
  if (!bucket->GetAsStrings(&count, &strs, &len)) {
    return error::kInvalidArguments;
  }
  const char** %(original_name)s =
      strs.size() > 0 ? const_cast<const char**>(&strs[0]) : nullptr;
  const GLint* length =
      len.size() > 0 ? const_cast<const GLint*>(&len[0]) : nullptr;
  (void)length;
"""
    f.write(code % {
        'name': self.name,
        'original_name': self._original_name,
      })

  def GetValidArg(self, func):
    return "kNameBucketId"

  def GetValidGLArg(self, func):
    return "_"

  def IsPointer(self):
    """Overridden from Argument."""
    return True

  def IsPointer2D(self):
    """Overridden from Argument."""
    return True


class ResourceIdArgument(Argument):
  """A class that represents a resource id argument to a function."""

  def __init__(self, name, arg_type, arg_default):
    match = re.match("(GLid\w+)", arg_type)
    self.resource_type = match.group(1)[4:]
    if self.resource_type == "Sync":
      arg_type = arg_type.replace(match.group(1), "GLsync")
    else:
      arg_type = arg_type.replace(match.group(1), "GLuint")
    Argument.__init__(self, name, arg_type, arg_default)

  def WriteGetCode(self, f):
    """Overridden from Argument."""
    if self.type == "GLsync":
      my_type = "GLuint"
    else:
      my_type = self.type
    f.write("  %s %s = %s;\n" % (my_type, self.name, self.GetArgAccessor('c')))

  def GetValidArg(self, func):
    return "client_%s_id_" % self.resource_type.lower()

  def GetValidGLArg(self, func):
    if self.resource_type == "Sync":
      return "reinterpret_cast<GLsync>(kService%sId)" % self.resource_type
    return "kService%sId" % self.resource_type


class ResourceIdBindArgument(Argument):
  """Represents a resource id argument to a bind function."""

  def __init__(self, name, arg_type, arg_default):
    match = re.match("(GLidBind\w+)", arg_type)
    self.resource_type = match.group(1)[8:]
    arg_type = arg_type.replace(match.group(1), "GLuint")
    Argument.__init__(self, name, arg_type, arg_default)

  def WriteGetCode(self, f):
    """Overridden from Argument."""
    code = """  %(type)s %(name)s = c.%(name)s;
"""
    f.write(code % {'type': self.type, 'name': self.name})

  def GetValidArg(self, func):
    return "client_%s_id_" % self.resource_type.lower()

  def GetValidGLArg(self, func):
    return "kService%sId" % self.resource_type


class ResourceIdZeroArgument(Argument):
  """Represents a resource id argument to a function that can be zero."""

  def __init__(self, name, arg_type, arg_default):
    match = re.match("(GLidZero\w+)", arg_type)
    self.resource_type = match.group(1)[8:]
    arg_type = arg_type.replace(match.group(1), "GLuint")
    Argument.__init__(self, name, arg_type, arg_default)

  def WriteGetCode(self, f):
    """Overridden from Argument."""
    f.write("  %s %s = %s;\n" % (self.type, self.name,
                                 self.GetArgAccessor('c')))

  def GetValidArg(self, func):
    return "client_%s_id_" % self.resource_type.lower()

  def GetValidGLArg(self, func):
    return "kService%sId" % self.resource_type

  def GetNumInvalidValues(self, _func):
    """returns the number of invalid values to be tested."""
    return 1

  def GetInvalidArg(self, _index):
    """returns an invalid value by index."""
    return ("kInvalidClientId", "kNoError", "GL_INVALID_VALUE")


class Int64Argument(Argument):
  """Represents a GLuint64 argument which splits up into 2 uint32_t items."""

  def __init__(self, name, arg_type, arg_default):
    Argument.__init__(self, name, arg_type, arg_default)

  def GetArgAccessor(self, cmd_struct_name):
    return "%s.%s()" % (cmd_struct_name, self.name)

  def WriteArgAccessor(self, f):
    """Writes specialized accessor for compound members."""
    f.write("  %s %s() const volatile {\n" % (self.type, self.name))
    f.write("    return static_cast<%s>(\n" % self.type)
    f.write("        %sGLES2Util::MapTwoUint32ToUint64(\n" % _Namespace())
    f.write("            %s_0,\n" % self.name)
    f.write("            %s_1));\n" % self.name)
    f.write("  }\n")
    f.write("\n")

  def WriteGetCode(self, f):
    """Writes the code to get an argument from a command structure."""
    f.write("  %s %s = c.%s();\n" % (self.type, self.name, self.name))

  def WriteSetCode(self, f, indent, var):
    indent_str = ' ' * indent
    f.write("%s%sGLES2Util::MapUint64ToTwoUint32(static_cast<uint64_t>(%s),\n" %
            (indent_str, _Namespace(), var))
    f.write("%s                                &%s_0,\n" %
            (indent_str, self.name))
    f.write("%s                                &%s_1);\n" %
            (indent_str, self.name))

class Function():
  """A class that represents a function."""

  def __init__(self, name, info, named_type_info, type_handlers):
    self.name = name
    self.named_type_info = named_type_info

    self.prefixed_name = info['prefixed_name']
    self.original_name = info['original_name']

    self.original_args = self.ParseArgs(info['original_args'])

    if 'cmd_args' in info:
      self.args_for_cmds = self.ParseArgs(info['cmd_args'])
    else:
      self.args_for_cmds = self.original_args[:]

    self.passthrough_service_doer_args = self.original_args[:]

    if 'size_args' in info:
      self.size_args = info['size_args']
    else:
      self.size_args = {}

    self.return_type = info['return_type']
    if self.return_type != 'void':
      self.return_arg = CreateArg(info['return_type'] + " result",
                                  named_type_info)
    else:
      self.return_arg = None

    self.num_pointer_args = sum(
      [1 for arg in self.args_for_cmds if arg.IsPointer()])
    if self.num_pointer_args > 0:
      for arg in reversed(self.original_args):
        if arg.IsPointer():
          self.last_original_pointer_arg = arg
          break
    else:
      self.last_original_pointer_arg = None
    self.info = info
    self.type_handler = type_handlers[info['type']]
    self.can_auto_generate = (self.num_pointer_args == 0 and
                              info['return_type'] == "void")

    # Satisfy pylint warning attribute-defined-outside-init.
    #
    # self.cmd_args is typically set in InitFunction, but that method may be
    # overriden.
    self.cmd_args = []
    self.InitFunction()

  def ParseArgs(self, arg_string):
    """Parses a function arg string."""
    args = []
    parts = arg_string.split(',')
    for p in parts:
      arg = CreateArg(p, self.named_type_info)
      if arg:
        args.append(arg)
    return args

  def IsType(self, type_name):
    """Returns true if function is a certain type."""
    return self.info['type'] == type_name

  def InitFunction(self):
    """Creates command args and calls the init function for the type handler.

    Creates argument lists for command buffer commands, eg. self.cmd_args and
    self.init_args.
    Calls the type function initialization.
    Override to create different kind of command buffer command argument lists.
    """
    self.cmd_args = []
    for arg in self.args_for_cmds:
      arg.AddCmdArgs(self.cmd_args)

    self.init_args = []
    for arg in self.args_for_cmds:
      arg.AddInitArgs(self.init_args)

    if self.return_arg:
      self.init_args.append(self.return_arg)

    self.type_handler.InitFunction(self)

  def IsImmediate(self):
    """Returns whether the function is immediate data function or not."""
    return False

  def IsES3(self):
    """Returns whether the function requires an ES3 context or not."""
    return self.GetInfo('es3', False)

  def IsES31(self):
    """Returns whether the function requires an ES31 context or not."""
    return self.GetInfo('es31', False)

  def GetInfo(self, name, default = None):
    """Returns a value from the function info for this function."""
    if name in self.info:
      return self.info[name]
    return default

  def GetValidArg(self, arg):
    """Gets a valid argument value for the parameter arg from the function info
    if one exists."""
    try:
      index = self.GetOriginalArgs().index(arg)
    except ValueError:
      return None

    valid_args = self.GetInfo('valid_args')
    if valid_args and str(index) in valid_args:
      return valid_args[str(index)]
    return None

  def AddInfo(self, name, value):
    """Adds an info."""
    self.info[name] = value

  def IsExtension(self):
    return self.GetInfo('extension') or self.GetInfo('extension_flag')

  def IsCoreGLFunction(self):
    return (not self.IsExtension() and
            not self.GetInfo('pepper_interface') and
            not self.IsES3() and
            not self.IsES31())

  def InPepperInterface(self, interface):
    ext = self.GetInfo('pepper_interface')
    if not interface.GetName():
      return self.IsCoreGLFunction()
    return ext == interface.GetName()

  def InAnyPepperExtension(self):
    return self.IsCoreGLFunction() or self.GetInfo('pepper_interface')

  def GetErrorReturnString(self):
    if self.GetInfo("error_return"):
      return self.GetInfo("error_return")
    if self.return_type == "GLboolean":
      return "GL_FALSE"
    if "*" in self.return_type:
      return "nullptr"
    return "0"

  def GetGLFunctionName(self):
    """Gets the function to call to execute GL for this command."""
    if self.GetInfo('decoder_func'):
      return self.GetInfo('decoder_func')
    return "api()->gl%sFn" % self.original_name

  def GetGLTestFunctionName(self):
    gl_func_name = self.GetInfo('gl_test_func')
    if gl_func_name == None:
      gl_func_name = self.GetGLFunctionName()
    if gl_func_name.startswith("gl"):
      gl_func_name = gl_func_name[2:]
    else:
      gl_func_name = self.original_name
    return gl_func_name

  def GetDataTransferMethods(self):
    return self.GetInfo('data_transfer_methods',
                        ['immediate' if self.num_pointer_args == 1 else 'shm'])

  def AddCmdArg(self, arg):
    """Adds a cmd argument to this function."""
    self.cmd_args.append(arg)

  def GetCmdArgs(self):
    """Gets the command args for this function."""
    return self.cmd_args

  def ClearCmdArgs(self):
    """Clears the command args for this function."""
    self.cmd_args = []

  def GetCmdConstants(self):
    """Gets the constants for this function."""
    return [arg for arg in self.args_for_cmds if arg.IsConstant()]

  def GetInitArgs(self):
    """Gets the init args for this function."""
    return self.init_args

  def GetOriginalArgs(self):
    """Gets the original arguments to this function."""
    return self.original_args

  def GetPassthroughServiceDoerArgs(self):
    """Gets the original arguments to this function."""
    return self.passthrough_service_doer_args

  def GetLastOriginalArg(self):
    """Gets the last original argument to this function."""
    return self.original_args[len(self.original_args) - 1]

  def GetLastOriginalPointerArg(self):
    return self.last_original_pointer_arg

  def GetResourceIdArg(self):
    for arg in self.original_args:
      if hasattr(arg, 'resource_type'):
        return arg
    return None

  def _MaybePrependComma(self, arg_string, add_comma):
    """Adds a comma if arg_string is not empty and add_comma is true."""
    comma = ""
    if add_comma and len(arg_string):
      comma = ", "
    return "%s%s" % (comma, arg_string)

  def MakeTypedOriginalArgString(self, prefix, add_comma = False,
                                 add_default = False):
    """Gets a list of arguments as they are in GL."""
    args = self.GetOriginalArgs()
    def ArgToString(arg):
      tmp = [arg.type, prefix + arg.name]
      if add_default and arg.default:
        tmp.append("=")
        tmp.append(arg.default)
      return " ".join(tmp)
    arg_string = ", ".join([ArgToString(arg) for arg in args])
    return self._MaybePrependComma(arg_string, add_comma)

  def MakeOriginalArgString(self, prefix, add_comma = False, separator = ", "):
    """Gets the list of arguments as they are in GL."""
    args = self.GetOriginalArgs()
    arg_string = separator.join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self._MaybePrependComma(arg_string, add_comma)

  def MakePassthroughServiceDoerArgString(self, prefix, add_comma = False,
                                          separator = ", "):
    """Gets the list of arguments as they are in used by the passthrough
       service doer function."""
    args = self.GetPassthroughServiceDoerArgs()
    arg_string = separator.join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self._MaybePrependComma(arg_string, add_comma)

  def MakeHelperArgString(self, prefix, add_comma = False, separator = ", "):
    """Gets a list of GL arguments after removing unneeded arguments."""
    args = self.GetOriginalArgs()
    arg_string = separator.join(
        ["%s%s" % (prefix, arg.name)
         for arg in args if not arg.IsConstant()])
    return self._MaybePrependComma(arg_string, add_comma)

  def MakeTypedPepperArgString(self, prefix):
    """Gets a list of arguments as they need to be for Pepper."""
    if self.GetInfo("pepper_args"):
      return self.GetInfo("pepper_args")
    return self.MakeTypedOriginalArgString(prefix, False)

  def MapCTypeToPepperIdlType(self, ctype, is_for_return_type=False):
    """Converts a C type name to the corresponding Pepper IDL type."""
    idltype = {
        'char*': '[out] str_t',
        'const GLchar* const*': '[out] cstr_t',
        'const char*': 'cstr_t',
        'const void*': 'mem_t',
        'void*': '[out] mem_t',
        'void**': '[out] mem_ptr_t',
    }.get(ctype, ctype)
    # We use "GLxxx_ptr_t" for "GLxxx*".
    matched = re.match(r'(const )?(GL\w+)\*$', ctype)
    if matched:
      idltype = matched.group(2) + '_ptr_t'
      if not matched.group(1):
        idltype = '[out] ' + idltype
    # If an in/out specifier is not specified yet, prepend [in].
    if idltype[0] != '[':
      idltype = '[in] ' + idltype
    # Strip the in/out specifier for a return type.
    if is_for_return_type:
      idltype = re.sub(r'\[\w+\] ', '', idltype)
    return idltype

  def MakeTypedPepperIdlArgStrings(self):
    """Gets a list of arguments as they need to be for Pepper IDL."""
    args = self.GetOriginalArgs()
    return ["%s %s" % (self.MapCTypeToPepperIdlType(arg.type), arg.name)
            for arg in args]

  def GetPepperName(self):
    if self.GetInfo("pepper_name"):
      return self.GetInfo("pepper_name")
    return self.name

  def MakeTypedCmdArgString(self, prefix, add_comma = False):
    """Gets a typed list of arguments as they need to be for command buffers."""
    args = self.GetCmdArgs()
    arg_string = ", ".join(
        ["%s %s%s" % (arg.type, prefix, arg.name) for arg in args])
    return self._MaybePrependComma(arg_string, add_comma)

  def MakeCmdArgString(self, prefix, add_comma = False):
    """Gets the list of arguments as they need to be for command buffers."""
    args = self.GetCmdArgs()
    arg_string = ", ".join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self._MaybePrependComma(arg_string, add_comma)

  def MakeTypedInitString(self, prefix, add_comma = False):
    """Gets a typed list of arguments as they need to be for cmd Init/Set."""
    args = self.GetInitArgs()
    arg_string = ", ".join(
        ["%s %s%s" % (arg.type, prefix, arg.name) for arg in args])
    return self._MaybePrependComma(arg_string, add_comma)

  def MakeInitString(self, prefix, add_comma = False):
    """Gets the list of arguments as they need to be for cmd Init/Set."""
    args = self.GetInitArgs()
    arg_string = ", ".join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self._MaybePrependComma(arg_string, add_comma)

  def MakeLogArgString(self):
    """Makes a string of the arguments for the LOG macros"""
    args = self.GetOriginalArgs()
    return ' << ", " << '.join([arg.GetLogArg() for arg in args])

  def WriteHandlerValidation(self, f):
    """Writes validation code for the function."""
    for arg in self.GetOriginalArgs():
      arg.WriteValidationCode(f, self)
    self.WriteValidationCode(f)

  def WriteQueueTraceEvent(self, f):
    if self.GetInfo("trace_queueing_flow", False):
      trace = 'TRACE_DISABLED_BY_DEFAULT("gpu_cmd_queue")'
      f.write("""if (c.trace_id) {
          TRACE_EVENT_WITH_FLOW0(%s, "CommandBufferQueue",
          c.trace_id, TRACE_EVENT_FLAG_FLOW_IN);\n}""" % trace)

  def WritePassthroughHandlerValidation(self, f):
    """Writes validation code for the function."""
    for arg in self.GetOriginalArgs():
      arg.WritePassthroughValidationCode(f, self)

  def WriteHandlerImplementation(self, f):
    """Writes the handler implementation for this command."""
    self.type_handler.WriteHandlerImplementation(self, f)

  def WriteValidationCode(self, f):
    """Writes the validation code for a command."""

  def WriteCmdFlag(self, f):
    """Writes the cmd cmd_flags constant."""
    # By default trace only at the highest level 3.
    trace_level = int(self.GetInfo('trace_level', default = 3))
    if trace_level not in range(0, 4):
      raise KeyError("Unhandled trace_level: %d" % trace_level)

    cmd_flags = ('CMD_FLAG_SET_TRACE_LEVEL(%d)' % trace_level)
    f.write("  static const uint8_t cmd_flags = %s;\n" % cmd_flags)


  def WriteCmdArgFlag(self, f):
    """Writes the cmd kArgFlags constant."""
    f.write("  static const cmd::ArgFlags kArgFlags = cmd::kFixed;\n")

  def WriteCmdComputeSize(self, f):
    """Writes the ComputeSize function for the command."""
    f.write("  static uint32_t ComputeSize() {\n")
    f.write(
        "    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT\n")
    f.write("  }\n")
    f.write("\n")

  def WriteCmdSetHeader(self, f):
    """Writes the cmd's SetHeader function."""
    f.write("  void SetHeader() {\n")
    f.write("    header.SetCmd<ValueType>();\n")
    f.write("  }\n")
    f.write("\n")

  def WriteCmdInit(self, f):
    """Writes the cmd's Init function."""
    f.write("  void Init(%s) {\n" % self.MakeTypedCmdArgString("_"))
    f.write("    SetHeader();\n")
    args = self.GetCmdArgs()
    for arg in args:
      arg.WriteSetCode(f, 4, '_%s' % arg.name)
    if self.GetInfo("trace_queueing_flow", False):
      trace = 'TRACE_DISABLED_BY_DEFAULT("gpu_cmd_queue")'
      f.write('bool is_tracing = false;')
      f.write('TRACE_EVENT_CATEGORY_GROUP_ENABLED(%s, &is_tracing);' % trace)
      f.write('if (is_tracing) {')
      f.write('  trace_id = base::RandUint64();')
      f.write('TRACE_EVENT_WITH_FLOW1(%s, "CommandBufferQueue",' % trace)
      f.write('trace_id, TRACE_EVENT_FLAG_FLOW_OUT,')
      f.write('"command", "%s");' % self.name)
      f.write('} else {\n  trace_id = 0;\n}\n');
    f.write("}\n")
    f.write("\n")

  def WriteCmdSet(self, f):
    """Writes the cmd's Set function."""
    copy_args = self.MakeCmdArgString("_", False)
    f.write("  void* Set(void* cmd%s) {\n" %
               self.MakeTypedCmdArgString("_", True))
    f.write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    f.write("    return NextCmdAddress<ValueType>(cmd);\n")
    f.write("  }\n")
    f.write("\n")

  def WriteArgAccessors(self, f):
    """Writes the cmd's accessor functions."""
    for arg in self.GetCmdArgs():
      arg.WriteArgAccessor(f)

  def WriteStruct(self, f):
    self.type_handler.WriteStruct(self, f)

  def WriteDocs(self, f):
    self.type_handler.WriteDocs(self, f)

  def WriteCmdHelper(self, f):
    """Writes the cmd's helper."""
    self.type_handler.WriteCmdHelper(self, f)

  def WriteServiceImplementation(self, f):
    """Writes the service implementation for a command."""
    self.type_handler.WriteServiceImplementation(self, f)

  def WritePassthroughServiceImplementation(self, f):
    """Writes the service implementation for a command."""
    self.type_handler.WritePassthroughServiceImplementation(self, f)

  def WriteServiceUnitTest(self, f, *extras):
    """Writes the service implementation for a command."""
    self.type_handler.WriteServiceUnitTest(self, f, *extras)

  def WriteGLES2CLibImplementation(self, f):
    """Writes the GLES2 C Lib Implemention."""
    self.type_handler.WriteGLES2CLibImplementation(self, f)

  def WriteGLES2InterfaceHeader(self, f):
    """Writes the GLES2 Interface declaration."""
    self.type_handler.WriteGLES2InterfaceHeader(self, f)

  def WriteGLES2InterfaceStub(self, f):
    """Writes the GLES2 Interface Stub declaration."""
    self.type_handler.WriteGLES2InterfaceStub(self, f)

  def WriteGLES2InterfaceStubImpl(self, f):
    """Writes the GLES2 Interface Stub declaration."""
    self.type_handler.WriteGLES2InterfaceStubImpl(self, f)

  def WriteGLES2ImplementationHeader(self, f):
    """Writes the GLES2 Implemention declaration."""
    self.type_handler.WriteGLES2ImplementationHeader(self, f)

  def WriteGLES2Implementation(self, f):
    """Writes the GLES2 Implemention definition."""
    self.type_handler.WriteGLES2Implementation(self, f)

  def WriteGLES2TraceImplementationHeader(self, f):
    """Writes the GLES2 Trace Implemention declaration."""
    self.type_handler.WriteGLES2TraceImplementationHeader(self, f)

  def WriteGLES2TraceImplementation(self, f):
    """Writes the GLES2 Trace Implemention definition."""
    self.type_handler.WriteGLES2TraceImplementation(self, f)

  def WriteGLES2Header(self, f):
    """Writes the GLES2 Implemention unit test."""
    self.type_handler.WriteGLES2Header(self, f)

  def WriteGLES2ImplementationUnitTest(self, f):
    """Writes the GLES2 Implemention unit test."""
    self.type_handler.WriteGLES2ImplementationUnitTest(self, f)

  def WriteDestinationInitalizationValidation(self, f):
    """Writes the client side destintion initialization validation."""
    self.type_handler.WriteDestinationInitalizationValidation(self, f)

  def WriteFormatTest(self, f):
    """Writes the cmd's format test."""
    self.type_handler.WriteFormatTest(self, f)


class PepperInterface():
  """A class that represents a function."""

  def __init__(self, info):
    self.name = info["name"]
    self.dev = info["dev"]

  def GetName(self):
    return self.name

  def GetInterfaceName(self):
    upperint = ""
    dev = ""
    if self.name:
      upperint = "_" + self.name.upper()
    if self.dev:
      dev = "_DEV"
    return "PPB_OPENGLES2%s%s_INTERFACE" % (upperint, dev)

  def GetStructName(self):
    dev = ""
    if self.dev:
      dev = "_Dev"
    return "PPB_OpenGLES2%s%s" % (self.name, dev)


class ImmediateFunction(Function):
  """A class that represents an immediate function command."""

  def __init__(self, func, type_handlers):
    Function.__init__(
        self,
        "%sImmediate" % func.name,
        func.info,
        func.named_type_info,
        type_handlers)

  def InitFunction(self):
    # Override args in original_args and args_for_cmds with immediate versions
    # of the args.

    new_original_args = []
    for arg in self.original_args:
      new_arg = arg.GetImmediateVersion()
      if new_arg:
        new_original_args.append(new_arg)
    self.original_args = new_original_args

    new_args_for_cmds = []
    for arg in self.args_for_cmds:
      new_arg = arg.GetImmediateVersion()
      if new_arg:
        new_args_for_cmds.append(new_arg)

    self.args_for_cmds = new_args_for_cmds

    Function.InitFunction(self)

  def IsImmediate(self):
    return True

  def WriteServiceImplementation(self, f):
    """Overridden from Function"""
    self.type_handler.WriteImmediateServiceImplementation(self, f)

  def WritePassthroughServiceImplementation(self, f):
    """Overridden from Function"""
    self.type_handler.WritePassthroughImmediateServiceImplementation(self, f)

  def WriteHandlerImplementation(self, f):
    """Overridden from Function"""
    self.type_handler.WriteImmediateHandlerImplementation(self, f)

  def WriteServiceUnitTest(self, f, *extras):
    """Writes the service implementation for a command."""
    self.type_handler.WriteImmediateServiceUnitTest(self, f, *extras)

  def WriteValidationCode(self, f):
    """Overridden from Function"""
    self.type_handler.WriteImmediateValidationCode(self, f)

  def WriteCmdArgFlag(self, f):
    """Overridden from Function"""
    f.write("  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;\n")

  def WriteCmdComputeSize(self, f):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdComputeSize(self, f)

  def WriteCmdSetHeader(self, f):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdSetHeader(self, f)

  def WriteCmdInit(self, f):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdInit(self, f)

  def WriteCmdSet(self, f):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdSet(self, f)

  def WriteCmdHelper(self, f):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdHelper(self, f)

  def WriteFormatTest(self, f):
    """Overridden from Function"""
    self.type_handler.WriteImmediateFormatTest(self, f)


class BucketFunction(Function):
  """A class that represnets a bucket version of a function command."""

  def __init__(self, func, type_handlers):
    Function.__init__(
      self,
      "%sBucket" % func.name,
      func.info,
      func.named_type_info,
      type_handlers)

  def InitFunction(self):
    # Override args in original_args and args_for_cmds with bucket versions
    # of the args.

    new_original_args = []
    for arg in self.original_args:
      new_arg = arg.GetBucketVersion()
      if new_arg:
        new_original_args.append(new_arg)
    self.original_args = new_original_args

    new_args_for_cmds = []
    for arg in self.args_for_cmds:
      new_arg = arg.GetBucketVersion()
      if new_arg:
        new_args_for_cmds.append(new_arg)

    self.args_for_cmds = new_args_for_cmds

    Function.InitFunction(self)

  def WriteServiceImplementation(self, f):
    """Overridden from Function"""
    self.type_handler.WriteBucketServiceImplementation(self, f)

  def WritePassthroughServiceImplementation(self, f):
    """Overridden from Function"""
    self.type_handler.WritePassthroughBucketServiceImplementation(self, f)

  def WriteHandlerImplementation(self, f):
    """Overridden from Function"""
    self.type_handler.WriteBucketHandlerImplementation(self, f)

  def WriteServiceUnitTest(self, f, *extras):
    """Overridden from Function"""
    self.type_handler.WriteBucketServiceUnitTest(self, f, *extras)

  def MakeOriginalArgString(self, prefix, add_comma = False, separator = ", "):
    """Overridden from Function"""
    args = self.GetOriginalArgs()
    arg_string = separator.join(
        ["%s%s" % (prefix, arg.name[0:-10] if arg.name.endswith("_bucket_id")
                           else arg.name) for arg in args])
    return super()._MaybePrependComma(arg_string, add_comma)


def CreateArg(arg_string, named_type_info):
  """Convert string argument to an Argument class that represents it.

  The parameter 'arg_string' can be a single argument to a GL function,
  something like 'GLsizei width' or 'const GLenum* bufs'. Returns an instance of
  the Argument class, or None if 'arg_string' is 'void'.
  """
  if arg_string == 'void':
    return None

  arg_string = arg_string.strip()
  arg_default = None
  if '=' in arg_string:
    arg_string, arg_default = arg_string.split('=')
    arg_default = arg_default.strip()
  arg_parts = arg_string.split()
  assert len(arg_parts) > 1
  arg_name = arg_parts[-1]
  arg_type = " ".join(arg_parts[0:-1])
  t = arg_parts[0]  # only the first part of arg_type

  # Is this a pointer argument?
  if arg_string.find('*') >= 0:
    return PointerArgument(arg_name, arg_type, arg_default)
  if t.startswith('EnumClass'):
    return EnumClassArgument(arg_name, arg_type, named_type_info, arg_default)
  # Is this a resource argument? Must come after pointer check.
  if t.startswith('GLidBind'):
    return ResourceIdBindArgument(arg_name, arg_type, arg_default)
  if t.startswith('GLidZero'):
    return ResourceIdZeroArgument(arg_name, arg_type, arg_default)
  if t.startswith('GLid'):
    return ResourceIdArgument(arg_name, arg_type, arg_default)
  if t.startswith('GLenum') and t !='GLenum':
    return EnumArgument(arg_name, arg_type, named_type_info, arg_default)
  if t.startswith('GLbitfield') and t != 'GLbitfield':
    return BitFieldArgument(arg_name, arg_type, named_type_info, arg_default)
  if t.startswith('GLboolean'):
    return GLBooleanArgument(arg_name, arg_type, arg_default)
  if t.startswith('GLintUniformLocation'):
    return UniformLocationArgument(arg_name, arg_default)
  if (t.startswith('GLint') and t != 'GLint' and
        not t.startswith('GLintptr')):
    return IntArgument(arg_name, arg_type, named_type_info, arg_default)
  if t == 'bool':
    return BoolArgument(arg_name, arg_type, arg_default)
  if t in ('GLsizeiNotNegative', 'GLintptrNotNegative'):
    return SizeNotNegativeArgument(arg_name, t.replace('NotNegative', ''),
                                   arg_default)
  if t.startswith('GLsize'):
    return SizeArgument(arg_name, arg_type, arg_default)
  if t in ('GLuint64', 'GLint64'):
    return Int64Argument(arg_name, arg_type, arg_default)
  return Argument(arg_name, arg_type, arg_default)


class GLGenerator():
  """A class to generate GL command buffers."""

  _whitespace_re = re.compile(r'^\w*$')
  _comment_re = re.compile(r'^//.*$')
  _function_re = re.compile(r'^GL_APICALL(.*?)GL_APIENTRY (.*?) \((.*?)\);$')

  def __init__(self, verbose, year, function_info, named_type_info,
               chromium_root_dir):
    self.original_functions = []
    self.functions = []
    self.chromium_root_dir = chromium_root_dir
    self.verbose = verbose
    self.year = year
    self.errors = 0
    self.pepper_interfaces = []
    self.interface_info = {}
    self.generated_cpp_filenames = []
    self.function_info = function_info
    self.named_type_info = named_type_info
    self.capability_flags = _CAPABILITY_FLAGS
    self.type_handlers = {
        '': TypeHandler(),
        'Bind': BindHandler(),
        'Create': CreateHandler(),
        'Custom': CustomHandler(),
        'Data': DataHandler(),
        'Delete': DeleteHandler(),
        'DELn': DELnHandler(),
        'GENn': GENnHandler(),
        'GETn': GETnHandler(),
        'GLchar': GLcharHandler(),
        'GLcharN': GLcharNHandler(),
        'Is': IsHandler(),
        'NoCommand': NoCommandHandler(),
        'PUT': PUTHandler(),
        'PUTn': PUTnHandler(),
        'PUTSTR': PUTSTRHandler(),
        'PUTXn': PUTXnHandler(),
        'StateSet': StateSetHandler(),
        'StateSetRGBAlpha': StateSetRGBAlphaHandler(),
        'StateSetFrontBack': StateSetFrontBackHandler(),
        'StateSetFrontBackSeparate':
        StateSetFrontBackSeparateHandler(),
        'StateSetNamedParameter': StateSetNamedParameter(),
        'STRn': STRnHandler(),
    }

    for interface in _PEPPER_INTERFACES:
      interface = PepperInterface(interface)
      self.pepper_interfaces.append(interface)
      self.interface_info[interface.GetName()] = interface

  def AddFunction(self, func):
    """Adds a function."""
    self.functions.append(func)

  def GetFunctionInfo(self, name):
    """Gets a type info for the given function name."""
    if name in self.function_info:
      func_info = self.function_info[name].copy()
    else:
      func_info = {}

    if not 'type' in func_info:
      func_info['type'] = ''

    return func_info

  def Log(self, msg):
    """Prints something if verbose is true."""
    if self.verbose:
      print(msg)

  def Error(self, msg):
    """Prints an error."""
    print("Error: %s" % msg)
    self.errors += 1

  def ParseGLH(self, filename):
    """Parses the cmd_buffer_functions.txt file and extracts the functions"""
    filename = os.path.join(self.chromium_root_dir, filename)
    with open(filename, "r") as f:
      functions = f.read()
    for line in functions.splitlines():
      if self._whitespace_re.match(line) or self._comment_re.match(line):
        continue
      match = self._function_re.match(line)
      if match:
        prefixed_name = match.group(2)
        func_name = prefixed_name[2:]
        func_info = self.GetFunctionInfo(func_name)
        if func_info['type'] == 'Noop':
          continue

        parsed_func_info = {
          'prefixed_name': prefixed_name,
          'original_name': func_name,
          'original_args': match.group(3),
          'return_type': match.group(1).strip(),
        }

        for k in parsed_func_info:
          if not k in func_info:
            func_info[k] = parsed_func_info[k]

        f = Function(func_name, func_info, self.named_type_info,
                     self.type_handlers)
        if not f.GetInfo('internal'):
          self.original_functions.append(f)

        #for arg in f.GetOriginalArgs():
        #  if not isinstance(arg, EnumArgument) and arg.type == 'GLenum':
        #    self.Log("%s uses bare GLenum %s." % (func_name, arg.name))

        func_type = f.GetInfo('type')
        if func_type != 'NoCommand':
          if f.type_handler.NeedsDataTransferFunction(f):
            methods = f.GetDataTransferMethods()
            if 'immediate' in methods:
              self.AddFunction(ImmediateFunction(f, self.type_handlers))
            if 'bucket' in methods:
              self.AddFunction(BucketFunction(f, self.type_handlers))
            if 'shm' in methods:
              self.AddFunction(f)
          else:
            self.AddFunction(f)
      else:
        self.Error("Could not parse function: %s using regex: %s" %
                   (line, self._function_re.pattern))

    self.Log("Auto Generated Functions    : %d" %
             len([f for f in self.functions if f.can_auto_generate or
                  (not f.IsType('') and not f.IsType('Custom') and
                   not f.IsType('Todo'))]))

    funcs = [f for f in self.functions if not f.can_auto_generate and
             (f.IsType('') or f.IsType('Custom') or f.IsType('Todo'))]
    self.Log("Non Auto Generated Functions: %d" % len(funcs))

    for f in funcs:
      self.Log("  %-10s %-20s gl%s" % (f.info['type'], f.return_type, f.name))

  def WriteCommandIds(self, filename):
    """Writes the command buffer format"""
    with CHeaderWriter(filename, self.year) as f:
      f.write("#define %s_COMMAND_LIST(OP) \\\n" % _upper_prefix)
      cmd_id = 256
      for func in self.functions:
        f.write("  %-60s /* %d */ \\\n" %
                   ("OP(%s)" % func.name, cmd_id))
        cmd_id += 1
      f.write("\n")

      f.write("enum CommandId {\n")
      f.write("  kOneBeforeStartPoint = cmd::kLastCommonId,  "
                 "// All %s commands start after this.\n" % _prefix)
      f.write("#define %s_CMD_OP(name) k ## name,\n" % _upper_prefix)
      f.write("  %s_COMMAND_LIST(%s_CMD_OP)\n" %
              (_upper_prefix, _upper_prefix))
      f.write("#undef %s_CMD_OP\n" % _upper_prefix)
      f.write("  kNumCommands,\n")
      f.write("  kFirst%sCommand = kOneBeforeStartPoint + 1\n" % _prefix)
      f.write("};\n")
      f.write("\n")
    self.generated_cpp_filenames.append(filename)

  def WriteFormat(self, filename):
    """Writes the command buffer format"""
    with CHeaderWriter(filename, self.year) as f:
      # Forward declaration of a few enums used in constant argument
      # to avoid including GL header files.
      enum_defines = {}
      if 'FenceSync' in self.function_info:
        enum_defines['GL_SYNC_GPU_COMMANDS_COMPLETE'] = '0x9117'
      if 'ClientWaitSync' in self.function_info:
        enum_defines['GL_SYNC_FLUSH_COMMANDS_BIT'] = '0x00000001'
      f.write('\n')
      for enum in enum_defines:
        f.write("#define %s %s\n" % (enum, enum_defines[enum]))
      f.write('\n')
      for func in self.functions:
        func.WriteStruct(f)
      f.write("\n")
    self.generated_cpp_filenames.append(filename)

  def WriteDocs(self, filename):
    """Writes the command buffer doc version of the commands"""
    with CHeaderWriter(filename, self.year) as f:
      for func in self.functions:
        func.WriteDocs(f)
      f.write("\n")
    self.generated_cpp_filenames.append(filename)

  def WriteFormatTest(self, filename):
    """Writes the command buffer format test."""
    comment = ("// This file contains unit tests for %s commands\n"
               "// It is included by %s_cmd_format_test.cc\n\n" %
               (_lower_prefix, _lower_prefix))
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.functions:
        func.WriteFormatTest(f)
    self.generated_cpp_filenames.append(filename)

  def WriteCmdHelperHeader(self, filename):
    """Writes the gles2 command helper."""
    with CHeaderWriter(filename, self.year) as f:
      for func in self.functions:
        func.WriteCmdHelper(f)
    self.generated_cpp_filenames.append(filename)

  def WriteServiceContextStateHeader(self, filename):
    """Writes the service context state header."""
    comment = "// It is included by context_state.h\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      f.write("struct EnableFlags {\n")
      f.write("  EnableFlags();\n")
      for capability in self.capability_flags:
        f.write("  bool %s;\n" % capability['name'])
        f.write("  bool cached_%s;\n" % capability['name'])
      f.write("};\n\n")

      for state_name in sorted(_STATE_INFO.keys()):
        state = _STATE_INFO[state_name]
        for item in state['states']:
          if isinstance(item['default'], list):
            f.write("%s %s[%d];\n" % (item['type'], item['name'],
                                         len(item['default'])))
          else:
            f.write("%s %s;\n" % (item['type'], item['name']))

          if item.get('cached', False):
            if isinstance(item['default'], list):
              f.write("%s cached_%s[%d];\n" % (item['type'], item['name'],
                                                  len(item['default'])))
            else:
              f.write("%s cached_%s;\n" % (item['type'], item['name']))

      f.write("\n")
      f.write("""
          inline void SetDeviceCapabilityState(GLenum cap, bool enable) {
            switch (cap) {
          """)
      for capability in self.capability_flags:
        f.write("""\
              case GL_%s:
            """ % capability['name'].upper())
        f.write("""\
                if (enable_flags.cached_%(name)s == enable &&
                    !ignore_cached_state)
                  return;
                enable_flags.cached_%(name)s = enable;
                break;
            """ % capability)

      f.write("""\
              default:
                NOTREACHED_IN_MIGRATION();
                return;
            }
            if (enable)
              api()->glEnableFn(cap);
            else
              api()->glDisableFn(cap);
          }
          """)
    self.generated_cpp_filenames.append(filename)

  def WriteClientContextStateHeader(self, filename):
    """Writes the client context state header."""
    comment = "// It is included by client_context_state.h\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      f.write("struct EnableFlags {\n")
      f.write("  EnableFlags();\n")
      for capability in self.capability_flags:
        if 'extension_flag' in capability:
          continue
        f.write("  bool %s;\n" % capability['name'])
      f.write("};\n\n")
    self.generated_cpp_filenames.append(filename)

  def WriteContextStateGetters(self, f, class_name):
    """Writes the state getters."""
    for gl_type in ["GLint", "GLfloat"]:
      f.write("""
bool %s::GetStateAs%s(
    GLenum pname, %s* params, GLsizei* num_written) const {
  switch (pname) {
""" % (class_name, gl_type, gl_type))
      for state_name in sorted(_STATE_INFO.keys()):
        state = _STATE_INFO[state_name]
        if 'enum' in state:
          f.write("    case %s:\n" % state['enum'])
          f.write("      *num_written = %d;\n" % len(state['states']))
          f.write("      if (params) {\n")
          for ndx,item in enumerate(state['states']):
            f.write("        params[%d] = static_cast<%s>(%s);\n" %
                       (ndx, gl_type, item['name']))
          f.write("      }\n")
          f.write("      return true;\n")
        else:
          for item in state['states']:
            f.write("    case %s:\n" % item['enum'])
            if isinstance(item['default'], list):
              item_len = len(item['default'])
              f.write("      *num_written = %d;\n" % item_len)
              f.write("      if (params) {\n")
              if item['type'] == gl_type:
                f.write("        memcpy(params, %s, sizeof(%s) * %d);\n" %
                           (item['name'], item['type'], item_len))
              else:
                f.write("        for (size_t i = 0; i < %s; ++i) {\n" %
                           item_len)
                f.write("          params[i] = %s;\n" %
                           (GetGLGetTypeConversion(gl_type, item['type'],
                                                   "%s[i]" % item['name'])))
                f.write("        }\n");
            else:
              f.write("      *num_written = 1;\n")
              f.write("      if (params) {\n")
              f.write("        params[0] = %s;\n" %
                         (GetGLGetTypeConversion(gl_type, item['type'],
                                                 item['name'])))
            f.write("      }\n")
            f.write("      return true;\n")
      for capability in self.capability_flags:
            f.write("    case GL_%s:\n" % capability['name'].upper())
            f.write("      *num_written = 1;\n")
            f.write("      if (params) {\n")
            f.write(
                "        params[0] = static_cast<%s>(enable_flags.%s);\n" %
                (gl_type, capability['name']))
            f.write("      }\n")
            f.write("      return true;\n")
      f.write("""    default:
      return false;
  }
}
""")

  def WriteServiceContextStateImpl(self, filename):
    """Writes the context state service implementation."""
    comment = "// It is included by context_state.cc\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      code = []
      for capability in self.capability_flags:
        code.append("%s(%s)" %
                    (capability['name'],
                     ('false', 'true')['default' in capability]))
        code.append("cached_%s(%s)" %
                    (capability['name'],
                     ('false', 'true')['default' in capability]))
      f.write("ContextState::EnableFlags::EnableFlags()\n    : %s {\n}\n" %
                 ",\n      ".join(code))
      f.write("\n")

      f.write("void ContextState::Initialize() {\n")
      for state_name in sorted(_STATE_INFO.keys()):
        state = _STATE_INFO[state_name]
        for item in state['states']:
          if isinstance(item['default'], list):
            for ndx, value in enumerate(item['default']):
              f.write("  %s[%d] = %s;\n" % (item['name'], ndx, value))
          else:
            f.write("  %s = %s;\n" % (item['name'], item['default']))
          if item.get('cached', False):
            if isinstance(item['default'], list):
              for ndx, value in enumerate(item['default']):
                f.write("  cached_%s[%d] = %s;\n" % (item['name'], ndx, value))
            else:
              f.write("  cached_%s = %s;\n" % (item['name'], item['default']))
      f.write("}\n")

      f.write("""
void ContextState::InitCapabilities(const ContextState* prev_state) const {
""")
      def WriteCapabilities(test_prev, es3_caps):
        for capability in self.capability_flags:
          capability_name = capability['name']
          capability_no_init = 'no_init' in capability and \
              capability['no_init'] == True
          if capability_no_init:
            continue
          capability_es3 = 'es3' in capability and capability['es3'] == True
          if capability_es3 and not es3_caps or not capability_es3 and es3_caps:
            continue
          if 'extension_flag' in capability:
            f.write("  if (feature_info_->feature_flags().%s) {\n  " %
                       capability['extension_flag'])
          if test_prev:
            f.write("""  if (prev_state->enable_flags.cached_%s !=
                                enable_flags.cached_%s) {\n""" %
                       (capability_name, capability_name))
          f.write("    EnableDisable(GL_%s, enable_flags.cached_%s);\n" %
                     (capability_name.upper(), capability_name))
          if test_prev:
            f.write("  }")
          if 'extension_flag' in capability:
            f.write("  }")

      f.write("  if (prev_state) {")
      WriteCapabilities(True, False)
      f.write("    if (feature_info_->IsES3Capable()) {\n")
      WriteCapabilities(True, True)
      f.write("    }\n")
      f.write("  } else {")
      WriteCapabilities(False, False)
      f.write("    if (feature_info_->IsES3Capable()) {\n")
      WriteCapabilities(False, True)
      f.write("    }\n")
      f.write("  }")
      f.write("""}

void ContextState::InitState(const ContextState *prev_state) const {
""")

      def WriteStates(test_prev):
        # We need to sort the keys so the expectations match
        for state_name in sorted(_STATE_INFO.keys()):
          state = _STATE_INFO[state_name]
          if 'no_init' in state and state['no_init']:
            continue
          if state['type'] == 'FrontBack':
            num_states = len(state['states'])
            for ndx, group in enumerate(Grouper(num_states // 2,
                                        state['states'])):
              if test_prev:
                f.write("  if (")
              args = []
              for place, item in enumerate(group):
                item_name = CachedStateName(item)
                args.append('%s' % item_name)
                if test_prev:
                  if place > 0:
                    f.write(' ||\n')
                  f.write("(%s != prev_state->%s)" % (item_name, item_name))
              if test_prev:
                f.write(")\n")
              f.write(
                  "  api()->gl%sFn(%s, %s);\n" %
                  (state['func'], ('GL_FRONT', 'GL_BACK')[ndx],
                   ", ".join(args)))
          elif state['type'] == 'NamedParameter':
            for item in state['states']:
              item_name = CachedStateName(item)

              operation = []
              if test_prev:
                if isinstance(item['default'], list):
                  operation.append("  if (memcmp(prev_state->%s, %s, "
                                      "sizeof(%s) * %d)) {\n" %
                                      (item_name, item_name, item['type'],
                                      len(item['default'])))
                else:
                  operation.append("  if (prev_state->%s != %s) {\n  " %
                                      (item_name, item_name))

              operation.append("  api()->gl%sFn(%s, %s);\n" %
                             (state['func'],
                             (item['enum_set']
                                 if 'enum_set' in item else item['enum']),
                             item['name']))

              if test_prev:
                operation.append("  }")

              guarded_operation = GuardState(item, ''.join(operation),
                                             "feature_info_")
              f.write(guarded_operation)
          else:
            if 'extension_flag' in state:
              f.write("  if (feature_info_->feature_flags().%s)\n  " %
                         state['extension_flag'])
            if test_prev:
              f.write("  if (")
            args = []
            for place, item in enumerate(state['states']):
              item_name = CachedStateName(item)
              args.append('%s' % item_name)
              if test_prev:
                if place > 0:
                  f.write(' ||\n')
                f.write("(%s != prev_state->%s)" %
                           (item_name, item_name))
            if test_prev:
              f.write("    )\n")
            if 'custom_function' in state:
              f.write("  %s(%s);\n" % (state['func'], ", ".join(args)))
            else:
              f.write("  api()->gl%sFn(%s);\n" % (state['func'],
                                                  ", ".join(args)))

      f.write("  if (prev_state) {")
      WriteStates(True)
      f.write("  } else {")
      WriteStates(False)
      f.write("  }")
      f.write("  InitStateManual(prev_state);")
      f.write("}\n")

      f.write("""bool ContextState::GetEnabled(GLenum cap) const {
  switch (cap) {
""")
      for capability in self.capability_flags:
        f.write("    case GL_%s:\n" % capability['name'].upper())
        f.write("      return enable_flags.%s;\n" % capability['name'])
      f.write("""    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}
""")
      self.WriteContextStateGetters(f, "ContextState")
    self.generated_cpp_filenames.append(filename)

  def WriteClientContextStateImpl(self, filename):
    """Writes the context state client side implementation."""
    comment = "// It is included by client_context_state.cc\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      code = []
      for capability in self.capability_flags:
        if 'extension_flag' in capability:
          continue
        code.append("%s(%s)" %
                    (capability['name'],
                     ('false', 'true')['default' in capability]))
      f.write(
        "ClientContextState::EnableFlags::EnableFlags()\n    : %s {\n}\n" %
        ",\n      ".join(code))
      f.write("\n")

      f.write("""
bool ClientContextState::SetCapabilityState(
    GLenum cap, bool enabled, bool* changed) {
  *changed = false;
  switch (cap) {
""")
      for capability in self.capability_flags:
        if 'extension_flag' in capability:
          continue
        f.write("    case GL_%s:\n" % capability['name'].upper())
        f.write("""      if (enable_flags.%(name)s != enabled) {
         *changed = true;
         enable_flags.%(name)s = enabled;
      }
      return true;
""" % capability)
      f.write("""    default:
      return false;
  }
}
""")
      f.write("""bool ClientContextState::GetEnabled(
    GLenum cap, bool* enabled) const {
  switch (cap) {
""")
      for capability in self.capability_flags:
        if 'extension_flag' in capability:
          continue
        f.write("    case GL_%s:\n" % capability['name'].upper())
        f.write("      *enabled = enable_flags.%s;\n" % capability['name'])
        f.write("      return true;\n")
      f.write("""    default:
      return false;
  }
}
""")
    self.generated_cpp_filenames.append(filename)

  def WriteServiceImplementation(self, filename):
    """Writes the service decoder implementation."""
    comment = "// It is included by %s_cmd_decoder.cc\n" % _lower_prefix
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.functions:
        func.WriteServiceImplementation(f)
      if self.capability_flags and _prefix == 'GLES2':
        f.write("""
bool GLES2DecoderImpl::SetCapabilityState(GLenum cap, bool enabled) {
  switch (cap) {
""")
        for capability in self.capability_flags:
          f.write("    case GL_%s:\n" % capability['name'].upper())
          if 'on_change' in capability:

            f.write("""\
              state_.enable_flags.%(name)s = enabled;
              if (state_.enable_flags.cached_%(name)s != enabled
                  || state_.ignore_cached_state) {
                %(on_change)s
              }
              return false;
              """ % capability)
          else:
            f.write("""\
              state_.enable_flags.%(name)s = enabled;
              if (state_.enable_flags.cached_%(name)s != enabled
                  || state_.ignore_cached_state) {
                state_.enable_flags.cached_%(name)s = enabled;
                return true;
              }
              return false;
              """ % capability)
        f.write("""    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}
""")
    self.generated_cpp_filenames.append(filename)

  def WritePassthroughServiceImplementation(self, filename):
    """Writes the passthrough service decoder implementation."""
    with CWriter(filename, self.year) as f:
      header = """
#include \"gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h\"

namespace gpu {
namespace gles2 {

""";
      f.write(header);

      for func in self.functions:
        func.WritePassthroughServiceImplementation(f)

      footer = """
}  // namespace gles2
}  // namespace gpu

""";
      f.write(footer);
    self.generated_cpp_filenames.append(filename)

  def WriteServiceUnitTests(self, filename_pattern):
    """Writes the service decoder unit tests."""
    num_tests = len(self.functions)
    FUNCTIONS_PER_FILE = 98  # hard code this so it doesn't change.
    count = 0
    for test_num in range(0, num_tests, FUNCTIONS_PER_FILE):
      count += 1
      filename = filename_pattern % count
      comment = "// It is included by %s_cmd_decoder_unittest_%d.cc\n" \
                % (_lower_prefix, count)
      with CHeaderWriter(filename, self.year, comment) as f:
        end = test_num + FUNCTIONS_PER_FILE
        if end > num_tests:
          end = num_tests
        for idx in range(test_num, end):
          func = self.functions[idx]
          test_name = '%sDecoderTest%d' % (_prefix, count)
          if func.IsES3():
            test_name = 'GLES3DecoderTest%d' % count

          # Do any filtering of the functions here, so that the functions
          # will not move between the numbered files if filtering properties
          # are changed.
          if func.GetInfo('extension_flag'):
            continue

          if func.GetInfo('unit_test') != False:
            func.WriteServiceUnitTest(f, {
              'test_name': test_name
            })
      self.generated_cpp_filenames.append(filename)


  def WriteServiceContextStateTestHelpers(self, filename):
    comment = "// It is included by context_state_test_helpers.cc\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      if self.capability_flags:
        f.write(
            """void ContextStateTestHelpers::SetupInitCapabilitiesExpectations(
    MockGL* gl,
    gles2::FeatureInfo* feature_info) {
""")
        for capability in self.capability_flags:
          capability_no_init = 'no_init' in capability and \
              capability['no_init'] == True
          if capability_no_init:
              continue
          capability_es3 = 'es3' in capability and capability['es3'] == True
          if capability_es3:
            continue
          if 'extension_flag' in capability:
            f.write("  if (feature_info->feature_flags().%s) {\n" %
                     capability['extension_flag'])
            f.write("  ")
          f.write("  ExpectEnableDisable(gl, GL_%s, %s);\n" %
                  (capability['name'].upper(),
                   ('false', 'true')['default' in capability]))
          if 'extension_flag' in capability:
            f.write("  }")
        f.write("  if (feature_info->IsES3Capable()) {")
        for capability in self.capability_flags:
          capability_es3 = 'es3' in capability and capability['es3'] == True
          if capability_es3:
            f.write("    ExpectEnableDisable(gl, GL_%s, %s);\n" %
                       (capability['name'].upper(),
                        ('false', 'true')['default' in capability]))
        f.write("""  }
}
""")
      f.write("""
void ContextStateTestHelpers::SetupInitStateExpectations(
    MockGL* gl,
    gles2::FeatureInfo* feature_info,
    const gfx::Size& initial_size) {
""")
      # We need to sort the keys so the expectations match
      for state_name in sorted(_STATE_INFO.keys()):
        state = _STATE_INFO[state_name]
        if state['type'] == 'FrontBack':
          num_states = len(state['states'])
          for ndx, group in enumerate(Grouper(num_states // 2,
                                              state['states'])):
            args = []
            for item in group:
              if 'expected' in item:
                args.append(item['expected'])
              else:
                args.append(item['default'])
            f.write(
                "  EXPECT_CALL(*gl, %s(%s, %s))\n" %
                (state['func'], ('GL_FRONT', 'GL_BACK')[ndx],
                 ", ".join(args)))
            f.write("      .Times(1)\n")
            f.write("      .RetiresOnSaturation();\n")
        elif state['type'] == 'NamedParameter':
          for item in state['states']:
            expect_value = item['default']
            if isinstance(expect_value, list):
              # TODO: Currently we do not check array values.
              expect_value = "_"

            operation = []
            operation.append(
                             "  EXPECT_CALL(*gl, %s(%s, %s))\n" %
                             (state['func'],
                              (item['enum_set']
                                  if 'enum_set' in item else item['enum']),
                              expect_value))
            operation.append("      .Times(1)\n")
            operation.append("      .RetiresOnSaturation();\n")

            guarded_operation = GuardState(item, ''.join(operation),
                                           "feature_info")
            f.write(guarded_operation)
        elif 'no_init' not in state:
          if 'extension_flag' in state:
            f.write("  if (feature_info->feature_flags().%s) {\n" %
                       state['extension_flag'])
            f.write("  ")
          args = []
          for item in state['states']:
            if 'expected' in item:
              args.append(item['expected'])
            else:
              args.append(item['default'])
          # TODO: Currently we do not check array values.
          args = ["_" if isinstance(arg, list) else arg for arg in args]
          if 'custom_function' in state:
            f.write("  SetupInitStateManualExpectationsFor%s(gl, %s);\n" %
                       (state['func'], ", ".join(args)))
          else:
            f.write("  EXPECT_CALL(*gl, %s(%s))\n" %
                       (state['func'], ", ".join(args)))
            f.write("      .Times(1)\n")
            f.write("      .RetiresOnSaturation();\n")
          if 'extension_flag' in state:
            f.write("  }\n")
      f.write("  SetupInitStateManualExpectations(gl, feature_info);\n")
      f.write("}\n")
    self.generated_cpp_filenames.append(filename)

  def WriteServiceUnitTestsForExtensions(self, filename):
    """Writes the service decoder unit tests for functions with extension_flag.

       The functions are special in that they need a specific unit test
       baseclass to turn on the extension.
    """
    functions = [f for f in self.functions if f.GetInfo('extension_flag')]
    comment = "// It is included by gles2_cmd_decoder_unittest_extensions.cc\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in functions:
        if True:
          if func.GetInfo('unit_test') != False:
            extension = ToCamelCase(
              ToGLExtensionString(func.GetInfo('extension_flag')))
            test_name = 'GLES2DecoderTestWith%s' % extension
            if func.IsES3():
              test_name = 'GLES3DecoderTestWith%s' % extension
            func.WriteServiceUnitTest(f, {
              'test_name': test_name
            })
    self.generated_cpp_filenames.append(filename)

  def WriteGLES2Header(self, filename):
    """Writes the GLES2 header."""
    comment = "// This file contains Chromium-specific GLES2 declarations.\n\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.original_functions:
        func.WriteGLES2Header(f)
      f.write("\n")
    self.generated_cpp_filenames.append(filename)

  def WriteGLES2CLibImplementation(self, filename):
    """Writes the GLES2 c lib implementation."""
    comment = "// These functions emulate GLES2 over command buffers.\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.original_functions:
        func.WriteGLES2CLibImplementation(f)
      f.write("""
namespace gles2 {

extern const NameToFunc g_gles2_function_table[] = {
""")
      for func in self.original_functions:
        f.write(
            '  { "gl%s", reinterpret_cast<GLES2FunctionPointer>(gl%s), },\n' %
            (func.name, func.name))
      f.write("""  { nullptr, nullptr, },
};

}  // namespace gles2
""")
    self.generated_cpp_filenames.append(filename)

  def WriteGLES2InterfaceHeader(self, filename):
    """Writes the GLES2 interface header."""
    comment = ("// This file is included by %s_interface.h to declare the\n"
               "// GL api functions.\n" % _lower_prefix)
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.original_functions:
        func.WriteGLES2InterfaceHeader(f)
    self.generated_cpp_filenames.append(filename)

  def WriteGLES2InterfaceStub(self, filename):
    """Writes the GLES2 interface stub header."""
    comment = "// This file is included by gles2_interface_stub.h.\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.original_functions:
        func.WriteGLES2InterfaceStub(f)
    self.generated_cpp_filenames.append(filename)

  def WriteGLES2InterfaceStubImpl(self, filename):
    """Writes the GLES2 interface header."""
    comment = "// This file is included by gles2_interface_stub.cc.\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.original_functions:
        func.WriteGLES2InterfaceStubImpl(f)
    self.generated_cpp_filenames.append(filename)

  def WriteGLES2ImplementationHeader(self, filename):
    """Writes the GLES2 Implementation header."""
    comment = \
      ("// This file is included by %s_implementation.h to declare the\n"
       "// GL api functions.\n" % _lower_prefix)
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.original_functions:
        func.WriteGLES2ImplementationHeader(f)
    self.generated_cpp_filenames.append(filename)

  def WriteGLES2Implementation(self, filename):
    """Writes the GLES2 Implementation."""
    comment = \
      ("// This file is included by %s_implementation.cc to define the\n"
       "// GL api functions.\n" % _lower_prefix)
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.original_functions:
        func.WriteGLES2Implementation(f)
    self.generated_cpp_filenames.append(filename)

  def WriteGLES2TraceImplementationHeader(self, filename):
    """Writes the GLES2 Trace Implementation header."""
    comment = "// This file is included by gles2_trace_implementation.h\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.original_functions:
        func.WriteGLES2TraceImplementationHeader(f)
    self.generated_cpp_filenames.append(filename)

  def WriteGLES2TraceImplementation(self, filename):
    """Writes the GLES2 Trace Implementation."""
    comment = "// This file is included by gles2_trace_implementation.cc\n"
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.original_functions:
        func.WriteGLES2TraceImplementation(f)
    self.generated_cpp_filenames.append(filename)

  def WriteGLES2ImplementationUnitTests(self, filename):
    """Writes the GLES2 helper header."""
    comment = \
      ("// This file is included by %s_implementation.h to declare the\n"
       "// GL api functions.\n" % _lower_prefix)
    with CHeaderWriter(filename, self.year, comment) as f:
      for func in self.original_functions:
        func.WriteGLES2ImplementationUnitTest(f)
    self.generated_cpp_filenames.append(filename)

  def WriteServiceUtilsHeader(self, filename):
    """Writes the gles2 auto generated utility header."""
    with CHeaderWriter(filename, self.year) as f:
      for name in sorted(self.named_type_info.keys()):
        named_type = NamedType(self.named_type_info[name])
        if not named_type.CreateValidator():
          continue
        class_name = ValidatorClassName(name)
        if named_type.IsComplete():
          f.write("""class %(class_name)s {
                      public:
                       bool IsValid(const %(type)s value) const;"""% {
            'class_name': class_name,
            'type': named_type.GetType()
          })
          if named_type.HasES3Values():
            f.write("""%s();
                       void SetIsES3(bool is_es3) { is_es3_ = is_es3; }
                      private:
                       bool is_es3_;""" % class_name)
          f.write("};\n")
          f.write("%s %s;\n\n" %
                     (class_name, ToUnderscore(name)))
        else:
          f.write("ValueValidator<%s> %s;\n" %
                     (named_type.GetType(), ToUnderscore(name)))
      f.write("\n")
    self.generated_cpp_filenames.append(filename)

  def WriteServiceUtilsImplementation(self, filename):
    """Writes the gles2 auto generated utility implementation."""
    with CHeaderWriter(filename, self.year) as f:
      names = sorted(self.named_type_info.keys())
      for name in names:
        named_type = NamedType(self.named_type_info[name])
        class_name = ValidatorClassName(name)
        if not named_type.CreateValidator():
          continue
        if named_type.IsComplete():
          if named_type.HasES3Values():
            f.write("""Validators::%(class_name)s::%(class_name)s()
                         : is_es3_(false) {}""" % { 'class_name': class_name })

          f.write("""bool Validators::%(class_name)s::IsValid(
                         const %(type)s value) const {
                       switch(value) {\n""" % {
            'class_name': class_name,
            'type': named_type.GetType()
          })
          if named_type.GetValidValues():
            for value in named_type.GetValidValues():
                f.write("case %s:\n" % value)
            f.write("return true;\n")
          if named_type.GetValidValuesES3():
            for value in named_type.GetValidValuesES3():
                f.write("case %s:\n" % value)
            f.write("return is_es3_;\n")
          if named_type.GetDeprecatedValuesES3():
            for value in named_type.GetDeprecatedValuesES3():
                f.write("case %s:\n" % value)
            f.write("return !is_es3_;\n")
          f.write("}\nreturn false;\n}\n")
          f.write("\n")
        else:
          if named_type.GetValidValues():
            f.write("static const %s valid_%s_table[] = {\n" %
                       (named_type.GetType(), ToUnderscore(name)))
            for value in named_type.GetValidValues():
              f.write("  %s,\n" % value)
            f.write("};\n")
            f.write("\n")
          if named_type.GetValidValuesES3():
            f.write("static const %s valid_%s_table_es3[] = {\n" %
                       (named_type.GetType(), ToUnderscore(name)))
            for value in named_type.GetValidValuesES3():
              f.write("  %s,\n" % value)
            f.write("};\n")
            f.write("\n")
          if named_type.GetDeprecatedValuesES3():
            f.write("static const %s deprecated_%s_table_es3[] = {\n" %
                       (named_type.GetType(), ToUnderscore(name)))
            for value in named_type.GetDeprecatedValuesES3():
              f.write("  %s,\n" % value)
            f.write("};\n")
          f.write("\n")
      f.write("Validators::Validators()")
      pre = '    : '
      for name in names:
        named_type = NamedType(self.named_type_info[name])
        if not named_type.CreateValidator() or named_type.IsComplete():
          continue
        if named_type.GetValidValues():
          code = """%(pre)s%(name)s(
            valid_%(name)s_table, std::size(valid_%(name)s_table))"""
        else:
          code = "%(pre)s%(name)s()"
        f.write(code % {
          'name': ToUnderscore(name),
          'pre': pre,
        })
        pre = ',\n    '
      f.write(" {\n");
      f.write("}\n\n");

      if _prefix == 'GLES2':
        f.write("void Validators::UpdateValuesES3() {\n")
        for name in names:
          named_type = NamedType(self.named_type_info[name])
          if not named_type.IsConstant() and named_type.IsComplete():
            if named_type.HasES3Values():
              f.write("  %(name)s.SetIsES3(true);" % {
                'name': ToUnderscore(name),
              })
            continue
          if named_type.GetDeprecatedValuesES3():
            code = """  %(name)s.RemoveValues(
      deprecated_%(name)s_table_es3, std::size(deprecated_%(name)s_table_es3));
"""
            f.write(code % {
              'name': ToUnderscore(name),
            })
          if named_type.GetValidValuesES3():
            code = """  %(name)s.AddValues(
      valid_%(name)s_table_es3, std::size(valid_%(name)s_table_es3));
"""
            f.write(code % {
              'name': ToUnderscore(name),
            })
        f.write("}\n\n");

        f.write("void Validators::UpdateETCCompressedTextureFormats() {\n")
        for name in ['CompressedTextureFormat', 'TextureInternalFormatStorage']:
          for fmt in _ETC_COMPRESSED_TEXTURE_FORMATS:
            code = """  %(name)s.AddValue(%(format)s);
"""
            f.write(code % {
              'name': ToUnderscore(name),
              'format': fmt,
            })
        f.write("}\n\n");
    self.generated_cpp_filenames.append(filename)

  def WriteCommonUtilsHeader(self, filename):
    """Writes the gles2 common utility header."""
    with CHeaderWriter(filename, self.year) as f:
      type_infos = sorted(self.named_type_info.keys())
      for type_info in type_infos:
        if self.named_type_info[type_info]['type'] == 'GLenum':
          f.write("static std::string GetString%s(uint32_t value);\n" %
                     type_info)
      f.write("\n")
    self.generated_cpp_filenames.append(filename)

  def WriteCommonUtilsImpl(self, filename):
    """Writes the gles2 common utility header."""
    with CHeaderWriter(filename, self.year) as f:
      enums = sorted(self.named_type_info.keys())
      for enum in enums:
        if self.named_type_info[enum]['type'] == 'GLenum':
          f.write("std::string %sUtil::GetString%s(uint32_t value) {\n" %
                     (_prefix, enum))
          valid_list = self.named_type_info[enum]['valid']
          if 'valid_es3' in self.named_type_info[enum]:
            for es3_enum in self.named_type_info[enum]['valid_es3']:
              if not es3_enum in valid_list:
                valid_list.append(es3_enum)
          assert len(valid_list) == len(set(valid_list))
          if len(valid_list) > 0:
            f.write("  static const EnumToString string_table[] = {\n")
            for value in valid_list:
              f.write('    { %s, "%s" },\n' % (value, value))
            f.write("""  };
  return %sUtil::GetQualifiedEnumString(
      string_table, std::size(string_table), value);
}

""" % _prefix)
          else:
            f.write("""  return %sUtil::GetQualifiedEnumString(
      nullptr, 0, value);
}

""" % _prefix)
    self.generated_cpp_filenames.append(filename)

  def WritePepperGLES2Interface(self, filename, dev):
    """Writes the Pepper OpenGLES interface definition."""
    with CWriter(filename, self.year) as f:
      f.write("label Chrome {\n")
      f.write("  M39 = 1.0\n")
      f.write("};\n\n")

      if not dev:
        # Declare GL types.
        f.write("[version=1.0]\n")
        f.write("describe {\n")
        for gltype in ['GLbitfield', 'GLboolean', 'GLbyte', 'GLclampf',
                       'GLclampx', 'GLenum', 'GLfixed', 'GLfloat', 'GLint',
                       'GLintptr', 'GLshort', 'GLsizei', 'GLsizeiptr',
                       'GLubyte', 'GLuint', 'GLushort']:
          f.write("  %s;\n" % gltype)
          f.write("  %s_ptr_t;\n" % gltype)
        f.write("};\n\n")

      # C level typedefs.
      f.write("#inline c\n")
      f.write("#include \"ppapi/c/pp_resource.h\"\n")
      if dev:
        f.write("#include \"ppapi/c/ppb_opengles2.h\"\n\n")
      else:
        f.write("\n#ifndef __gl2_h_\n")
        for (k, v) in _GL_TYPES.items():
          f.write("typedef %s %s;\n" % (v, k))
        f.write("#ifdef _WIN64\n")
        for (k, v) in _GL_TYPES_64.items():
          f.write("typedef %s %s;\n" % (v, k))
        f.write("#else\n")
        for (k, v) in _GL_TYPES_32.items():
          f.write("typedef %s %s;\n" % (v, k))
        f.write("#endif  // _WIN64\n")
        f.write("#endif  // __gl2_h_\n\n")
      f.write("#endinl\n")

      for interface in self.pepper_interfaces:
        if interface.dev != dev:
          continue
        # Historically, we provide OpenGLES2 interfaces with struct
        # namespace. Not to break code which uses the interface as
        # "struct OpenGLES2", we put it in struct namespace.
        f.write('\n[macro="%s", force_struct_namespace]\n' %
                   interface.GetInterfaceName())
        f.write("interface %s {\n" % interface.GetStructName())
        for func in self.original_functions:
          if not func.InPepperInterface(interface):
            continue

          ret_type = func.MapCTypeToPepperIdlType(func.return_type,
                                                  is_for_return_type=True)
          func_prefix = "  %s %s(" % (ret_type, func.GetPepperName())
          f.write(func_prefix)
          f.write("[in] PP_Resource context")
          for arg in func.MakeTypedPepperIdlArgStrings():
            f.write(",\n" + " " * len(func_prefix) + arg)
          f.write(");\n")
        f.write("};\n\n")

  def WritePepperGLES2Implementation(self, filename):
    """Writes the Pepper OpenGLES interface implementation."""
    with CWriter(filename, self.year) as f:
      f.write("#include \"ppapi/shared_impl/ppb_opengles2_shared.h\"\n\n")
      f.write("#include \"base/logging.h\"\n")
      f.write("#include \"gpu/command_buffer/client/gles2_implementation.h\"\n")
      f.write("#include \"ppapi/shared_impl/ppb_graphics_3d_shared.h\"\n")
      f.write("#include \"ppapi/thunk/enter.h\"\n\n")

      f.write("namespace ppapi {\n\n")
      f.write("namespace {\n\n")

      f.write("typedef thunk::EnterResource<thunk::PPB_Graphics3D_API>"
                 " Enter3D;\n\n")

      f.write("gpu::gles2::GLES2Implementation* ToGles2Impl(Enter3D*"
                 " enter) {\n")
      f.write("  DCHECK(enter);\n")
      f.write("  DCHECK(enter->succeeded());\n")
      f.write("  return static_cast<PPB_Graphics3D_Shared*>(enter->object())->"
                 "gles2_impl();\n");
      f.write("}\n\n");

      for func in self.original_functions:
        if not func.InAnyPepperExtension():
          continue

        original_arg = func.MakeTypedPepperArgString("")
        context_arg = "PP_Resource context_id"
        if len(original_arg):
          arg = context_arg + ", " + original_arg
        else:
          arg = context_arg
        f.write("%s %s(%s) {\n" %
                   (func.return_type, func.GetPepperName(), arg))
        f.write("  Enter3D enter(context_id, true);\n")
        f.write("  if (enter.succeeded()) {\n")

        return_str = "" if func.return_type == "void" else "return "
        f.write("    %sToGles2Impl(&enter)->%s(%s);\n" %
                   (return_str, func.original_name,
                    func.MakeOriginalArgString("")))
        f.write("  }")
        if func.return_type == "void":
          f.write("\n")
        else:
          f.write(" else {\n")
          f.write("    return %s;\n" % func.GetErrorReturnString())
          f.write("  }\n")
        f.write("}\n\n")

      f.write("}  // namespace\n")

      for interface in self.pepper_interfaces:
        f.write("const %s* PPB_OpenGLES2_Shared::Get%sInterface() {\n" %
                   (interface.GetStructName(), interface.GetName()))
        f.write("  static const struct %s "
                   "ppb_opengles2 = {\n" % interface.GetStructName())
        f.write("    &")
        f.write(",\n    &".join(
          f.GetPepperName() for f in self.original_functions
            if f.InPepperInterface(interface)))
        f.write("\n")

        f.write("  };\n")
        f.write("  return &ppb_opengles2;\n")
        f.write("}\n")

      f.write("}  // namespace ppapi\n")
    self.generated_cpp_filenames.append(filename)

  def WriteGLES2ToPPAPIBridge(self, filename):
    """Connects GLES2 helper library to PPB_OpenGLES2 interface"""
    with CWriter(filename, self.year) as f:
      f.write("#ifndef GL_GLEXT_PROTOTYPES\n")
      f.write("#define GL_GLEXT_PROTOTYPES\n")
      f.write("#endif\n")
      f.write("#include <GLES2/gl2.h>\n")
      f.write("#include <GLES2/gl2ext.h>\n")
      f.write("#include \"ppapi/lib/gl/gles2/gl2ext_ppapi.h\"\n\n")

      for func in self.original_functions:
        if not func.InAnyPepperExtension():
          continue

        interface = self.interface_info[func.GetInfo('pepper_interface') or '']

        f.write("%s GL_APIENTRY gl%s(%s) {\n" %
                   (func.return_type, func.GetPepperName(),
                    func.MakeTypedPepperArgString("")))
        return_str = "" if func.return_type == "void" else "return "
        interface_str = "glGet%sInterfacePPAPI()" % interface.GetName()
        original_arg = func.MakeOriginalArgString("")
        context_arg = "glGetCurrentContextPPAPI()"
        if len(original_arg):
          arg = context_arg + ", " + original_arg
        else:
          arg = context_arg
        if interface.GetName():
          f.write("  const struct %s* ext = %s;\n" %
                     (interface.GetStructName(), interface_str))
          f.write("  if (ext)\n")
          f.write("    %sext->%s(%s);\n" %
                     (return_str, func.GetPepperName(), arg))
          if return_str:
            f.write("  %s0;\n" % return_str)
        else:
          f.write("  %s%s->%s(%s);\n" %
                     (return_str, interface_str, func.GetPepperName(), arg))
        f.write("}\n\n")
    self.generated_cpp_filenames.append(filename)


def Format(generated_files, output_dir, chromium_root_dir):
  """Format generated_files relative to output_dir using clang-format."""
  formatter = "third_party/depot_tools/clang-format"
  if platform.system() == "Windows":
    formatter = "third_party\\depot_tools\\clang-format.bat"
  formatter = os.path.join(chromium_root_dir, formatter)
  generated_files = map(lambda filename: os.path.join(output_dir, filename),
                        generated_files)
  for filename in generated_files:
    call([formatter, "-i", "-style=chromium", filename], cwd=chromium_root_dir)
