// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/base/gcm_util.h"

#include "base/strings/escape.h"

namespace gcm {

void BuildFormEncoding(const std::string& key,
                       const std::string& value,
                       std::string* out) {
  if (!out->empty())
    out->append("&");
  out->append(key + "=" + base::EscapeUrlEncodedData(value, true));
}

}  // namespace gcm
