// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_REQUEST_PRIORITY_H_
#define NET_BASE_REQUEST_PRIORITY_H_

#include "net/base/net_export.h"

namespace net {

// Prioritization used in various parts of the networking code such
// as connection prioritization and resource loading prioritization.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: RequestPriority
//
// This enum should be synchronized with the enum NetRequestPriority in
// tools/metrics/histograms/enums.xml.
enum RequestPriority {
  THROTTLED = 0,  // Used to signal that resources
                  // should be reserved for following
                  // requests (i.e. that higher priority
                  // following requests are expected).
  MINIMUM_PRIORITY = THROTTLED,
  IDLE = 1,  // Default "as resources available" level.
  LOWEST = 2,
  DEFAULT_PRIORITY = LOWEST,
  LOW = 3,
  MEDIUM = 4,
  HIGHEST = 5,
  MAXIMUM_PRIORITY = HIGHEST,
};

// For simplicity, one can assume that one can index into array of
// NUM_PRIORITIES elements with a RequestPriority (i.e.,
// MINIMUM_PRIORITY == 0).
enum RequestPrioritySize {
  NUM_PRIORITIES = MAXIMUM_PRIORITY + 1,
};

// Default value to use for the incremental loading flag as part of HTTP
// extensible priorities (RFC 9218). Currently used for HTTP/3.
//
// This default is the value that works best in most cases that the networking
// code supports (simultaneous requests are loaded concurrently and don't block
// one another).
//
// This is independent from the spec default for the protocol, which is false.
const bool kDefaultPriorityIncremental = true;

NET_EXPORT const char* RequestPriorityToString(RequestPriority priority);

}  // namespace net

#endif  // NET_BASE_REQUEST_PRIORITY_H_
