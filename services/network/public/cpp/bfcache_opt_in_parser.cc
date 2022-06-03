// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/bfcache_opt_in_parser.h"

#include "base/strings/string_piece.h"
#include "net/http/structured_headers.h"

namespace network {

bool ParseBFCacheOptInUnload(base::StringPiece header_value) {
  const auto maybe_list = net::structured_headers::ParseList(header_value);
  if (!maybe_list.has_value())
    return false;

  using net::structured_headers::Item;
  using net::structured_headers::ParameterizedMember;

  const auto& list = *maybe_list;
  for (const ParameterizedMember& parameterized_member : list) {
    // Ignore inner list
    if (parameterized_member.member.size() != 1u)
      continue;

    const Item& single_item = parameterized_member.member[0].item;
    if (!single_item.is_token())
      continue;

    if (single_item.GetString() == "unload")
      return true;
  }

  return false;
}

}  // namespace network
