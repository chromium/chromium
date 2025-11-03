// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_SWITCHES_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_SWITCHES_H_

#include "base/component_export.h"

namespace network {

namespace switches {

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kForceEffectiveConnectionType[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kForcePermissionPolicyUnloadDefaultEnabled[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kHostResolverRules[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kHostRules[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kIgnoreCertificateErrorsSPKIList[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES) extern const char kLogNetLog[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kNetLogCaptureMode[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kLogNetLogDuration[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kNetLogMaxSizeMb[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kSSLKeyLogFile[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kTestThirdPartyCookiePhaseout[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kUnsafelyTreatInsecureOriginAsSecure[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kAdditionalTrustTokenKeyCommitments[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kUseFirstPartySet[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kUseRelatedWebsiteSet[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kIpAddressSpaceOverrides[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kDisableSharedDictionaryStorageCleanupForTesting[];
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const char kIgnoreBadMessageForTesting[];

}  // namespace switches

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_SWITCHES_H_
