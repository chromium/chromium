// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import <optional>
#import <string>
#import <string_view>

#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/enterprise/browser/enterprise_switches.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/features.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

namespace {

// Page served by the embedded test server for all tests.
const char kTestPagePath[] = "/echo";

// JavaScript expression evaluating to the type of the Device Trust API object.
NSString* const kDeviceTrustAPIType =
    @"typeof window.chrome?.enterprise?.deviceTrust";

// JavaScript condition which is true once `getAttestation()` is installed.
NSString* const kDeviceTrustAPIInstalled =
    @"typeof window.chrome?.enterprise?.deviceTrust?.getAttestation === "
    @"'function'";

// A challenge which is a syntactically valid request (JSON with a base64
// encoded `challenge`, decoding to a well-formed `SignedData` proto) but whose
// signature was not produced by Verified Access. It passes every local check,
// so it exercises the full native pipeline (signals collection and challenge
// verification) before being rejected.
NSString* const kUnverifiedChallenge = @"'{\"challenge\":\"CgFhEgFi\"}'";

// A challenge signed by the production Verified Access key, shared with
// `device_trust_service_unittest.cc` and the desktop
// `device_trust_browsertest_base.cc`. It is accepted by challenge verification
// and therefore answered with a challenge response.
NSString* const kVerifiedAccessChallenge =
    @"'{\"challenge\":\""
    @"CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIELlPXqh8+rZJ2VIqwPXtPFrr653QdRrIzHF"
    @"wqP+b3L8GJTcufirLxKAAkindNwTfwYUcbCFDjiW3kXdmDPE0wC0J6b5ZI6X6vOVcSMXTpK7"
    @"nxsAGKzFV+i80LCnfwUZn7Ne1bHzloAqBdpLOu53vQ63hKRk6MRPhc9jYVDsvqXfQ7s+FUA5"
    @"r3lxdoluxwAUMFqcP4VgnMvKzKTPYbnnB+xj5h5BZqjQToXJYoP4VC3/ID+YHNsCWy5o7+G5"
    @"jnq0ak3zeqWfo1+lCibMPsCM+2g7nCZIwvwWlfoKwv3aKvOVMBcJxPAIxH1w+hH+NWxqRi6q"
    @"gZm84q0ylm0ybs6TFjdgLvSViAIp0Z9p/An/u3W4CMboCswxIxNYRCGrIIVPElE3Yb4QS65m"
    @"Krg=\"}'";

// Outcome of a `getAttestation()` call, as observed by the page.
struct AttestationError {
  std::string code;
  std::string message;
};

// Allowlists `host` for the user-level Device Trust connector. The policy is
// `cloud_only`, so it has to be installed with a cloud source.
void AllowlistHost(std::string_view host) {
  base::ListValue allowlist;
  allowlist.Append(host);
  [PolicyAppInterface
      mergeCloudUserPolicyValue:base::SysUTF8ToNSString(
                                    base::WriteJson(allowlist).value())
                         forKey:
                             base::SysUTF8ToNSString(
                                 policy::key::
                                     kUserContextAwareAccessSignalsAllowlist)];
}

// Returns the type of `window.chrome.enterprise.deviceTrust` in the current
// page, e.g. "undefined" or "object".
std::string DeviceTrustAPIType() {
  base::Value type = [ChromeEarlGrey evaluateJavaScript:kDeviceTrustAPIType];
  GREYAssertTrue(type.is_string(), @"`typeof` should evaluate to a string");
  return type.GetString();
}

// Calls `getAttestation()` with `challengeExpression` in the current page and
// records the settled outcome in `window.deviceTrustResult`.
void CallGetAttestation(NSString* challengeExpression) {
  NSString* script = [NSString
      stringWithFormat:
          @"(function() {"
           "  window.deviceTrustResult = null;"
           "  window.chrome.enterprise.deviceTrust.getAttestation(%@)"
           "    .then(function(payload) {"
           "      window.deviceTrustResult = {payload: payload};"
           "    }, function(error) {"
           "      window.deviceTrustResult = {"
           "        code: String(error.code), message: String(error.message)"
           "      };"
           "    });"
           "})();",
          challengeExpression];
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
}

// Waits for the last `getAttestation()` call to settle and returns the
// recorded outcome.
base::DictValue WaitForAttestationResult() {
  [ChromeEarlGrey
      waitForJavaScriptCondition:@"window.deviceTrustResult !== null"];
  base::Value result =
      [ChromeEarlGrey evaluateJavaScript:@"window.deviceTrustResult"];
  GREYAssertTrue(result.is_dict(), @"Result should be a dictionary");
  return std::move(result).TakeDict();
}

// Waits for the last `getAttestation()` call to settle and returns its error.
// Fails the test if the call resolved instead.
AttestationError WaitForAttestationError() {
  base::DictValue result = WaitForAttestationResult();
  const std::string* code = result.FindString("code");
  const std::string* message = result.FindString("message");
  GREYAssertTrue(code && message, @"`getAttestation()` should have rejected");
  return {*code, *message};
}

// Waits for the last `getAttestation()` call to settle and returns the payload
// it resolved with. Fails the test if the call rejected instead.
std::string WaitForAttestationPayload() {
  base::DictValue result = WaitForAttestationResult();
  const std::string* payload = result.FindString("payload");
  GREYAssertTrue(payload, @"`getAttestation()` should have resolved");
  return *payload;
}

// Calls `getAttestation()` with `challengeExpression` and returns the error it
// rejected with.
AttestationError GetAttestationError(NSString* challengeExpression) {
  CallGetAttestation(challengeExpression);
  return WaitForAttestationError();
}

}  // namespace

