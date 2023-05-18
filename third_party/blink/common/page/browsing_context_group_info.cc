// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/browsing_context_group_info.h"

namespace blink {

// static
BrowsingContextGroupInfo BrowsingContextGroupInfo::CreateUnique() {
  return BrowsingContextGroupInfo(base::UnguessableToken::Create(),
                                  base::UnguessableToken::Create());
}

BrowsingContextGroupInfo::BrowsingContextGroupInfo(
    const base::UnguessableToken& browsing_context_group_token,
    const base::UnguessableToken& coop_related_group_token)
    : browsing_context_group_token(browsing_context_group_token),
      coop_related_group_token(coop_related_group_token) {}

BrowsingContextGroupInfo::BrowsingContextGroupInfo(
    mojo::DefaultConstruct::Tag) {}

}  // namespace blink
