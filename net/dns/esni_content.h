// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_ESNI_CONTENT_H_
#define NET_DNS_ESNI_CONTENT_H_

#include <map>
#include <set>
#include <string>

#include "base/strings/string_piece.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"

namespace net {

// An EsniContent struct represents an aggregation of the
// content of several ESNI (TLS 1.3 Encrypted Server Name Indication,
// draft 4) resource records.
//
// This aggregation contains:
// (1) The ESNI key objects from each of the ESNI records, and
// (2) A collection of IP addresses, each of which is associated
// with one or more of the key objects. (Each key will likely also
// be associated with several destination addresses.)
class NET_EXPORT EsniContent {
 public:
  EsniContent();
  EsniContent(const EsniContent& other);
  EsniContent(EsniContent&& other);
  EsniContent& operator=(const EsniContent& other);
  EsniContent& operator=(EsniContent&& other);
  ~EsniContent();

  // Key objects (which might be up to ~50K in length) are stored
  // in a collection of std::string; use transparent comparison
  // to allow checking whether a given base::StringPiece is in
  // the collection without making copies.
  struct StringPieceComparator {
    using is_transparent = int;

    bool operator()(const base::StringPiece lhs,
                    const base::StringPiece rhs) const {
      return lhs < rhs;
    }
  };

  const std::set<std::string, StringPieceComparator>& keys() const;
  const std::map<IPAddress, std::set<base::StringPiece>>& keys_for_addresses()
      const;

  // Adds |key| (if it is not already stored) without associating it
  // with any particular addresss; if this addition is performed, it
  // copies the underlying string.
  void AddKey(base::StringPiece key);

  // Associates a key with an address, copying the underlying string to
  // the internal collection of keys if it is not already stored.
  void AddKeyForAddress(const IPAddress& address, base::StringPiece key);

  // Merges the contents of |other|:
  // 1. unions the collection of stored keys with |other.keys()| and
  // 2. unions the stored address-key associations with
  // |other.keys_for_addresses()|.
  void MergeFrom(const EsniContent& other);

 private:
  // In order to keep the StringPieces in |keys_for_addresses_| valid,
  // |keys_| must be of a collection type guaranteeing stable pointers.
  std::set<std::string, StringPieceComparator> keys_;

  std::map<IPAddress, std::set<base::StringPiece>> keys_for_addresses_;
};

// Two EsniContent structs are equal if they have the same set of keys, the
// same set of IP addresses, and the same subset of the keys corresponding to
// each IP address.
NET_EXPORT_PRIVATE
bool operator==(const EsniContent& c1, const EsniContent& c2);

}  // namespace net

#endif  // NET_DNS_ESNI_CONTENT_H_
