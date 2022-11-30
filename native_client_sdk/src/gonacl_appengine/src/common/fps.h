// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GONACL_APPENGINE_SRC_COMMON_FPS_H_
#define GONACL_APPENGINE_SRC_COMMON_FPS_H_

#include <sys/time.h>

// Timer helper for fps.  Returns seconds elapsed since first call to
// getseconds(), as a double.
static inline double getseconds() {
  static int first_call = 1;
  static struct timeval start_tv;
  static int start_tv_retv;
  const double usec_to_sec = 0.000001;

  if (first_call) {
    first_call = 0;
    start_tv_retv = gettimeofday(&start_tv, NULL);
  }

  struct timeval tv;
  if ((0 == start_tv_retv) && (0 == gettimeofday(&tv, NULL)))
    return (tv.tv_sec - start_tv.tv_sec) + tv.tv_usec * usec_to_sec;
  return 0.0;
}

struct FpsState {
  double last_time;
  int frame_count;
};

/**
 * Initialize the FpsState object.
 */
inline void FpsInit(struct FpsState* state) {
  state->last_time = getseconds();
  state->frame_count = 0;
}

/**
 * Call this whenever you render, after calling FpsInit above.
 *
 * Returns 1 if the value should be displayed. In this case, the result will
 * be written to the |out_fps| parameter.
 */
inline int FpsStep(struct FpsState* state, double* out_fps) {
  const double kFpsUpdateSecs = 1.0f;
  double current_time = getseconds();

  state->frame_count++;

  if (current_time < state->last_time + kFpsUpdateSecs)
    return 0;

  *out_fps = state->frame_count / (current_time - state->last_time);
  state->last_time = current_time;
  state->frame_count = 0;
  return 1;
}

#endif  // GONACL_APPENGINE_SRC_COMMON_FPS_H_