// Tests the Device Trust JavaScript API when the feature is disabled.
@interface DeviceTrustDisabledTestCase : ChromeTestCase
@end

@implementation DeviceTrustDisabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_disabled.push_back(
      enterprise_connectors::features::kEnableIOSDeviceTrustConnector);
  // Required for the `cloud_only` allowlist policy to be accepted.
  config.additional_args.push_back(
      base::StrCat({"--", switches::kEnableChromeBrowserCloudManagement}));
  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)tearDownHelper {
  [PolicyAppInterface clearPolicies];
  [super tearDownHelper];
}

// Verifies that the API is not installed when the feature is disabled, even on
// an allowlisted page.
- (void)testAPIAbsentWhenFeatureDisabled {
  AllowlistHost(self.testServer->base_url().host());
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPagePath)];
  GREYAssertEqual(std::string("undefined"), DeviceTrustAPIType(),
                  @"API should not be installed when the feature is disabled");
}

@end

// Tests the Device Trust JavaScript API when the feature is enabled.
@interface DeviceTrustEnabledTestCase : ChromeTestCase
@end

@implementation DeviceTrustEnabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(
      enterprise_connectors::features::kEnableIOSDeviceTrustConnector);
  // Required for the `cloud_only` allowlist policy to be accepted.
  config.additional_args.push_back(
      base::StrCat({"--", switches::kEnableChromeBrowserCloudManagement}));
  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)tearDownHelper {
  [PolicyAppInterface clearPolicies];
  [super tearDownHelper];
}

// Allowlists the test server, loads the test page and waits for the API to be
// installed in it.
- (void)loadAllowlistedPage {
  AllowlistHost(self.testServer->base_url().host());
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPagePath)];
  [ChromeEarlGrey waitForJavaScriptCondition:kDeviceTrustAPIInstalled];
}

// Verifies that the API is not installed when no allowlist policy is set.
- (void)testAPIAbsentWithoutAllowlistPolicy {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPagePath)];
  GREYAssertEqual(std::string("undefined"), DeviceTrustAPIType(),
                  @"API should not be installed without an allowlist policy");
}

// Verifies that the API is not installed on a page whose URL is not covered by
// the allowlist policy.
- (void)testAPIAbsentOnNonAllowlistedPage {
  AllowlistHost("example.com");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPagePath)];
  GREYAssertEqual(std::string("undefined"), DeviceTrustAPIType(),
                  @"API should not be installed on a non-allowlisted page");
}

