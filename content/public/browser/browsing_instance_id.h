// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSING_INSTANCE_ID_H_
#define CONTENT_PUBLIC_BROWSER_BROWSING_INSTANCE_ID_H_

#include "base/types/id_type.h"

namespace content {

// BrowsingInstanceId is a unique ID of a BrowsingInstance (i.e., group of
// related browsing contexts).
//
// Note: BrowsingInstanceId is intentionally exposed via the //content/public
// API, even though BrowsingInstance is not a part of the public API.
// (BrowsingInstance is only used and managed internally within the //content
// layer.)
//
// Note: BrowsingInstanceIdTag is not a real class - this template argument of
// IdType32 is only used to differentiate IdType<BrowsingInstanceIdTag> from
// IdType32<OtherIdTags> so that BrowsingInstanceId cannot be assigned to
// OtherId and vice versa.
using BrowsingInstanceId = base::IdType32<class BrowsingInstanceIdTag>;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSING_INSTANCE_ID_H_
