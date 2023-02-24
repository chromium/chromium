// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list.h"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/values.h"
#include "net/base/sys_addrinfo.h"

namespace net {

AddressList::AddressList() = default;

AddressList::AddressList(const AddressList&) = default;

AddressList& AddressList::operator=(const AddressList&) = default;

AddressList::AddressList(AddressList&&) = default;

AddressList& AddressList::operator=(AddressList&&) = default;

AddressList::~AddressList() = default;

AddressList::AddressList(const IPEndPoint& endpoint) {
  push_back(endpoint);
}

AddressList::AddressList(const IPEndPoint& endpoint,
                         std::vector<std::string> aliases)
    : dns_aliases_(std::move(aliases)) {
  push_back(endpoint);
}

AddressList::AddressList(std::vector<IPEndPoint> endpoints)
    : endpoints_(std::move(endpoints)) {}

// static
AddressList AddressList::CreateFromIPAddress(const IPAddress& address,
                                             uint16_t port) {
  return AddressList(IPEndPoint(address, port));
}

// static
AddressList AddressList::CreateFromIPAddressList(
    const IPAddressList& addresses,
    std::vector<std::string> aliases) {
  AddressList list;
  for (const auto& address : addresses) {
    list.push_back(IPEndPoint(address, 0));
  }
  list.SetDnsAliases(std::move(aliases));
  return list;
}

// static
AddressList AddressList::CreateFromAddrinfo(const struct addrinfo* head) {
  DCHECK(head);
  AddressList list;
  if (head->ai_canonname) {
    std::vector<std::string> aliases({std::string(head->ai_canonname)});
    list.SetDnsAliases(std::move(aliases));
  }
  for (const struct addrinfo* ai = head; ai; ai = ai->ai_next) {
    IPEndPoint ipe;
    // NOTE: Ignoring non-INET* families.
    if (ipe.FromSockAddr(ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen)))
      list.push_back(ipe);
    else
      DLOG(WARNING) << "Unknown family found in addrinfo: " << ai->ai_family;
  }
  return list;
}

// static
AddressList AddressList::CopyWithPort(const AddressList& list, uint16_t port) {
  AddressList out;
  out.SetDnsAliases(list.dns_aliases());
  for (const auto& i : list)
    out.push_back(IPEndPoint(i.address(), port));
  return out;
}

void AddressList::SetDefaultCanonicalName() {
  DCHECK(!empty());
  DCHECK(dns_aliases_.empty());
  SetDnsAliases({front().ToStringWithoutPort()});
}

void AddressList::SetDnsAliases(std::vector<std::string> aliases) {
  // TODO(cammie): Track down the callers who use {""} for `aliases` and
  // update so that we can enforce by DCHECK below.
  // The empty canonical name is represented by a empty `dns_aliases_`
  // vector, so in this case we reset the field.
  if (aliases == std::vector<std::string>({""})) {
    dns_aliases_ = std::vector<std::string>();
    return;
  }

  dns_aliases_ = std::move(aliases);
}

void AddressList::AppendDnsAliases(std::vector<std::string> aliases) {
  DCHECK(aliases != std::vector<std::string>({""}));
  using iter_t = std::vector<std::string>::iterator;

  dns_aliases_.insert(dns_aliases_.end(),
                      std::move_iterator<iter_t>(aliases.begin()),
                      std::move_iterator<iter_t>(aliases.end()));
}

base::Value::Dict AddressList::NetLogParams() const {
  base::Value::Dict dict;

  base::Value::List address_list;
  for (const auto& ip_endpoint : *this)
    address_list.Append(ip_endpoint.ToString());
  dict.Set("address_list", std::move(address_list));

  base::Value::List alias_list;
  for (const std::string& alias : dns_aliases_)
    alias_list.Append(alias);
  dict.Set("aliases", std::move(alias_list));

  return dict;
}

void AddressList::Deduplicate() {
  if (size() > 1) {
    std::vector<std::pair<IPEndPoint, int>> make_me_into_a_map(size());
    for (auto& addr : *this)
      make_me_into_a_map.emplace_back(addr, 0);
    base::flat_map<IPEndPoint, int> inserted(std::move(make_me_into_a_map));

    std::vector<IPEndPoint> deduplicated_addresses;
    deduplicated_addresses.reserve(inserted.size());
    for (const auto& addr : *this) {
      int& count = inserted[addr];
      if (!count) {
        deduplicated_addresses.push_back(addr);
        ++count;
      }
    }
    endpoints_.swap(deduplicated_addresses);
  }
}

}  // namespace net
