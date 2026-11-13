// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <optional>
#import <string>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/data_type.h"
#import "components/sync/base/features.h"
#import "components/sync/protocol/encryption.pb.h"
#import "components/sync_tab_context/fake_ephemeral_key_server.h"
#import "components/sync_tab_context/http_rpc_constants.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/sync/test/tab_context_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

namespace {

constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

NSString* const kTestContainerUuid = @"12345678-1234-1234-1234-123456789abc";
NSString* const kTestAccessToken = @"remote_ephemeral_access_token";
NSString* const kMismatchedGaiaId = @"mismatched_gaia_id";
NSString* const kTestEntryId = @"entry_ephemeral";
constexpr char kTestPageContext[] =
    "Page context payload uploaded via ephemeral container.";

// Holds the shared `EmbeddedTestServer` and `FakeEphemeralKeyServer` across
// tests in `TabContextDecryptionTokenTestCase` so the server port (and
// therefore `AppLaunchConfiguration`) remains stable across test methods.
struct SharedTabContextTestServer {
  std::optional<net::EmbeddedTestServer> test_server;
  std::optional<sync_tab_context::FakeEphemeralKeyServer> ephemeral_key_server;

  void EnsureStarted() {
    if (test_server.has_value()) {
      return;
    }
    test_server.emplace();
    net::test_server::RegisterDefaultHandlers(&*test_server);
    CHECK(test_server->InitializeAndListen());
    ephemeral_key_server.emplace(test_server->base_url());
    test_server->RegisterRequestHandler(base::BindRepeating(
        &sync_tab_context::FakeEphemeralKeyServer::HandleRequest,
        base::Unretained(&*ephemeral_key_server)));
    test_server->StartAcceptingConnections();
  }

  void ShutDown() {
    if (!test_server.has_value()) {
      return;
    }
    CHECK(test_server->ShutdownAndWaitUntilComplete());
    ephemeral_key_server.reset();
    test_server.reset();
  }
};

SharedTabContextTestServer& GetSharedServer() {
  static base::NoDestructor<SharedTabContextTestServer> instance;
  return *instance;
}

// Loads the `/echo` page on the allowed origin and waits for it to commit.
void LoadAllowedOriginPage() {
  const GURL allowed_url = GetSharedServer().test_server->GetURL("/echo");
  [ChromeEarlGrey loadURL:allowed_url];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

// Signs in with `fakeIdentity1` and waits for Sync transport to become active.
FakeSystemIdentity* SignInAndWaitForSyncTransport() {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fake_identity];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
  return fake_identity;
}

// Returns whether `window.chrome.getContainerDecryptionToken` is exposed as a
// function in the active WebState.
bool IsApiDefined() {
  base::Value result = [ChromeEarlGrey
      evaluateJavaScript:@"typeof window.chrome?.getContainerDecryptionToken "
                         @"=== 'function'"];
  return result.is_bool() && result.GetBool();
}

// Invokes `chrome.getContainerDecryptionToken` in the active WebState and waits
// for its callback to complete. Returns the raw bytes from the `ArrayBuffer`
// reply as a `std::string`, or `std::nullopt` if the callback received `null`.
std::optional<std::string> CallGetContainerDecryptionToken(
    NSString* gaia_id,
    NSString* container_id) {
  [ChromeEarlGrey
      waitForJavaScriptCondition:
          @"typeof window.chrome?.getContainerDecryptionToken === 'function'"];
  NSString* script = [NSString
      stringWithFormat:@"window.tokenDone = false;"
                        "window.tokenResult = null;"
                        "chrome.getContainerDecryptionToken(function(buffer) {"
                        "  if (buffer instanceof ArrayBuffer) {"
                        "    window.tokenResult = Array.from("
                        "        new Uint8Array(buffer));"
                        "  }"
                        "  window.tokenDone = true;"
                        "}, '%@', '%@');",
                       gaia_id, container_id];
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  [ChromeEarlGrey waitForJavaScriptCondition:@"window.tokenDone === true"];
  base::Value result =
      [ChromeEarlGrey evaluateJavaScript:@"window.tokenResult"];
  if (!result.is_list()) {
    return std::nullopt;
  }
  std::string token;
  token.reserve(result.GetList().size());
  for (const base::Value& byte_value : result.GetList()) {
    if (!byte_value.is_int() && !byte_value.is_double()) {
      return std::nullopt;
    }
    token.push_back(static_cast<char>(byte_value.GetDouble()));
  }
  return token;
}

// Waits for `entity_count` entities of `entity_type` on the fake sync server.
void WaitForEntitiesOnFakeServer(int entity_count,
                                 syncer::DataType entity_type) {
  ConditionBlock condition = ^{
    return [ChromeEarlGrey numberOfSyncEntitiesWithType:entity_type] ==
           entity_count;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(kSyncOperationTimeout,
                                                          condition),
             @"Expected %d %s entities but found %d", entity_count,
             std::string(syncer::DataTypeToDebugString(entity_type)).c_str(),
             [ChromeEarlGrey numberOfSyncEntitiesWithType:entity_type]);
}

}  // namespace

// End-to-end EarlGrey tests for `TabContextDecryptionTokenJavaScriptFeature`
// and `TabContextSyncService`.
@interface TabContextDecryptionTokenTestCase : ChromeTestCase
@end

@implementation TabContextDecryptionTokenTestCase

