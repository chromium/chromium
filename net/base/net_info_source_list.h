// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_INFO_SOURCE_LIST_H_
#define NET_BASE_NET_INFO_SOURCE_LIST_H_

// NetInfo Sources written to NetLog JSON files.
const char kNetInfoProxySettings[] = "proxySettings";
const char kNetInfoBadProxies[] = "badProxies";
const char kNetInfoHostResolver[] = "hostResolverInfo";
const char kNetInfoDohProvidersDisabledDueToFeature[] =
    "dohProvidersDisabledDueToFeature";
const char kNetInfoSocketPool[] = "socketPoolInfo";
const char kNetInfoQuic[] = "quicInfo";
const char kNetInfoSpdySessions[] = "spdySessionInfo";
const char kNetInfoSpdyStatus[] = "spdyStatus";
const char kNetInfoAltSvcMappings[] = "altSvcMappings";
const char kNetInfoHTTPCache[] = "httpCacheInfo";
const char kNetInfoReporting[] = "reportingInfo";
const char kNetInfoFieldTrials[] = "activeFieldTrialGroups";

#endif  // NET_BASE_NET_INFO_SOURCE_LIST_H_
