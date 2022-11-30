// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_DOCUMENT_DATA_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_DOCUMENT_DATA_H_

#include "content/public/browser/document_user_data.h"
#include "url/origin.h"

class AdAuctionDocumentData
    : public content::DocumentUserData<AdAuctionDocumentData> {
 public:
  ~AdAuctionDocumentData() override;

  const url::Origin& interest_group_owner() const {
    return interest_group_owner_;
  }
  const std::string& interest_group_name() const {
    return interest_group_name_;
  }

 private:
  // No public constructors to force going through static methods of
  // DocumentUserData (e.g. CreateForCurrentDocument).
  explicit AdAuctionDocumentData(content::RenderFrameHost* rfh,
                                 url::Origin interest_group_owner,
                                 std::string interest_group_name);

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  url::Origin interest_group_owner_;
  std::string interest_group_name_;
};

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_DOCUMENT_DATA_H_
