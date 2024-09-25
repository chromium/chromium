// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_switches.h"

namespace network::switches {

// Forces Network Quality Estimator (NQE) to return a specific effective
// connection type.
const char kForceEffectiveConnectionType[] = "force-effective-connection-type";

// These mappings only apply to the host resolver.
const char kHostResolverRules[] = "host-resolver-rules";

// A set of public key hashes for which to ignore certificate-related errors.
//
// If the certificate chain presented by the server does not validate, and one
// or more certificates have public key hashes that match a key from this list,
// the error is ignored.
//
// The switch value must be a comma-separated list of Base64-encoded SHA-256
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

// Sets the maximum size, in megabytes. The log file can grow to before older
// data is overwritten. Do not use this flag if you want an unlimited file size.
const char kNetLogMaxSizeMb[] = "net-log-max-size-mb";

// Causes SSL key material to be logged to the specified file for debugging
// purposes. See
// https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/Key_Log_Format
// for the format.
const char kSSLKeyLogFile[] = "ssl-key-log-file";

const char kTestThirdPartyCookiePhaseout[] = "test-third-party-cookie-phaseout";

// Treat given (insecure) origins as secure origins. Multiple origins can be
// supplied as a comma-separated list. For the definition of secure contexts,
// see https://w3c.github.io/webappsec-secure-contexts/ and
// https://www.w3.org/TR/powerful-features/#is-origin-trustworthy
//
// Example:
// --unsafely-treat-insecure-origin-as-secure=http://a.test,http://b.test
const char kUnsafelyTreatInsecureOriginAsSecure[] =
    "unsafely-treat-insecure-origin-as-secure";

// Manually sets additional Private State Tokens key commitments in the network
// service to the given value, which should be a JSON dictionary satisfying the
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
    "additional-private-state-token-key-commitments";

// Allows the manual specification of a First-Party Set, as a comma-separated
// list of origins. The first origin in the list is treated as the owner of the
// set.
// DEPRECATED(crbug.com/1486689): This switch is under deprecation due to
// renaming "First-Party Set" to "Related Website Set". Please use
// `kUseRelatedWebsiteSet` instead.
const char kUseFirstPartySet[] = "use-first-party-set";

// Allows the manual specification of a Related Website Set, as a
// comma-separated list of origins. The first origin in the list is treated as
// the primary site of the set.
const char kUseRelatedWebsiteSet[] = "use-related-website-set";

// Specifies manual overrides to the IP endpoint -> IP address space mapping.
// This allows running local tests against "public" and "private" IP addresses.
//
// This switch is specified as a comma-separated list of overrides. Each
// override is given as a colon-separated "<endpoint>:<address space>" pair.
// Grammar, in pseudo-BNF format:
//
//   switch := override-list
//   override-list := override “,” override-list | <nil>
//   override := ip-endpoint “=” address-space
//   address-space := “public” | “private” | “local”
//   ip-endpoint := ip-address ":" port
//   ip-address := see `net::ParseURLHostnameToAddress()` for details
//   port := integer in the [0-65535] range
//
// Any invalid entries in the comma-separated list are ignored.
//
// See also the design doc:
// https://docs.google.com/document/d/1-umCGylIOuSG02k9KGDwKayt3bzBXtGwVlCQHHkIcnQ/edit#
//
// And the Web Platform Test RFC #72 behind it:
// https://github.com/web-platform-tests/rfcs/blob/master/rfcs/address_space_overrides.md
const char kIpAddressSpaceOverrides[] = "ip-address-space-overrides";

// The switch to disable the shared dictionary storage clean up task. Only for
// testing.
const char kDisableSharedDictionaryStorageCleanupForTesting[] =
    "disable-shared-dictionary-storage-cleanup-for-testing";

}  // namespace network::switches
