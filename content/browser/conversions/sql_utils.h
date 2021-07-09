// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_SQL_UTILS_H_
#define CONTENT_BROWSER_CONVERSIONS_SQL_UTILS_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "url/origin.h"

namespace content {

std::string SerializeOrigin(const url::Origin& origin) WARN_UNUSED_RESULT;

url::Origin DeserializeOrigin(const std::string& origin) WARN_UNUSED_RESULT;

int64_t SerializeImpressionOrConversionData(uint64_t data) WARN_UNUSED_RESULT;

uint64_t DeserializeImpressionOrConversionData(int64_t data) WARN_UNUSED_RESULT;

// Prevent these functions from being called in the wrong direction.
int64_t SerializeImpressionOrConversionData(int64_t data) = delete;
uint64_t DeserializeImpressionOrConversionData(uint64_t data) = delete;

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_SQL_UTILS_H_
