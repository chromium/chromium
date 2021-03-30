// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_SQL_UTILS_H_
#define CONTENT_BROWSER_CONVERSIONS_SQL_UTILS_H_

#include <stdint.h>

#include "base/time/time.h"
#include "url/origin.h"

namespace content {

std::string SerializeOrigin(const url::Origin& origin);

url::Origin DeserializeOrigin(const std::string& origin);

int64_t SerializeTime(base::Time time);

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_SQL_UTILS_H_
