// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

// static
NetworkTrafficAnnotationTag NetworkTrafficAnnotationTag::FromJavaAnnotation(
    int32_t unique_id_hash_code) {
  return NetworkTrafficAnnotationTag(unique_id_hash_code);
}

}  // namespace net
