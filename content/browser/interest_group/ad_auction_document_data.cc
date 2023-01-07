// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_document_data.h"

DOCUMENT_USER_DATA_KEY_IMPL(AdAuctionDocumentData);

AdAuctionDocumentData::~AdAuctionDocumentData() = default;

AdAuctionDocumentData::AdAuctionDocumentData(content::RenderFrameHost* rfh,
                                             url::Origin interest_group_owner,
                                             std::string interest_group_name)
    : DocumentUserData(rfh),
      interest_group_owner_(interest_group_owner),
      interest_group_name_(interest_group_name) {}
