// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MULTI_DRAW_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_MULTI_DRAW_MANAGER_H_

#include <vector>

#include "gpu/gpu_gles2_export.h"

// Forwardly declare a few GL types to avoid including GL header files.
typedef unsigned GLenum;
typedef int GLsizei;
typedef int GLint;
typedef unsigned int GLuint;

namespace gpu {
namespace gles2 {

class GPU_GLES2_EXPORT MultiDrawManager {
 public:
  enum class DrawFunction {
    DrawArrays,
    DrawArraysInstanced,
    DrawArraysInstancedBaseInstance,
    DrawElements,
    DrawElementsInstanced,
    DrawElementsInstancedBaseVertexBaseInstance,
  };

  struct GPU_GLES2_EXPORT ResultData {
    DrawFunction draw_function;
    GLsizei drawcount = 0;
    GLenum mode = 0;
    GLenum type = 0;
    std::vector<GLint> firsts;
    std::vector<GLsizei> counts;
    std::vector<GLsizei> offsets;
    std::vector<const void*> indices;
    std::vector<GLsizei> instance_counts;
    std::vector<GLint> basevertices;
    std::vector<GLuint> baseinstances;

    ResultData();
    ~ResultData();
    ResultData(ResultData&& rhs);
    ResultData& operator=(ResultData&& rhs);
  };

  enum class IndexStorageType {
    Offset,
    Pointer,
  };

  MultiDrawManager(IndexStorageType index_type);

  bool Begin(GLsizei drawcount);
  bool End(ResultData* result);
  bool MultiDrawArrays(GLenum mode,
                       const GLint* firsts,
                       const GLsizei* counts,
                       GLsizei drawcount);
  bool MultiDrawArraysInstanced(GLenum mode,
                                const GLint* firsts,
                                const GLsizei* counts,
                                const GLsizei* instance_counts,
                                GLsizei drawcount);
  bool MultiDrawArraysInstancedBaseInstance(GLenum mode,
                                            const GLint* firsts,
                                            const GLsizei* counts,
                                            const GLsizei* instance_counts,
                                            const GLuint* baseinstances,
                                            GLsizei drawcount);
  bool MultiDrawElements(GLenum mode,
                         const GLsizei* counts,
                         GLenum type,
                         const GLsizei* offsets,
                         GLsizei drawcount);
  bool MultiDrawElementsInstanced(GLenum mode,
                                  const GLsizei* counts,
                                  GLenum type,
                                  const GLsizei* offsets,
                                  const GLsizei* instance_counts,
                                  GLsizei drawcount);
  bool MultiDrawElementsInstancedBaseVertexBaseInstance(
      GLenum mode,
      const GLsizei* counts,
      GLenum type,
      const GLsizei* offsets,
      const GLsizei* instance_counts,
      const GLint* basevertices,
      const GLuint* baseinstances,
      GLsizei drawcount);

 private:
  void ResizeArrays();
  bool ValidateDrawcount(GLsizei drawcount) const;
  bool EnsureDrawArraysFunction(DrawFunction draw_function,
                                GLenum mode,
                                GLsizei drawcount);
  bool EnsureDrawElementsFunction(DrawFunction draw_function,
                                  GLenum mode,
                                  GLenum type,
                                  GLsizei drawcount);
  void CopyArraysHelper(GLsizei drawcount,
                        const GLint* firsts,
                        const GLsizei* counts,
                        const GLsizei* offsets,
                        const GLsizei* instance_counts,
                        const GLint* basevertices,
                        const GLuint* baseinstances);

  enum class DrawState {
    Begin,
    Draw,
    End,
  };

  DrawState draw_state_;
  GLsizei current_draw_offset_;
  IndexStorageType index_type_;
  ResultData result_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MULTI_DRAW_MANAGER_H_
