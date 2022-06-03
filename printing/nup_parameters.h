// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NUP_PARAMETERS_H_
#define PRINTING_NUP_PARAMETERS_H_

#include "base/component_export.h"

namespace printing {

class COMPONENT_EXPORT(PRINTING) NupParameters {
 public:
  NupParameters();

  // Whether or not the input `pages_per_sheet` is a supported N-up value.
  // Supported values are: 1 2 4 6 9 16
  static bool IsSupported(int pages_per_sheet);

  // Orientation of the to-be-generated N-up PDF document.
  bool landscape() const { return landscape_; }
  int num_pages_on_x_axis() const { return num_pages_on_x_axis_; }
  int num_pages_on_y_axis() const { return num_pages_on_y_axis_; }

  // Calculates the `num_pages_on_x_axis_`, `num_pages_on_y_axis_` and
  // `landscape_` based on the input.  `is_source_landscape` is true if the
  // source document orientation is landscape.
  // Callers should check `pages_per_sheet` values with IsSupported() first,
  // and only pass in supported values.
  void SetParameters(int pages_per_sheet, bool is_source_landscape);

  // Turns off N-up mode by resetting `num_pages_on_x_axis` = 1,
  // `num_pages_on_y_axis` = 1, and `landscape_` = false
  void Clear();

 private:
  int num_pages_on_x_axis_;
  int num_pages_on_y_axis_;
  bool landscape_;
};

}  // namespace printing

#endif  // PRINTING_NUP_PARAMETERS_H_
