// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_store.h"

#include "base/bind.h"
#include "base/callback.h"

namespace net {

CookieStore::CookieStore() = default;

CookieStore::~CookieStore() = default;

// Default implementation which returns a default vector of UNKNOWN
// CookieAccessSemantics.
void CookieStore::GetAllCookiesWithAccessSemanticsAsync(
    GetAllCookiesWithAccessSemanticsCallback callback) {
  GetAllCookiesCallback adapted_callback = base::BindOnce(
      [](CookieStore::GetAllCookiesWithAccessSemanticsCallback
             original_callback,
         const CookieList& cookies) {
        std::vector<CookieAccessSemantics> default_access_semantics_list;
        default_access_semantics_list.assign(cookies.size(),
                                             CookieAccessSemantics::UNKNOWN);
        std::move(original_callback)
            .Run(cookies, default_access_semantics_list);
      },
      std::move(callback));
  GetAllCookiesAsync(std::move(adapted_callback));
}

void CookieStore::DeleteAllAsync(DeleteCallback callback) {
  DeleteAllCreatedInTimeRangeAsync(CookieDeletionInfo::TimeRange(),
                                   std::move(callback));
}

void CookieStore::SetForceKeepSessionState() {
  // By default, do nothing.
}

void CookieStore::SetCookieAccessDelegate(
    std::unique_ptr<CookieAccessDelegate> delegate) {
  cookie_access_delegate_ = std::move(delegate);
}

void CookieStore::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_absolute_name) const {}

}  // namespace net
