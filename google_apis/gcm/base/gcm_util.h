// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_BASE_GCM_UTIL_H_
#define GOOGLE_APIS_GCM_BASE_GCM_UTIL_H_

#include <string>

namespace gcm {

// Encodes key-value pair into form format that could be submitted as http post
// data. Each key-value pair is separated by an '&', and each key is separated
// from its value by an '='.
void BuildFormEncoding(const std::string& key,
                       const std::string& value,
                       std::string* out);

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_BASE_GCM_UTIL_H_