+ (void)tearDown {
  GetSharedServer().ShutDown();
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  GetSharedServer().EnsureStarted();
  const GURL& baseURL = GetSharedServer().test_server->base_url();

  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(syncer::kSyncEncryptedTabContext);
  config.additional_args.push_back(
      base::StrCat({"--", syncer::kSyncShortNudgeDelayForTest}));
  config.additional_args.push_back(
      base::StrCat({"--", sync_tab_context::kTabContextAllowedOriginSwitch, "=",
                    baseURL.spec()}));
  config.additional_args.push_back(base::StrCat(
      {"--", sync_tab_context::kEphemeralKeyServerUrlSwitch, "=",
       sync_tab_context::FakeEphemeralKeyServer::GetServerURL(baseURL)
           .spec()}));
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearFakeSyncServerData];
}

- (void)tearDownHelper {
  [ChromeEarlGrey clearFakeSyncServerData];
  [super tearDownHelper];
}

// Tests that `chrome.getContainerDecryptionToken` is not exposed on a
// disallowed origin (`http://localhost:<port>` when the allowed origin is
// `http://127.0.0.1:<port>`).
- (void)testShouldNotExposeApiOnDisallowedOrigin {
  const GURL disallowedURL =
      GetSharedServer().test_server->GetURL("localhost", "/echo");
  [ChromeEarlGrey loadURL:disallowedURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  GREYAssertFalse(IsApiDefined(),
                  @"API should not be exposed on disallowed origin");
}

// Tests that `chrome.getContainerDecryptionToken` is exposed on the allowed
// origin.
- (void)testShouldExposeApiOnAllowedOrigin {
  LoadAllowedOriginPage();

  GREYAssertTrue(IsApiDefined(), @"API should be exposed on allowed origin");
}

// Tests that `chrome.getContainerDecryptionToken` resolves to `null` when the
// requested Gaia ID does not match the signed-in primary account.
- (void)testShouldReturnNullOnGaiaIdMismatch {
  [TabContextAppInterface injectEphemeralContainerWithID:kTestContainerUuid
                                             accessToken:kTestAccessToken];

  SignInAndWaitForSyncTransport();
  LoadAllowedOriginPage();

  const std::optional<std::string> token =
      CallGetContainerDecryptionToken(kMismatchedGaiaId, kTestContainerUuid);
  GREYAssertFalse(token.has_value(),
                  @"Expected null token on Gaia ID mismatch");
}

// Tests that a container downloaded from Sync can be queried via
// `chrome.getContainerDecryptionToken` on the allowed origin.
- (void)testShouldDownloadRemoteContainerAndReturnToken {
  [TabContextAppInterface injectEphemeralContainerWithID:kTestContainerUuid
                                             accessToken:kTestAccessToken];

  FakeSystemIdentity* fakeIdentity = SignInAndWaitForSyncTransport();
  LoadAllowedOriginPage();

  const std::optional<std::string> token = CallGetContainerDecryptionToken(
      fakeIdentity.gaiaId.ToNSString(), kTestContainerUuid);
  GREYAssertTrue(token.has_value(), @"Expected valid token from JS bridge");
  GREYAssertEqualObjects(base::SysUTF8ToNSString(*token), kTestAccessToken,
                         @"Unexpected token value");
}

// Tests end-to-end creation of an ephemeral container, uploading encrypted page
// context to the Sync server, retrieving the container decryption token via the
// JavaScript bridge, and decrypting the uploaded payload.
- (void)testShouldCreateEphemeralContainerUploadPageContextAndDecrypt {
  FakeSystemIdentity* fakeIdentity = SignInAndWaitForSyncTransport();

  NSString* containerID = [TabContextAppInterface createEphemeralContainer];
  GREYAssertNotNil(containerID, @"Failed to create ephemeral container");

  GREYAssertTrue([TabContextAppInterface
                     uploadPageContext:base::SysUTF8ToNSString(kTestPageContext)
                               entryID:kTestEntryId
                           containerID:containerID],
                 @"Failed to upload page context");

  WaitForEntitiesOnFakeServer(1, syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER);
  WaitForEntitiesOnFakeServer(1, syncer::ENCRYPTED_TAB_CONTEXT_ITEM);

  NSData* encryptedData = [TabContextAppInterface
      encryptedContentForServerItemWithContainerID:containerID
                                           entryID:kTestEntryId];
  GREYAssertNotNil(encryptedData,
                   @"Missing encrypted content on fake sync server");

  LoadAllowedOriginPage();

  const std::optional<std::string> accessToken =
      CallGetContainerDecryptionToken(fakeIdentity.gaiaId.ToNSString(),
                                      containerID);
  GREYAssertTrue(accessToken.has_value(),
                 @"Expected valid access token from JS bridge");

  sync_pb::EncryptedData encryptedContent;
  GREYAssertTrue(encryptedContent.ParseFromArray(encryptedData.bytes,
                                                 encryptedData.length),
                 @"Failed to parse EncryptedData proto");
  const std::optional<std::string> decrypted =
      GetSharedServer().ephemeral_key_server->DecryptContent(*accessToken,
                                                             encryptedContent);
  GREYAssertTrue(decrypted.has_value(),
                 @"Failed to decrypt content with token from JS bridge");
  GREYAssertTrue(*decrypted == kTestPageContext,
                 @"Decrypted content did not match uploaded page context");
}

@end
