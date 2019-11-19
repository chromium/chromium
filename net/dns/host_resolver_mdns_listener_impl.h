// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_MDNS_LISTENER_IMPL_H_
#define NET_DNS_HOST_RESOLVER_MDNS_LISTENER_IMPL_H_

#include <memory>
#include <string>
#include <utility>

#include "base/sequence_checker.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mdns_client.h"
#include "net/dns/public/dns_query_type.h"

namespace net {

class HostPortPair;
class RecordParsed;

// Intermediary between the HostResolver::CreateMdnsListener API and the
// underlying listener functionality in MDnsClient.
class HostResolverMdnsListenerImpl : public HostResolver::MdnsListener,
                                     public net::MDnsListener::Delegate {
 public:
  using Delegate = HostResolver::MdnsListener::Delegate;

  HostResolverMdnsListenerImpl(const HostPortPair& query_host,
                               DnsQueryType query_type);
  ~HostResolverMdnsListenerImpl() override;

  void set_inner_listener(std::unique_ptr<net::MDnsListener> inner_listener) {
    DCHECK_EQ(OK, initialization_error_);
    inner_listener_ = std::move(inner_listener);
  }

  void set_initialization_error(int error) {
    DCHECK(!inner_listener_);
    initialization_error_ = error;
  }

  // HostResolver::MdnsListener implementation
  int Start(Delegate* delegate) override;

  // net::MDnsListener::Delegate implementation
  void OnRecordUpdate(net::MDnsListener::UpdateType update,
                      const RecordParsed* record) override;
  void OnNsecRecord(const std::string& name, unsigned type) override;
  void OnCachePurged() override;

 private:
  HostPortPair query_host_;
  DnsQueryType query_type_;

  int initialization_error_ = OK;
  std::unique_ptr<net::MDnsListener> inner_listener_;
  Delegate* delegate_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_MDNS_LISTENER_IMPL_H_
