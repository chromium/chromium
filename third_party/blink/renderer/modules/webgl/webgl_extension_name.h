// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_EXTENSION_NAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_EXTENSION_NAME_H_

namespace blink {

// Extension names are needed to properly wrap instances in JavaScript objects.
enum WebGLExtensionName {
  kANGLEInstancedArraysName,
  kEXTBlendMinMaxName,
  kEXTColorBufferFloatName,
  kEXTColorBufferHalfFloatName,
  kEXTDisjointTimerQueryName,
  kEXTDisjointTimerQueryWebGL2Name,
  kEXTFloatBlendName,
  kEXTFragDepthName,
  kEXTShaderTextureLODName,
  kEXTsRGBName,
  kEXTTextureCompressionBPTCName,
  kEXTTextureCompressionRGTCName,
  kEXTTextureFilterAnisotropicName,
  kEXTTextureNorm16Name,
  kKHRParallelShaderCompileName,
  kOESDrawBuffersIndexedName,
  kOESElementIndexUintName,
  kOESFboRenderMipmapName,
  kOESStandardDerivativesName,
  kOESTextureFloatLinearName,
  kOESTextureFloatName,
  kOESTextureHalfFloatLinearName,
  kOESTextureHalfFloatName,
  kOESVertexArrayObjectName,
  kOVRMultiview2Name,
  kWebGLClipCullDistanceName,
  kWebGLColorBufferFloatName,
  kWebGLCompressedTextureASTCName,
  kWebGLCompressedTextureETCName,
  kWebGLCompressedTextureETC1Name,
  kWebGLCompressedTexturePVRTCName,
  kWebGLCompressedTextureS3TCName,
  kWebGLCompressedTextureS3TCsRGBName,
  kWebGLDebugRendererInfoName,
  kWebGLDebugShadersName,
  kWebGLDepthTextureName,
  kWebGLDrawBuffersName,
  kWebGLDrawInstancedBaseVertexBaseInstanceName,
  kWebGLGetBufferSubDataAsyncName,
  kWebGLLoseContextName,
  kWebGLMultiDrawName,
  kWebGLMultiDrawInstancedName,
  kWebGLMultiDrawInstancedBaseVertexBaseInstanceName,
  kWebGLMultiviewName,
  kWebGLProvokingVertexName,
  kWebGLVideoTextureName,
  kWebGLWebCodecsVideoFrameName,
  kWebGLExtensionNameCount,  // Must be the last entry
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_EXTENSION_NAME_H_
