// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/transport_util.h"

namespace media {
namespace cast {
namespace transport_util {

int LookupOptionWithDefault(const base::Value::Dict& options,
                            const std::string& path,
                            int default_value) {
  return options.FindInt(path).value_or(default_value);
}

}  // namespace transport_util
}  // namespace cast
}  // namespace media
