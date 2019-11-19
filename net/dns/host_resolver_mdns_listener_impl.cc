// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_mdns_listener_impl.h"

#include "base/logging.h"
#include "net/base/host_port_pair.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_mdns_task.h"
#include "net/dns/record_parsed.h"

namespace net {

namespace {

HostResolver::MdnsListener::Delegate::UpdateType ConvertUpdateType(
    net::MDnsListener::UpdateType type) {
  switch (type) {
    case net::MDnsListener::RECORD_ADDED:
      return HostResolver::MdnsListener::Delegate::UpdateType::ADDED;
    case net::MDnsListener::RECORD_CHANGED:
      return HostResolver::MdnsListener::Delegate::UpdateType::CHANGED;
    case net::MDnsListener::RECORD_REMOVED:
      return HostResolver::MdnsListener::Delegate::UpdateType::REMOVED;
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
    case DnsQueryType::ESNI:
      NOTREACHED();
      break;
    case DnsQueryType::A:
    case DnsQueryType::AAAA:
      DCHECK(parsed_entry.addresses());
      DCHECK_EQ(1u, parsed_entry.addresses().value().size());
      delegate_->OnAddressResult(ConvertUpdateType(update), query_type_,
                                 parsed_entry.addresses().value().front());
      break;
    case DnsQueryType::TXT:
      DCHECK(parsed_entry.text_records());
      delegate_->OnTextResult(ConvertUpdateType(update), query_type_,
                              parsed_entry.text_records().value());
      break;
    case DnsQueryType::PTR:
    case DnsQueryType::SRV:
      DCHECK(parsed_entry.hostnames());
      delegate_->OnHostnameResult(ConvertUpdateType(update), query_type_,
                                  parsed_entry.hostnames().value().front());
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
