// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_SERVICES_CONTEXT_H_
#define REMOTING_SIGNALING_FTL_SERVICES_CONTEXT_H_

#include <string>

#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"

namespace remoting {

// This is the class for creating context objects to be used when connecting
// to FTL backend.
class FtlServicesContext final {
 public:
  // Exposed for testing.
  static constexpr base::TimeDelta kBackoffInitialDelay = base::Seconds(1);
  static constexpr base::TimeDelta kBackoffMaxDelay = base::Minutes(1);

  static const net::BackoffEntry::Policy& GetBackoffPolicy();
  static std::string GetServerEndpoint();
  static std::string GetChromotingAppIdentifier();
  static ftl::Id CreateIdFromString(const std::string& ftl_id);
  static ftl::RequestHeader CreateRequestHeader(
      const std::string& ftl_auth_token = {});

  FtlServicesContext() = delete;
  FtlServicesContext(const FtlServicesContext&) = delete;
  FtlServicesContext& operator=(const FtlServicesContext&) = delete;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_SERVICES_CONTEXT_H_
