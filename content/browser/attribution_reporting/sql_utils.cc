// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/sql_utils.h"

#include <string>

#include "url/gurl.h"
#include "url/origin.h"

namespace content {

url::Origin DeserializeOrigin(const std::string& origin) {
  return url::Origin::Create(GURL(origin));
}

}  // namespace content
