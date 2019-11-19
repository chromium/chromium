// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_auth_cache.h"

#include "base/logging.h"
#include "url/gurl.h"

namespace net {

// static
const size_t FtpAuthCache::kMaxEntries = 10;

FtpAuthCache::Entry::Entry(const GURL& origin,
                           const AuthCredentials& credentials)
    : origin(origin),
      credentials(credentials) {
}

FtpAuthCache::Entry::~Entry() = default;

FtpAuthCache::FtpAuthCache() = default;

FtpAuthCache::~FtpAuthCache() = default;

FtpAuthCache::Entry* FtpAuthCache::Lookup(const GURL& origin) {
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->origin == origin)
      return &(*it);
  }
  return nullptr;
}

void FtpAuthCache::Add(const GURL& origin, const AuthCredentials& credentials) {
  DCHECK(origin.SchemeIs("ftp"));
  DCHECK_EQ(origin.GetOrigin(), origin);

  Entry* entry = Lookup(origin);
  if (entry) {
    entry->credentials = credentials;
  } else {
    entries_.push_front(Entry(origin, credentials));

    // Prevent unbound memory growth of the cache.
    if (entries_.size() > kMaxEntries)
      entries_.pop_back();
  }
}

void FtpAuthCache::Remove(const GURL& origin,
                          const AuthCredentials& credentials) {
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->origin == origin && it->credentials.Equals(credentials)) {
      entries_.erase(it);
      DCHECK(!Lookup(origin));
      return;
    }
  }
}

}  // namespace net