// Verifies that the API is not installed in incognito, even on an allowlisted
// page.
- (void)testAPIAbsentInIncognito {
  AllowlistHost(self.testServer->base_url().host());
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPagePath)];
  GREYAssertEqual(std::string("undefined"), DeviceTrustAPIType(),
                  @"API should not be installed in incognito");
}

// Verifies that the API is installed on an allowlisted page.
- (void)testAPIInstalledOnAllowlistedPage {
  [self loadAllowlistedPage];
  GREYAssertEqual(std::string("object"), DeviceTrustAPIType(),
                  @"API should be installed on an allowlisted page");
}

// Verifies that the API object cannot be tampered with by the page.
- (void)testAPIObjectIsFrozen {
  [self loadAllowlistedPage];
  base::Value frozen = [ChromeEarlGrey
      evaluateJavaScript:
          @"Object.isFrozen(window.chrome.enterprise.deviceTrust)"];
  GREYAssertTrue(frozen.is_bool() && frozen.GetBool(),
                 @"API object should be frozen");
}

// Verifies that a non-string challenge is rejected by the page script.
- (void)testRejectsNonStringChallenge {
  [self loadAllowlistedPage];
  AttestationError error = GetAttestationError(@"12345");
  GREYAssertEqual(std::string("INVALID_CHALLENGE_REQUEST"), error.code,
                  @"Unexpected error code for a non-string challenge");
  GREYAssertEqual(std::string("challengeRequest must be a string."),
                  error.message,
                  @"Unexpected error message for a non-string challenge");
}

// Verifies that a blank challenge is rejected by the page script.
- (void)testRejectsEmptyChallenge {
  [self loadAllowlistedPage];
  AttestationError error = GetAttestationError(@"'   '");
  GREYAssertEqual(std::string("INVALID_CHALLENGE_REQUEST"), error.code,
                  @"Unexpected error code for a blank challenge");
  GREYAssertEqual(std::string("challengeRequest must be non-empty."),
                  error.message,
                  @"Unexpected error message for a blank challenge");
}

// Verifies that a challenge above the size limit is rejected by the page
// script.
- (void)testRejectsOversizedChallenge {
  [self loadAllowlistedPage];
  AttestationError error = GetAttestationError(@"'a'.repeat(1025)");
  GREYAssertEqual(std::string("INVALID_CHALLENGE_REQUEST"), error.code,
                  @"Unexpected error code for an oversized challenge");
  GREYAssertEqual(std::string("challengeRequest is too large."), error.message,
                  @"Unexpected error message for an oversized challenge");
}

// Verifies that a challenge which is not a JSON challenge request reaches the
// native side and is rejected there.
- (void)testRejectsMalformedChallengeNatively {
  [self loadAllowlistedPage];
  AttestationError error = GetAttestationError(@"'not-a-json-challenge'");
  GREYAssertEqual(std::string("INVALID_CHALLENGE_REQUEST"), error.code,
                  @"Unexpected error code for a malformed challenge");
  GREYAssertEqual(std::string("Failed to parse challenge."), error.message,
                  @"Malformed challenge should be rejected by the native side");
}

// Verifies that a well-formed challenge which was not issued by Verified
// Access goes through the whole attestation pipeline and is rejected.
- (void)testRejectsUnverifiedChallenge {
  [self loadAllowlistedPage];
  AttestationError error = GetAttestationError(kUnverifiedChallenge);
  GREYAssertEqual(std::string("INTERNAL_ERROR"), error.code,
                  @"Unexpected error code for an unverified challenge");
}

// Verifies that a challenge issued by Verified Access goes through the whole
// attestation pipeline and is answered with a challenge response.
- (void)testResolvesVerifiedAccessChallenge {
  [self loadAllowlistedPage];
  CallGetAttestation(kVerifiedAccessChallenge);
  std::optional<base::DictValue> payload = base::JSONReader::ReadDict(
      WaitForAttestationPayload(), base::JSON_PARSE_RFC);
  GREYAssertTrue(payload.has_value(), @"Payload should be a JSON dictionary");
  const std::string* challengeResponse =
      payload->FindString("challengeResponse");
  GREYAssertTrue(challengeResponse && !challengeResponse->empty(),
                 @"Payload should carry a non-empty `challengeResponse`");
}

@end
