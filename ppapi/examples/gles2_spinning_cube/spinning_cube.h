// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_EXAMPLES_GLES2_SPINNING_CUBE_SPINNING_CUBE_H_
#define PPAPI_EXAMPLES_GLES2_SPINNING_CUBE_SPINNING_CUBE_H_

#include "ppapi/c/pp_stdint.h"

class SpinningCube {
 public:
  SpinningCube();
  ~SpinningCube();

  void Init(uint32_t width, uint32_t height);
  void set_direction(int direction) { direction_ = direction; }
  void SetFlingMultiplier(float drag_distance, float drag_time);
  void UpdateForTimeDelta(float delta_time);
  void UpdateForDragDistance(float distance);
  void Draw();

  void OnGLContextLost();

 private:
  class GLState;

  // Disallow copy and assign.
  SpinningCube(const SpinningCube& other);
  SpinningCube& operator=(const SpinningCube& other);

  void Update();

  bool initialized_;
  uint32_t width_;
  uint32_t height_;
  // Owned ptr.
  GLState* state_;
  float fling_multiplier_;
  int direction_;
};

#endif  // PPAPI_EXAMPLES_GLES2_SPINNING_CUBE_SPINNING_CUBE_H_
