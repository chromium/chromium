// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_H_

#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"
#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw_common.h"

namespace blink {

class WebGLMultiDraw final : public WebGLExtension,
                             public WebGLMultiDrawCommon {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  explicit WebGLMultiDraw(WebGLRenderingContextBase*);
  WebGLExtensionName GetName() const override;

  void multiDrawArraysWEBGL(
      GLenum mode,
      const V8UnionInt32ArrayAllowSharedOrLongSequence* firstsList,
      GLuint firstsOffset,
      const V8UnionInt32ArrayAllowSharedOrLongSequence* countsList,
      GLuint countsOffset,
      GLsizei drawcount) {
    multiDrawArraysImpl(mode, MakeSpan(firstsList), firstsOffset,
                        MakeSpan(countsList), countsOffset, drawcount);
  }

  void multiDrawElementsWEBGL(
      GLenum mode,
      const V8UnionInt32ArrayAllowSharedOrLongSequence* countsList,
      GLuint countsOffset,
      GLenum type,
      const V8UnionInt32ArrayAllowSharedOrLongSequence* offsetsList,
      GLuint offsetsOffset,
      GLsizei drawcount) {
    multiDrawElementsImpl(mode, MakeSpan(countsList), countsOffset, type,
                          MakeSpan(offsetsList), offsetsOffset, drawcount);
  }

  void multiDrawArraysInstancedWEBGL(
      GLenum mode,
      const V8UnionInt32ArrayAllowSharedOrLongSequence* firstsList,
      GLuint firstsOffset,
      const V8UnionInt32ArrayAllowSharedOrLongSequence* countsList,
      GLuint countsOffset,
      const V8UnionInt32ArrayAllowSharedOrLongSequence* instanceCountsList,
      GLuint instanceCountsOffset,
      GLsizei drawcount) {
    multiDrawArraysInstancedImpl(mode, MakeSpan(firstsList), firstsOffset,
                                 MakeSpan(countsList), countsOffset,
                                 MakeSpan(instanceCountsList),
                                 instanceCountsOffset, drawcount);
  }

  void multiDrawElementsInstancedWEBGL(
      GLenum mode,
      const V8UnionInt32ArrayAllowSharedOrLongSequence* countsList,
      GLuint countsOffset,
      GLenum type,
      const V8UnionInt32ArrayAllowSharedOrLongSequence* offsetsList,
      GLuint offsetsOffset,
      const V8UnionInt32ArrayAllowSharedOrLongSequence* instanceCountsList,
      GLuint instanceCountsOffset,
      GLsizei drawcount) {
    multiDrawElementsInstancedImpl(mode, MakeSpan(countsList), countsOffset,
                                   type, MakeSpan(offsetsList), offsetsOffset,
                                   MakeSpan(instanceCountsList),
                                   instanceCountsOffset, drawcount);
  }

 private:
  void multiDrawArraysImpl(GLenum mode,
                           const base::span<const int32_t>& firsts,
                           GLuint firstsOffset,
                           const base::span<const int32_t>& counts,
                           GLuint countsOffset,
                           GLsizei drawcount);

  void multiDrawElementsImpl(GLenum mode,
                             const base::span<const int32_t>& counts,
                             GLuint countsOffset,
                             GLenum type,
                             const base::span<const int32_t>& offsets,
                             GLuint offsetsOffset,
                             GLsizei drawcount);

  void multiDrawArraysInstancedImpl(
      GLenum mode,
      const base::span<const int32_t>& firsts,
      GLuint firstsOffset,
      const base::span<const int32_t>& counts,
      GLuint countsOffset,
      const base::span<const int32_t>& instanceCounts,
      GLuint instanceCountsOffset,
      GLsizei drawcount);

  void multiDrawElementsInstancedImpl(
      GLenum mode,
      const base::span<const int32_t>& counts,
      GLuint countsOffset,
      GLenum type,
      const base::span<const int32_t>& offsets,
      GLuint offsetsOffset,
      const base::span<const int32_t>& instanceCounts,
      GLuint instanceCountsOffset,
      GLsizei drawcount);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_H_
