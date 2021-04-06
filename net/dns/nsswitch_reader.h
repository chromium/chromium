// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_NSSWITCH_READER_H_
#define NET_DNS_NSSWITCH_READER_H_

#include <string>
#include <tuple>
#include <vector>

#include "base/callback.h"
#include "net/base/net_export.h"

namespace net {

// Reader to read and parse Posix nsswitch.conf files, particularly the "hosts:"
// database entry.
class NET_EXPORT_PRIVATE NsswitchReader {
 public:
  enum class Service {
    kUnknown,
    kFiles,
    kDns,
    kMdns,
    kMdns4,
    kMdns6,
    kMdnsMinimal,
    kMdns4Minimal,
    kMdns6Minimal,
    kMyHostname,
    kResolve,
    kNis,
  };

  enum class Status {
    kUnknown,
    kSuccess,
    kNotFound,
    kUnavailable,
    kTryAgain,
  };

  enum class Action {
    kUnknown,
    kReturn,
    kContinue,
    kMerge,
  };

  struct ServiceAction {
    bool operator==(const ServiceAction& other) const {
      return std::tie(negated, status, action) ==
             std::tie(other.negated, other.status, other.action);
    }

    bool negated;
    Status status;
    Action action;
  };

  struct NET_EXPORT_PRIVATE ServiceSpecification {
    explicit ServiceSpecification(Service service,
                                  std::vector<ServiceAction> actions = {});
    ~ServiceSpecification();
    ServiceSpecification(const ServiceSpecification&);
    ServiceSpecification& operator=(const ServiceSpecification&);
    ServiceSpecification(ServiceSpecification&&);
    ServiceSpecification& operator=(ServiceSpecification&&);

    bool operator==(const ServiceSpecification& other) const {
      return std::tie(service, actions) ==
             std::tie(other.service, other.actions);
    }

    Service service;
    std::vector<ServiceAction> actions;
  };

  // Test-replacable call for the actual file read. Default implementation does
  // a fresh read of the nsswitch.conf file every time it is called. Returns
  // empty string on error reading the file.
  using FileReadCall = base::RepeatingCallback<std::string()>;

  NsswitchReader();
  ~NsswitchReader();

  NsswitchReader(const NsswitchReader&) = delete;
  NsswitchReader& operator=(const NsswitchReader&) = delete;

  // Reads nsswitch.conf and parses the "hosts:" database. In case of multiple
  // matching databases, only parses the first. Assumes a basic default
  // configuration if the file cannot be read or a "hosts:" database cannot be
  // found.
  std::vector<ServiceSpecification> ReadAndParseHosts();

  void set_file_read_call_for_testing(FileReadCall file_read_call) {
    file_read_call_ = std::move(file_read_call);
  }

 private:
  FileReadCall file_read_call_;
};

}  // namespace net

#endif  // NET_DNS_NSSWITCH_READER_H_
