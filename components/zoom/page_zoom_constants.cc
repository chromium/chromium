// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zoom/page_zoom_constants.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace zoom {

const double kPresetZoomFactors[] = {0.25, 1 / 3.0, 0.5, 2 / 3.0, 0.75, 0.8,
                                     0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 2.5,
                                     3.0, 4.0, 5.0};
const std::size_t kPresetZoomFactorsSize = std::size(kPresetZoomFactors);

std::string GetPresetZoomFactorsAsJSON() {
  base::Value::List zoom_factors;
  for (double zoom_value : kPresetZoomFactors) {
    zoom_factors.Append(zoom_value);
  }
  std::string zoom_factors_json;
  bool success = base::JSONWriter::Write(zoom_factors, &zoom_factors_json);
  DCHECK(success);
  return zoom_factors_json;
}

}  // namespace zoom
