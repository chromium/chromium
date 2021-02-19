// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion.h"

namespace blink {

bool NGExclusion::operator==(const NGExclusion& other) const {
  return type == other.type && rect == other.rect &&
         shape_data == other.shape_data;
}

}  // namespace blink
