// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/extensions_3d_util.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gles2_cmd_copy_texture_chromium_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

namespace {

void SplitStringHelper(const String& str, HashSet<String>& set) {
  Vector<String> substrings;
  str.Split(' ', substrings);
  for (const auto& substring : substrings)
    set.insert(substring);
}

}  // anonymous namespace

std::unique_ptr<Extensions3DUtil> Extensions3DUtil::Create(
    gpu::gles2::GLES2Interface* gl) {
  std::unique_ptr<Extensions3DUtil> out =
      base::WrapUnique(new Extensions3DUtil(gl));
  out->InitializeExtensions();
  return out;
}

Extensions3DUtil::Extensions3DUtil(gpu::gles2::GLES2Interface* gl)
    : gl_(gl), is_valid_(true) {}

Extensions3DUtil::~Extensions3DUtil() = default;

void Extensions3DUtil::InitializeExtensions() {
  if (gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    // If the context is lost don't initialize the extension strings.
    // This will cause supportsExtension, ensureExtensionEnabled, and
    // isExtensionEnabled to always return false.
    is_valid_ = false;
    return;
  }

  String extensions_string(gl_->GetString(GL_EXTENSIONS));
  SplitStringHelper(extensions_string, enabled_extensions_);

  String requestable_extensions_string(gl_->GetRequestableExtensionsCHROMIUM());
  SplitStringHelper(requestable_extensions_string, requestable_extensions_);
}

bool Extensions3DUtil::SupportsExtension(const String& name) {
  return enabled_extensions_.Contains(name) ||
         requestable_extensions_.Contains(name);
}

bool Extensions3DUtil::EnsureExtensionEnabled(const String& name) {
  if (enabled_extensions_.Contains(name))
    return true;

  if (requestable_extensions_.Contains(name)) {
    gl_->RequestExtensionCHROMIUM(name.Ascii().c_str());
    enabled_extensions_.clear();
    requestable_extensions_.clear();
    InitializeExtensions();
  }
  return enabled_extensions_.Contains(name);
}

bool Extensions3DUtil::IsExtensionEnabled(const String& name) {
  return enabled_extensions_.Contains(name);
}

// static
bool Extensions3DUtil::CopyTextureCHROMIUMNeedsESSL3(GLenum dest_format) {
  return gpu::gles2::CopyTextureCHROMIUMNeedsESSL3(dest_format);
}

// static
bool Extensions3DUtil::CanUseCopyTextureCHROMIUM(GLenum dest_target) {
  switch (dest_target) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_RECTANGLE_ARB:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return true;
    default:
      return false;
  }
}

}  // namespace blink
