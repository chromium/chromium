// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TRAFFIC_ANNOTATION_NETWORK_TRAFFIC_ANNOTATION_TEST_HELPER_H_
#define NET_TRAFFIC_ANNOTATION_NETWORK_TRAFFIC_ANNOTATION_TEST_HELPER_H_

#include "net/traffic_annotation/network_traffic_annotation.h"

// Macros for tests traffic annotations.
#define TRAFFIC_ANNOTATION_FOR_TESTS   \
  net::DefineNetworkTrafficAnnotation( \
      "test", "Traffic annotation for unit, browser and other tests")
#define PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS  \
  net::DefinePartialNetworkTrafficAnnotation( \
      "test_partial", "test",                 \
      "Partial traffic annotation for unit, browser and other tests")

#endif  // NET_TRAFFIC_ANNOTATION_NETWORK_TRAFFIC_ANNOTATION_TEST_HELPER_H_
