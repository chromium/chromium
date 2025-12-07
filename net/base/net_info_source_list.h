// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_INFO_SOURCE_LIST_H_
#define NET_BASE_NET_INFO_SOURCE_LIST_H_

namespace net {

// NetInfo Sources written to NetLog JSON files.
inline constexpr char kNetInfoProxySettings[] = "proxySettings";
inline constexpr char kNetInfoBadProxies[] = "badProxies";
inline constexpr char kNetInfoHostResolver[] = "hostResolverInfo";
inline constexpr char kNetInfoDohProvidersDisabledDueToFeature[] =
    "dohProvidersDisabledDueToFeature";
inline constexpr char kNetInfoSocketPool[] = "socketPoolInfo";
inline constexpr char kNetInfoHttpStreamPool[] = "httpStreamPoolInfo";
inline constexpr char kNetInfoQuic[] = "quicInfo";
inline constexpr char kNetInfoSpdySessions[] = "spdySessionInfo";
inline constexpr char kNetInfoSpdyStatus[] = "spdyStatus";
inline constexpr char kNetInfoAltSvcMappings[] = "altSvcMappings";
inline constexpr char kNetInfoHTTPCache[] = "httpCacheInfo";
inline constexpr char kNetInfoReporting[] = "reportingInfo";
inline constexpr char kNetInfoFieldTrials[] = "activeFieldTrialGroups";

}  // namespace net

#endif  // NET_BASE_NET_INFO_SOURCE_LIST_H_
