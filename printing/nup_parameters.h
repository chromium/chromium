// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NUP_PARAMETERS_H_
#define PRINTING_NUP_PARAMETERS_H_

#include "base/component_export.h"

namespace printing {

class COMPONENT_EXPORT(PRINTING) NupParameters {
 public:
  // Callers should check `pages_per_sheet` values with IsSupported() first,
  // and only pass in supported values.
  NupParameters(int pages_per_sheet, bool is_source_landscape);

  // Whether or not the input `pages_per_sheet` is a supported N-up value.
  // Supported values are: 1 2 4 6 9 16
  static bool IsSupported(int pages_per_sheet);

  // Orientation of the to-be-generated N-up PDF document.
  bool landscape() const { return landscape_; }
  int num_pages_on_x_axis() const { return num_pages_on_x_axis_; }
  int num_pages_on_y_axis() const { return num_pages_on_y_axis_; }

 private:
  // Calculates the `num_pages_on_x_axis_`, `num_pages_on_y_axis_` and
  // `landscape_` based on the input.
  void SetParameters(int pages_per_sheet, bool is_source_landscape);

  int num_pages_on_x_axis_ = 1;
  int num_pages_on_y_axis_ = 1;
  bool landscape_ = false;
};

}  // namespace printing

#endif  // PRINTING_NUP_PARAMETERS_H_
