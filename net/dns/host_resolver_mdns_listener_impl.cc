// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_mdns_listener_impl.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "net/base/host_port_pair.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_mdns_task.h"
#include "net/dns/public/mdns_listener_update_type.h"
#include "net/dns/record_parsed.h"

namespace net {

namespace {

MdnsListenerUpdateType ConvertUpdateType(net::MDnsListener::UpdateType type) {
  switch (type) {
    case net::MDnsListener::RECORD_ADDED:
      return MdnsListenerUpdateType::kAdded;
    case net::MDnsListener::RECORD_CHANGED:
      return MdnsListenerUpdateType::kChanged;
    case net::MDnsListener::RECORD_REMOVED:
      return MdnsListenerUpdateType::kRemoved;
  }
}

}  // namespace

HostResolverMdnsListenerImpl::HostResolverMdnsListenerImpl(
    const HostPortPair& query_host,
    DnsQueryType query_type)
    : query_host_(query_host), query_type_(query_type) {
  DCHECK_NE(DnsQueryType::UNSPECIFIED, query_type_);
}

HostResolverMdnsListenerImpl::~HostResolverMdnsListenerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Destroy |inner_listener_| first to cancel listening and callbacks to |this|
  // before anything else becomes invalid.
  inner_listener_ = nullptr;
}

int HostResolverMdnsListenerImpl::Start(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate);

  if (initialization_error_ != OK)
    return initialization_error_;

  DCHECK(inner_listener_);

  delegate_ = delegate;
  return inner_listener_->Start() ? OK : ERR_FAILED;
}

void HostResolverMdnsListenerImpl::OnRecordUpdate(
    net::MDnsListener::UpdateType update,
    const RecordParsed* record) {
  DCHECK(delegate_);

  HostCache::Entry parsed_entry =
      HostResolverMdnsTask::ParseResult(OK, query_type_, record,
                                        query_host_.host())
          .CopyWithDefaultPort(query_host_.port());
  if (parsed_entry.error() != OK) {
    delegate_->OnUnhandledResult(ConvertUpdateType(update), query_type_);
    return;
  }

  switch (query_type_) {
    case DnsQueryType::UNSPECIFIED:
    case DnsQueryType::HTTPS:
      NOTREACHED_IN_MIGRATION();
      break;
    case DnsQueryType::A:
    case DnsQueryType::AAAA:
      DCHECK_EQ(1u, parsed_entry.ip_endpoints().size());
      delegate_->OnAddressResult(ConvertUpdateType(update), query_type_,
                                 parsed_entry.ip_endpoints().front());
      break;
    case DnsQueryType::TXT:
      delegate_->OnTextResult(ConvertUpdateType(update), query_type_,
                              parsed_entry.text_records());
      break;
    case DnsQueryType::PTR:
    case DnsQueryType::SRV:
      DCHECK(!parsed_entry.hostnames().empty());
      delegate_->OnHostnameResult(ConvertUpdateType(update), query_type_,
                                  parsed_entry.hostnames().front());
      break;
  }
}

void HostResolverMdnsListenerImpl::OnNsecRecord(const std::string& name,
                                                unsigned type) {
  // Do nothing. HostResolver does not support listening for NSEC records.
}

void HostResolverMdnsListenerImpl::OnCachePurged() {
  // Do nothing. HostResolver does not support listening for cache purges.
}

}  // namespace net
