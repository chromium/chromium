// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_SERVER_ITERATOR_H_
#define NET_DNS_DNS_SERVER_ITERATOR_H_

#include <stddef.h>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "net/base/net_export.h"
#include "net/dns/public/secure_dns_mode.h"

namespace net {

class DnsSession;
class ResolveContext;

// Iterator used to get the next server to try for a DNS transaction.
// Each iterator should be scoped to a single query. A new query, therefore,
// requires a new iterator.
//
// Finds the first eligible server below the global failure limits
// (|max_failures|), or if no eligible servers are below failure limits, the
// eligible one with the oldest last failure. Global failures are tracked by
// ResolveContext.
//
// If |session| goes out of date, this iterator will report that no attempts are
// available and thus cease to return anything.
class NET_EXPORT_PRIVATE DnsServerIterator {
 public:
  DnsServerIterator(size_t nameservers_size,
                    size_t starting_index,
                    int max_times_returned,
                    int max_failures,
                    const ResolveContext* resolve_context,
                    const DnsSession* session);

  virtual ~DnsServerIterator();

  // Not copy or moveable.
  DnsServerIterator(const DnsServerIterator&) = delete;
  DnsServerIterator& operator=(const DnsServerIterator&) = delete;
  DnsServerIterator(DnsServerIterator&&) = delete;

  // Return the index of the next server to be attempted.
  // Should only be called if AttemptAvailable() is true.
  virtual size_t GetNextAttemptIndex() = 0;

  virtual bool AttemptAvailable() = 0;

 protected:
  // The number of times each server index was returned.
  std::vector<int> times_returned_;
  // The number of attempts that will be made per server.
  int max_times_returned_;
  // The failure limit before a server is skipped in the attempt ordering.
  // Servers past their failure limit will only be used once all remaining
  // servers are also past their failure limit.
  int max_failures_;
  raw_ptr<const ResolveContext, DanglingUntriaged> resolve_context_;
  // The first server index to try when GetNextAttemptIndex() is called.
  size_t next_index_;

  raw_ptr<const DnsSession, DanglingUntriaged> session_;
};

// Iterator used to get the next server to try for a DoH transaction.
// Each iterator should be scoped to a single query. A new query, therefore,
// requires a new iterator.
//
// Finds the first eligible server below the global failure limits
// (|max_failures|), or if no eligible servers are below failure limits, the
// eligible one with the oldest last failure. Global failures are tracked by
// ResolveContext.
//
// Once a server is returned |max_times_returned| times, it is ignored.
//
// If in AUTOMATIC mode, DoH servers are only eligible if "available".  See
// GetDohServerAvailability() for details.
class NET_EXPORT_PRIVATE DohDnsServerIterator : public DnsServerIterator {
 public:
  DohDnsServerIterator(size_t nameservers_size,
                       size_t starting_index,
                       int max_times_returned,
                       int max_failures,
                       const SecureDnsMode& secure_dns_mode,
                       const ResolveContext* resolve_context,
                       const DnsSession* session)
      : DnsServerIterator(nameservers_size,
                          starting_index,
                          max_times_returned,
                          max_failures,
                          resolve_context,
                          session),
        secure_dns_mode_(secure_dns_mode) {}

  ~DohDnsServerIterator() override = default;

  // Not copy or moveable.
  DohDnsServerIterator(const DohDnsServerIterator&) = delete;
  DohDnsServerIterator& operator=(const DohDnsServerIterator&) = delete;
  DohDnsServerIterator(DohDnsServerIterator&&) = delete;

  size_t GetNextAttemptIndex() override;

  // Return true if any servers in the list still has attempts available.
  // False otherwise. An attempt is possible if any server, that is available,
  // is under max_times_returned_ tries.
  bool AttemptAvailable() override;

 private:
  SecureDnsMode secure_dns_mode_;
};

// Iterator used to get the next server to try for a classic DNS transaction.
// Each iterator should be scoped to a single query. A new query, therefore,
// requires a new iterator.
//
// Finds the first eligible server below the global failure limits
// (|max_failures|), or if no eligible servers are below failure limits, the
// eligible one with the oldest last failure. Global failures are tracked by
// ResolveContext.

// Once a server is returned |max_times_returned| times, it is ignored.
class NET_EXPORT_PRIVATE ClassicDnsServerIterator : public DnsServerIterator {
 public:
  ClassicDnsServerIterator(size_t nameservers_size,
                           size_t starting_index,
                           int max_times_returned,
                           int max_failures,
                           const ResolveContext* resolve_context,
                           const DnsSession* session)
      : DnsServerIterator(nameservers_size,
                          starting_index,
                          max_times_returned,
                          max_failures,
                          resolve_context,
                          session) {}

  ~ClassicDnsServerIterator() override = default;

  // Not copy or moveable.
  ClassicDnsServerIterator(const ClassicDnsServerIterator&) = delete;
  ClassicDnsServerIterator& operator=(const ClassicDnsServerIterator&) = delete;
  ClassicDnsServerIterator(ClassicDnsServerIterator&&) = delete;

  size_t GetNextAttemptIndex() override;

  // Return true if any servers in the list still has attempts available.
  // False otherwise. An attempt is possible if any server is under
  // max_times_returned_ tries.
  bool AttemptAvailable() override;
};

// Iterator that can be used when the query is sent via APIs that do not allow
// controlling the underlying DNS server used. For example, this is the case for
// DnsTransactionFactory::AttemptMode::kPlatform on Android.
// In this scenario, we want to limit the number of attempts for each query to
// one: calling GetNextAttemptIndex causes future calls to AttemptAvailable to
// always return false.
class NET_EXPORT_PRIVATE OneShotDnsServerIterator : public DnsServerIterator {
 public:
  OneShotDnsServerIterator()
      : DnsServerIterator(/*nameservers_size=*/0,
                          /*starting_index=*/0,
                          /*max_times_returned=*/0,
                          /*max_failures=*/0,
                          /*resolve_context=*/nullptr,
                          /*session=*/nullptr) {}

  ~OneShotDnsServerIterator() override = default;

  // Not copy or moveable.
  OneShotDnsServerIterator(const OneShotDnsServerIterator&) = delete;
  OneShotDnsServerIterator& operator=(const OneShotDnsServerIterator&) = delete;
  OneShotDnsServerIterator(OneShotDnsServerIterator&&) = delete;

  // DnsServerIterator methods:
  size_t GetNextAttemptIndex() override;
  bool AttemptAvailable() override;

 private:
  bool attempt_available_ = true;
};

}  // namespace net
#endif  // NET_DNS_DNS_SERVER_ITERATOR_H_
