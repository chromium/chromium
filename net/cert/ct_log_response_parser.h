// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_LOG_RESPONSE_PARSER_H_
#define NET_CERT_CT_LOG_RESPONSE_PARSER_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"

namespace base {
class Value;
}  // namespace base

namespace net::ct {
struct SignedTreeHead;

// Fills in |signed_tree_head| from its JSON representation in
// |json_signed_tree_head|.
// Returns true and fills in |signed_tree_head| if all fields are present and
// valid.Otherwise, returns false and does not modify |signed_tree_head|.
NET_EXPORT bool FillSignedTreeHead(const base::Value& json_signed_tree_head,
                                   SignedTreeHead* signed_tree_head);

NET_EXPORT bool FillConsistencyProof(
    const base::Value& json_signed_tree_head,
    std::vector<std::string>* consistency_proof);

}  // namespace net::ct
#endif  // NET_CERT_CT_LOG_RESPONSE_PARSER_H_
