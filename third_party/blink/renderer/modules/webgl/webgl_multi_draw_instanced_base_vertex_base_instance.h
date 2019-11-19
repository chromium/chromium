// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_INSTANCED_BASE_VERTEX_BASE_INSTANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_INSTANCED_BASE_VERTEX_BASE_INSTANCE_H_

#include "third_party/blink/renderer/bindings/modules/v8/int32_array_or_long_sequence.h"
#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"
#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw_common.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class WebGLMultiDrawInstancedBaseVertexBaseInstance final
    : public WebGLExtension,
      public WebGLMultiDrawCommon {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebGLMultiDrawInstancedBaseVertexBaseInstance* Create(
      WebGLRenderingContextBase*);
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  explicit WebGLMultiDrawInstancedBaseVertexBaseInstance(
      WebGLRenderingContextBase*);
  WebGLExtensionName GetName() const override;

  void multiDrawArraysInstancedBaseInstanceWEBGL(
      GLenum mode,
      Int32ArrayOrLongSequence firsts_list,
      GLuint firsts_offset,
      Int32ArrayOrLongSequence counts_list,
      GLuint counts_offset,
      Int32ArrayOrLongSequence instance_counts_list,
      GLuint instance_counts_offset,
      Int32ArrayOrLongSequence baseinstances_list,
      GLuint baseinstances_offset,
      GLsizei drawcount) {
    multiDrawArraysInstancedBaseInstanceImpl(
        mode, MakeSpan(firsts_list), firsts_offset, MakeSpan(counts_list),
        counts_offset, MakeSpan(instance_counts_list), instance_counts_offset,
        MakeSpan(baseinstances_list), baseinstances_offset, drawcount);
  }

  void multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL(
      GLenum mode,
      Int32ArrayOrLongSequence counts_list,
      GLuint counts_offset,
      GLenum type,
      Int32ArrayOrLongSequence offsets_list,
      GLuint offsets_offset,
      Int32ArrayOrLongSequence instance_counts_list,
      GLuint instance_counts_offset,
      Int32ArrayOrLongSequence basevertices_list,
      GLuint basevertices_offset,
      Int32ArrayOrLongSequence baseinstances_list,
      GLuint baseinstances_offset,
      GLsizei drawcount) {
    multiDrawElementsInstancedBaseVertexBaseInstanceImpl(
        mode, MakeSpan(counts_list), counts_offset, type,
        MakeSpan(offsets_list), offsets_offset, MakeSpan(instance_counts_list),
        instance_counts_offset, MakeSpan(basevertices_list),
        basevertices_offset, MakeSpan(baseinstances_list), baseinstances_offset,
        drawcount);
  }

 private:
  void multiDrawArraysInstancedBaseInstanceImpl(
      GLenum mode,
      const base::span<const int32_t> firsts,
      GLuint firsts_offset,
      const base::span<const int32_t> counts,
      GLuint counts_offset,
      const base::span<const int32_t> instance_counts,
      GLuint instance_counts_offset,
      const base::span<const int32_t> baseinstances,
      GLuint baseinstances_offset,
      GLsizei drawcount);

  void multiDrawElementsInstancedBaseVertexBaseInstanceImpl(
      GLenum mode,
      const base::span<const int32_t> counts,
      GLuint counts_offset,
      GLenum type,
      const base::span<const int32_t> offsets,
      GLuint offsets_offset,
      const base::span<const int32_t> instance_counts,
      GLuint instance_counts_offset,
      const base::span<const int32_t> basevertices,
      GLuint basevertices_offset,
      const base::span<const int32_t> baseinstances,
      GLuint baseinstances_offset,
      GLsizei drawcount);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_INSTANCED_BASE_VERTEX_BASE_INSTANCE_H_
