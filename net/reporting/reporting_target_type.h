// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_TARGET_TYPE_H_
#define NET_REPORTING_REPORTING_TARGET_TYPE_H_

namespace net {

// Used to distinguish web developer and enterprise entities so that enterprise
// reports aren’t sent to web developer endpoints and web developer reports
// aren’t sent to enterprise endpoints
enum class ReportingTargetType { kDeveloper, kEnterprise };

}  // namespace net

#endif  // NET_REPORTING_REPORTING_TARGET_TYPE_H_
