// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_NSSWITCH_READER_H_
#define NET_DNS_NSSWITCH_READER_H_

#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback.h"
#include "net/base/net_export.h"

namespace net {

// Reader to read and parse Posix nsswitch.conf files, particularly the "hosts:"
// database entry.
class NET_EXPORT_PRIVATE NsswitchReader {
 public:
  // These values are emitted in metrics. Entries should not be renumbered and
  // numeric values should never be reused. (See NsswitchService in
  // tools/metrics/histograms/enums.xml.)
  enum class Service {
    kUnknown = 0,
    kFiles = 1,
    kDns = 2,
    kMdns = 3,
    kMdns4 = 4,
    kMdns6 = 5,
    kMdnsMinimal = 6,
    kMdns4Minimal = 7,
    kMdns6Minimal = 8,
    kMyHostname = 9,
    kResolve = 10,
    kNis = 11,
    kMaxValue = kNis
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
  virtual ~NsswitchReader();

  NsswitchReader(const NsswitchReader&) = delete;
  NsswitchReader& operator=(const NsswitchReader&) = delete;

  // Reads nsswitch.conf and parses the "hosts:" database. In case of multiple
  // matching databases, only parses the first. Assumes a basic default
  // configuration if the file cannot be read or a "hosts:" database cannot be
  // found.
  virtual std::vector<ServiceSpecification> ReadAndParseHosts();

  void set_file_read_call_for_testing(FileReadCall file_read_call) {
    file_read_call_ = std::move(file_read_call);
  }

 private:
  FileReadCall file_read_call_;
};

}  // namespace net

#endif  // NET_DNS_NSSWITCH_READER_H_
