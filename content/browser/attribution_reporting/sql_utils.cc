// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/sql_utils.h"

#include "base/check.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

std::string SerializeOrigin(const url::Origin& origin) {
  // Conversion API is only designed to be used for secure
  // contexts (targets and reporting endpoints). We should have filtered out bad
  // origins at a higher layer.
  DCHECK(!origin.opaque());
  return origin.Serialize();
}

url::Origin DeserializeOrigin(const std::string& origin) {
  return url::Origin::Create(GURL(origin));
}

}  // namespace content
