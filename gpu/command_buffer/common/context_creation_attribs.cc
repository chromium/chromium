// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/context_creation_attribs.h"

#include "base/notreached.h"

namespace gpu {

bool IsGLContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_WEBGL2:
    case CONTEXT_TYPE_OPENGLES31_FOR_TESTING:
      return true;
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool IsWebGLContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_WEBGL2:
      return true;
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_OPENGLES31_FOR_TESTING:
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool IsWebGL1OrES2ContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_OPENGLES2:
      return true;
    case CONTEXT_TYPE_WEBGL2:
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_OPENGLES31_FOR_TESTING:
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool IsWebGL2OrES3ContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGL2:
      return true;
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_OPENGLES31_FOR_TESTING:
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool IsWebGL2OrES3OrHigherContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGL2:
    case CONTEXT_TYPE_OPENGLES31_FOR_TESTING:
      return true;
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool IsES31ForTestingContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_OPENGLES31_FOR_TESTING:
      return true;
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGL2:
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool IsWebGPUContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_WEBGPU:
      return true;
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_WEBGL2:
    case CONTEXT_TYPE_OPENGLES31_FOR_TESTING:
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

const char* ContextTypeToLabel(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_OPENGLES2:
      return "OPENGLES2";
    case CONTEXT_TYPE_OPENGLES3:
      return "OPENGLES3";
    case CONTEXT_TYPE_WEBGL1:
      return "WEBGL1";
    case CONTEXT_TYPE_WEBGL2:
      return "WEBGL2";
    case CONTEXT_TYPE_OPENGLES31_FOR_TESTING:
      return "GLES31_FOR_TESTING";
    case CONTEXT_TYPE_WEBGPU:
      return "WEBGPU";
  }

  NOTREACHED_IN_MIGRATION();
  return "BadGLContext";
}

ContextCreationAttribs::ContextCreationAttribs() = default;

ContextCreationAttribs::ContextCreationAttribs(
    const ContextCreationAttribs& other) = default;

ContextCreationAttribs& ContextCreationAttribs::operator=(
    const ContextCreationAttribs& other) = default;

}  // namespace gpu
