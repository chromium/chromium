// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_switches.h"

namespace network {

namespace switches {

// Forces Network Quality Estimator (NQE) to return a specific effective
// connection type.
const char kForceEffectiveConnectionType[] = "force-effective-connection-type";

// These mappings only apply to the host resolver.
const char kHostResolverRules[] = "host-resolver-rules";

// Causes net::URLFetchers to ignore requests for SSL client certificates,
// causing them to attempt an unauthenticated SSL/TLS session. This is intended
// for use when testing various service URLs (eg: kPromoServerURL, kSbURLPrefix,
// kSyncServiceURL, etc).
const char kIgnoreUrlFetcherCertRequests[] = "ignore-urlfetcher-cert-requests";

// A set of public key hashes for which to ignore certificate-related errors.
//
// If the certificate chain presented by the server does not validate, and one
// or more certificates have public key hashes that match a key from this list,
// the error is ignored.
//
// The switch value must a be a comma-separated list of Base64-encoded SHA-256
// SPKI Fingerprints (RFC 7469, Section 2.4).
//
// This switch has no effect unless --user-data-dir (as defined by the content
// embedder) is also present.
const char kIgnoreCertificateErrorsSPKIList[] =
    "ignore-certificate-errors-spki-list";

// Enables saving net log events to a file. If a value is given, it used as the
// path the the file, otherwise the file is named netlog.json and placed in the
// user data directory.
const char kLogNetLog[] = "log-net-log";

// Sets the granularity of events to capture in the network log. The mode can be
// set to one of the following values:
//   "Default"
//   "IncludeSensitive"
//   "Everything"
//
// See the enums of the corresponding name in net_log_capture_mode.h for a
// description of their meanings.
const char kNetLogCaptureMode[] = "net-log-capture-mode";

// Causes SSL key material to be logged to the specified file for debugging
// purposes. See
// https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/Key_Log_Format
// for the format.
const char kSSLKeyLogFile[] = "ssl-key-log-file";

// Allows overriding the list of restricted ports by passing a comma-separated
// list of port numbers.
const char kExplicitlyAllowedPorts[] = "explicitly-allowed-ports";

// Treat given (insecure) origins as secure origins. Multiple origins can be
// supplied as a comma-separated list. For the definition of secure contexts,
// see https://w3c.github.io/webappsec-secure-contexts/ and
// https://www.w3.org/TR/powerful-features/#is-origin-trustworthy
//
// Example:
// --unsafely-treat-insecure-origin-as-secure=http://a.test,http://b.test
const char kUnsafelyTreatInsecureOriginAsSecure[] =
    "unsafely-treat-insecure-origin-as-secure";

// Manually sets additional Trust Tokens key commitments in the network service
// to the given value, which should be a JSON dictionary satisfying the
// requirements of TrustTokenKeyCommitmentParser::ParseMultipleIssuers.
//
// These keys are available in addition to keys provided by the most recent call
// to TrustTokenKeyCommitments::Set.
//
// For issuers with keys provided through both the command line and
// TrustTokenKeyCommitments::Set, the keys provided through the command line
// take precedence. This is because someone testing manually might want to pass
// additional keys via the command line to a real Chrome release with the
// component updater enabled, and it would be surprising if the manually-passed
// keys were overwritten some time after startup when the component updater
// runs.
const char kAdditionalTrustTokenKeyCommitments[] =
    "additional-trust-token-key-commitments";

}  // namespace switches

}  // namespace network
