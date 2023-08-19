// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INTEREST_GROUP_API_OPERATION_H_
#define CONTENT_PUBLIC_BROWSER_INTEREST_GROUP_API_OPERATION_H_

namespace content {

// The possible operations performable by parties related to the Interest
// Group API.
enum class InterestGroupApiOperation { kJoin, kLeave, kUpdate, kSell, kBuy };

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_INTEREST_GROUP_API_OPERATION_H_
