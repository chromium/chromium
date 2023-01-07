// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gles2_conform_support/egl/config.h"
#include "base/check.h"

namespace gles2_conform_support {
namespace egl {

Config::Config(EGLint surface_type)
    : buffer_size_(0),
      red_size_(0),
      green_size_(0),
      blue_size_(0),
      luminance_size_(0),
      alpha_size_(0),
      alpha_mask_size_(0),
      bind_to_texture_rgb_(EGL_FALSE),
      bind_to_texture_rgba_(EGL_FALSE),
      color_buffer_type_(EGL_RGB_BUFFER),
      config_caveat_(EGL_NONE),
      config_id_(EGL_DONT_CARE),
      conformant_(EGL_OPENGL_ES2_BIT),
      depth_size_(0),
      level_(0),
      max_pbuffer_width_(0),
      max_pbuffer_height_(0),
      max_pbuffer_pixels_(0),
      min_swap_interval_(EGL_DONT_CARE),
      max_swap_interval_(EGL_DONT_CARE),
      native_renderable_(EGL_TRUE),
      native_visual_id_(0),
      native_visual_type_(EGL_DONT_CARE),
      renderable_type_(EGL_OPENGL_ES2_BIT),
      sample_buffers_(0),
      samples_(0),
      stencil_size_(0),
      surface_type_(surface_type),
      transparent_type_(EGL_NONE),
      transparent_red_value_(EGL_DONT_CARE),
      transparent_green_value_(EGL_DONT_CARE),
      transparent_blue_value_(EGL_DONT_CARE) {
  DCHECK(surface_type == EGL_WINDOW_BIT || surface_type == EGL_PBUFFER_BIT);
}

Config::~Config() = default;

bool Config::Matches(const EGLint* attrib_list) const {
  DCHECK(ValidateAttributeList(attrib_list));
  if (attrib_list) {
    for (int i = 0; attrib_list[i] != EGL_NONE; i += 2) {
      switch (attrib_list[i]) {
        case EGL_SURFACE_TYPE: {
          EGLint requested_surface_type = attrib_list[i + 1];
          if (requested_surface_type != EGL_DONT_CARE &&
              (requested_surface_type & surface_type_) !=
                  requested_surface_type)
            return false;
          break;
        }
        default:
          break;
      }
    }
  }
  return true;
}

bool Config::GetAttrib(EGLint attribute, EGLint* value) const {
  // TODO(alokp): Find out how to get correct values.
  switch (attribute) {
    case EGL_BUFFER_SIZE:
      *value = buffer_size_;
      break;
    case EGL_RED_SIZE:
      *value = red_size_;
      break;
    case EGL_GREEN_SIZE:
      *value = green_size_;
      break;
    case EGL_BLUE_SIZE:
      *value = blue_size_;
      break;
    case EGL_LUMINANCE_SIZE:
      *value = luminance_size_;
      break;
    case EGL_ALPHA_SIZE:
      *value = alpha_size_;
      break;
    case EGL_ALPHA_MASK_SIZE:
      *value = alpha_mask_size_;
      break;
    case EGL_BIND_TO_TEXTURE_RGB:
      *value = bind_to_texture_rgb_;
      break;
    case EGL_BIND_TO_TEXTURE_RGBA:
      *value = bind_to_texture_rgba_;
      break;
    case EGL_COLOR_BUFFER_TYPE:
      *value = color_buffer_type_;
      break;
    case EGL_CONFIG_CAVEAT:
      *value = config_caveat_;
      break;
    case EGL_CONFIG_ID:
      *value = config_id_;
      break;
    case EGL_CONFORMANT:
      *value = conformant_;
      break;
    case EGL_DEPTH_SIZE:
      *value = depth_size_;
      break;
    case EGL_LEVEL:
      *value = level_;
      break;
    case EGL_MAX_PBUFFER_WIDTH:
      *value = max_pbuffer_width_;
      break;
    case EGL_MAX_PBUFFER_HEIGHT:
      *value = max_pbuffer_height_;
      break;
    case EGL_MAX_PBUFFER_PIXELS:
      *value = max_pbuffer_pixels_;
      break;
    case EGL_MIN_SWAP_INTERVAL:
      *value = min_swap_interval_;
      break;
    case EGL_MAX_SWAP_INTERVAL:
      *value = max_swap_interval_;
      break;
    case EGL_NATIVE_RENDERABLE:
      *value = native_renderable_;
      break;
    case EGL_NATIVE_VISUAL_ID:
      *value = native_visual_id_;
      break;
    case EGL_NATIVE_VISUAL_TYPE:
      *value = native_visual_type_;
      break;
    case EGL_RENDERABLE_TYPE:
      *value = renderable_type_;
      break;
    case EGL_SAMPLE_BUFFERS:
      *value = sample_buffers_;
      break;
    case EGL_SAMPLES:
      *value = samples_;
      break;
    case EGL_STENCIL_SIZE:
      *value = stencil_size_;
      break;
    case EGL_SURFACE_TYPE:
      *value = surface_type_;
      break;
    case EGL_TRANSPARENT_TYPE:
      *value = transparent_type_;
      break;
    case EGL_TRANSPARENT_RED_VALUE:
      *value = transparent_red_value_;
      break;
    case EGL_TRANSPARENT_GREEN_VALUE:
      *value = transparent_green_value_;
      break;
    case EGL_TRANSPARENT_BLUE_VALUE:
      *value = transparent_blue_value_;
      break;
    default:
      return false;
  }
  return true;
}

bool Config::ValidateAttributeList(const EGLint* attrib_list) {
  if (attrib_list) {
    for (int i = 0; attrib_list[i] != EGL_NONE; i += 2) {
      switch (attrib_list[i]) {
        case EGL_ALPHA_MASK_SIZE:
        case EGL_ALPHA_SIZE:
        case EGL_BIND_TO_TEXTURE_RGB:
        case EGL_BIND_TO_TEXTURE_RGBA:
        case EGL_BLUE_SIZE:
        case EGL_BUFFER_SIZE:
        case EGL_COLOR_BUFFER_TYPE:
        case EGL_CONFIG_CAVEAT:
        case EGL_CONFIG_ID:
        case EGL_CONFORMANT:
        case EGL_DEPTH_SIZE:
        case EGL_GREEN_SIZE:
        case EGL_LEVEL:
        case EGL_LUMINANCE_SIZE:
        case EGL_MATCH_NATIVE_PIXMAP:
        case EGL_NATIVE_RENDERABLE:
        case EGL_MAX_SWAP_INTERVAL:
        case EGL_MIN_SWAP_INTERVAL:
        case EGL_RED_SIZE:
        case EGL_SAMPLE_BUFFERS:
        case EGL_SAMPLES:
        case EGL_STENCIL_SIZE:
        case EGL_RENDERABLE_TYPE:
        case EGL_SURFACE_TYPE:
        case EGL_MULTISAMPLE_RESOLVE_BOX_BIT:
        case EGL_PBUFFER_BIT:
        case EGL_PIXMAP_BIT:
        case EGL_SWAP_BEHAVIOR_PRESERVED_BIT:
        case EGL_VG_ALPHA_FORMAT_PRE_BIT:
        case EGL_VG_COLORSPACE_LINEAR_BIT:
        case EGL_WINDOW_BIT:
        case EGL_TRANSPARENT_TYPE:
        case EGL_TRANSPARENT_RED_VALUE:
        case EGL_TRANSPARENT_GREEN_VALUE:
        case EGL_TRANSPARENT_BLUE_VALUE:
          break;
        default:
          return false;
      }
    }
  }
  return true;
}

}  // namespace egl
}  // namespace gles2_conform_support