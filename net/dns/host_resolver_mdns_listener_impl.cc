// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_mdns_listener_impl.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/host_resolver_internal_result.h"
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
  CHECK(record);

  std::unique_ptr<HostResolverInternalResult> parsed_entry =
      HostResolverMdnsTask::ParseResult(OK, query_host_.host(), query_type_,
                                        record);
  if (parsed_entry->type() == HostResolverInternalResult::Type::kError) {
    delegate_->OnUnhandledResult(ConvertUpdateType(update), query_type_);
    return;
  }
  CHECK_EQ(parsed_entry->type(), HostResolverInternalResult::Type::kData);

  switch (query_type_) {
    case DnsQueryType::UNSPECIFIED:
    case DnsQueryType::HTTPS:
      NOTREACHED();
    case DnsQueryType::A:
    case DnsQueryType::AAAA: {
      CHECK_EQ(1u, parsed_entry->AsData().endpoints().size());
      IPEndPoint endpoint = parsed_entry->AsData().endpoints().front();
      if (endpoint.port() == 0) {
        endpoint = IPEndPoint(endpoint.address(), query_host_.port());
      }
      delegate_->OnAddressResult(ConvertUpdateType(update), query_type_,
                                 endpoint);
      break;
    }
    case DnsQueryType::TXT:
      delegate_->OnTextResult(ConvertUpdateType(update), query_type_,
                              parsed_entry->AsData().strings());
      break;
    case DnsQueryType::PTR:
    case DnsQueryType::SRV:
      DCHECK(!parsed_entry->AsData().hosts().empty());
      HostPortPair host = parsed_entry->AsData().hosts().front();
      if (host.port() == 0) {
        host.set_port(query_host_.port());
      }
      delegate_->OnHostnameResult(ConvertUpdateType(update), query_type_,
                                  std::move(host));
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
