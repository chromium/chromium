// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/dhcp_pac_file_fetcher.h"

#include "net/base/net_errors.h"

namespace net {

std::string DhcpPacFileFetcher::GetFetcherName() const {
  return std::string();
}

DhcpPacFileFetcher::DhcpPacFileFetcher() = default;

DhcpPacFileFetcher::~DhcpPacFileFetcher() = default;

DoNothingDhcpPacFileFetcher::DoNothingDhcpPacFileFetcher() = default;

DoNothingDhcpPacFileFetcher::~DoNothingDhcpPacFileFetcher() = default;

int DoNothingDhcpPacFileFetcher::Fetch(
    std::u16string* utf16_text,
    CompletionOnceCallback callback,
    const NetLogWithSource& net_log,
    const NetworkTrafficAnnotationTag traffic_annotation) {
  return ERR_NOT_IMPLEMENTED;
}

void DoNothingDhcpPacFileFetcher::Cancel() {}

void DoNothingDhcpPacFileFetcher::OnShutdown() {}

const GURL& DoNothingDhcpPacFileFetcher::GetPacURL() const {
  return gurl_;
}

std::string DoNothingDhcpPacFileFetcher::GetFetcherName() const {
  return "do nothing";
}

}  // namespace net
