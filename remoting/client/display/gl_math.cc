// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/client/display/gl_math.h"

#include <sstream>

namespace remoting {

void TransposeTransformationMatrix(std::array<float, 9>* matrix) {
  // | ??, m1, m2, |    | ??, m3, m6 |
  // | m3, ??, m5, | -> | m1, ??, m7 |
  // | m6, m7, ??  |    | m2, m5, ?? |
  std::swap((*matrix)[1], (*matrix)[3]);
  std::swap((*matrix)[2], (*matrix)[6]);
  std::swap((*matrix)[5], (*matrix)[7]);
}

void FillRectangleVertexPositions(float left,
                                  float top,
                                  float width,
                                  float height,
                                  std::array<float, 8>* positions) {
  (*positions)[0] = left;
  (*positions)[1] = top;

  (*positions)[2] = left;
  (*positions)[3] = top + height;

  (*positions)[4] = left + width;
  (*positions)[5] = top;

  (*positions)[6] = left + width;
  (*positions)[7] = top + height;
}

std::string MatrixToString(const float* mat, int num_rows, int num_cols) {
  std::ostringstream outstream;
  outstream << "[\n";
  for (int i = 0; i < num_rows; i++) {
    for (int j = 0; j < num_cols; j++) {
      outstream << mat[i * num_cols + j] << ", ";
    }
    outstream << "\n";
  }
  outstream << "]";
  return outstream.str();
}

}  // namespace remoting
