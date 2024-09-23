// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zoom/page_zoom_constants.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "third_party/blink/public/common/page/page_zoom.h"

namespace zoom {

std::string GetPresetZoomFactorsAsJSON() {
  base::Value::List zoom_factors;
  for (double zoom_value : blink::kPresetBrowserZoomFactors) {
    zoom_factors.Append(zoom_value);
  }
  std::string zoom_factors_json;
  bool success = base::JSONWriter::Write(zoom_factors, &zoom_factors_json);
  DCHECK(success);
  return zoom_factors_json;
}

}  // namespace zoom
