// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IDEMPOTENCY_H_
#define NET_BASE_IDEMPOTENCY_H_

namespace net {

// Idempotency of the request, which determines that if it is safe to enable
// 0-RTT for the request. By default, 0-RTT is only enabled for safe
// HTTP methods, i.e., GET, HEAD, OPTIONS, and TRACE. For other methods,
// enabling 0-RTT may cause security issues since a network observer can replay
// the request. If the request has any side effects, those effects can happen
// multiple times. It is only safe to enable the 0-RTT if it is known that
// the request is idempotent.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: Idempotency
enum Idempotency {
  DEFAULT_IDEMPOTENCY = 0,
  IDEMPOTENT = 1,
  NOT_IDEMPOTENT = 2,
};

}  // namespace net

#endif  // NET_BASE_IDEMPOTENCY_H_
