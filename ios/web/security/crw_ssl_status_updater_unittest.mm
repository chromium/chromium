// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/security/crw_ssl_status_updater.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/legacy_navigation_manager_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/security/ssl_status.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/security/wk_web_view_security_util.h"
#import "ios/web/test/fakes/fake_navigation_manager_delegate.h"
#include "net/cert/x509_util_ios_and_mac.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Mocks CRWSSLStatusUpdaterTestDataSource.
@interface CRWSSLStatusUpdaterTestDataSource
    : NSObject <CRWSSLStatusUpdaterDataSource> {
  StatusQueryHandler _verificationCompletionHandler;
}

// Yes if |SSLStatusUpdater:querySSLStatusForTrust:host:completionHandler| was
// called.
@property(nonatomic, readonly) BOOL certVerificationRequested;

// Calls completion handler passed in
// |SSLStatusUpdater:querySSLStatusForTrust:host:completionHandler|.
- (void)finishVerificationWithCertStatus:(net::CertStatus)certStatus
                           securityStyle:(web::SecurityStyle)securityStyle;

@end

@implementation CRWSSLStatusUpdaterTestDataSource

- (BOOL)certVerificationRequested {
  return _verificationCompletionHandler ? YES : NO;
}

- (void)finishVerificationWithCertStatus:(net::CertStatus)certStatus
                           securityStyle:(web::SecurityStyle)securityStyle {
  _verificationCompletionHandler(securityStyle, certStatus);
}

#pragma mark CRWSSLStatusUpdaterDataSource

- (void)SSLStatusUpdater:(CRWSSLStatusUpdater*)SSLStatusUpdater
    querySSLStatusForTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                      host:(NSString*)host
         completionHandler:(StatusQueryHandler)completionHandler {
  _verificationCompletionHandler = [completionHandler copy];
}

@end

namespace web {

namespace {
// Generated cert filename.
const char kCertFileName[] = "ok_cert.pem";
// Test hostname for cert verification.
NSString* const kHostName = @"www.example.com";
// Test https url for cert verification.
const char kHttpsUrl[] = "https://www.example.com";
// Test http url for cert verification.
const char kHttpUrl[] = "http://www.example.com";
}  // namespace

// Test fixture to test CRWSSLStatusUpdater class.
class CRWSSLStatusUpdaterTest : public web::WebTest {
 protected:
  void SetUp() override {
    web::WebTest::SetUp();

    data_source_ = [[CRWSSLStatusUpdaterTestDataSource alloc] init];
    delegate_ =
        [OCMockObject mockForProtocol:@protocol(CRWSSLStatusUpdaterDelegate)];

    nav_manager_.reset(new LegacyNavigationManagerImpl());
    nav_manager_->SetBrowserState(GetBrowserState());
    nav_manager_->SetDelegate(&nav_delegate_);

    ssl_status_updater_ =
        [[CRWSSLStatusUpdater alloc] initWithDataSource:data_source_
                                      navigationManager:nav_manager_.get()];
    [ssl_status_updater_ setDelegate:delegate_];

    // Create test cert chain.
    scoped_refptr<net::X509Certificate> cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), kCertFileName);
    ASSERT_TRUE(cert);
    base::ScopedCFTypeRef<CFMutableArrayRef> chain(
        net::x509_util::CreateSecCertificateArrayForX509Certificate(
            cert.get()));
    ASSERT_TRUE(chain);
    trust_ = CreateServerTrustFromChain(base::mac::CFToNSCast(chain.get()),
                                        kHostName);
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
    web::WebTest::TearDown();
  }

  // Creates an autoreleased session controller with a single committed entry
  // and link it with nav_manager_.
  void CreateSessionControllerWithEntry(std::string item_url_spec) {
    std::vector<std::unique_ptr<web::NavigationItem>> nav_items;
    CRWSessionController* session_controller =
        [[CRWSessionController alloc] initWithBrowserState:GetBrowserState()
                                           navigationItems:std::move(nav_items)
                                    lastCommittedItemIndex:0];
    nav_manager_->SetSessionController(session_controller);
    [session_controller
                 addPendingItem:GURL(item_url_spec)
                       referrer:Referrer()
                     transition:ui::PAGE_TRANSITION_LINK
                 initiationType:web::NavigationInitiationType::BROWSER_INITIATED
        userAgentOverrideOption:NavigationManager::UserAgentOverrideOption::
                                    INHERIT];
    [session_controller commitPendingItem];
  }

  CRWSSLStatusUpdaterTestDataSource* data_source_;
  id delegate_;
  std::unique_ptr<web::NavigationManagerImpl> nav_manager_;
  FakeNavigationManagerDelegate nav_delegate_;
  CRWSSLStatusUpdater* ssl_status_updater_;
  base::ScopedCFTypeRef<SecTrustRef> trust_;
};

// Tests that CRWSSLStatusUpdater init returns non nil object.
TEST_F(CRWSSLStatusUpdaterTest, Initialization) {
  EXPECT_TRUE(ssl_status_updater_);
}

// Tests updating http navigation item.
TEST_F(CRWSSLStatusUpdaterTest, HttpItem) {
  CreateSessionControllerWithEntry(kHttpUrl);
  web::NavigationItem* item = nav_manager_->GetLastCommittedItem();
  // Make sure that item change callback was called.
  [[delegate_ expect] SSLStatusUpdater:ssl_status_updater_
      didChangeSSLStatusForNavigationItem:item];

  [ssl_status_updater_ updateSSLStatusForNavigationItem:item
                                           withCertHost:kHostName
                                                  trust:trust_
                                   hasOnlySecureContent:NO];

  // No certificate for http.
  EXPECT_FALSE(!!item->GetSSL().certificate);

  // Always normal content for http.
  EXPECT_EQ(web::SSLStatus::NORMAL_CONTENT, item->GetSSL().content_status);

  // Make sure that security style and content status did change.
  EXPECT_EQ(web::SECURITY_STYLE_UNAUTHENTICATED, item->GetSSL().security_style);
}

// Tests that delegate callback is not called if no changes were made to http
// navigation item.
TEST_F(CRWSSLStatusUpdaterTest, NoChangesToHttpItem) {
  CreateSessionControllerWithEntry(kHttpUrl);
  web::NavigationItem* item = nav_manager_->GetLastCommittedItem();
  item->GetSSL().security_style = SECURITY_STYLE_UNAUTHENTICATED;

  [ssl_status_updater_ updateSSLStatusForNavigationItem:item
                                           withCertHost:kHostName
                                                  trust:trust_
                                   hasOnlySecureContent:YES];
  // No certificate for http.
  EXPECT_FALSE(!!item->GetSSL().certificate);
  // Make sure that security style did not change.
  EXPECT_EQ(web::SECURITY_STYLE_UNAUTHENTICATED, item->GetSSL().security_style);
}

// Tests updating https navigation item without cert.
TEST_F(CRWSSLStatusUpdaterTest, HttpsItemNoCert) {
  CreateSessionControllerWithEntry(kHttpsUrl);
  web::NavigationItem* item = nav_manager_->GetLastCommittedItem();
  // Change default value to test that |item| is actually changed.
  item->GetSSL().security_style = SECURITY_STYLE_UNAUTHENTICATED;

  // Make sure that item change callback was called.
  [[delegate_ expect] SSLStatusUpdater:ssl_status_updater_
      didChangeSSLStatusForNavigationItem:item];

  [ssl_status_updater_
      updateSSLStatusForNavigationItem:item
                          withCertHost:kHostName
                                 trust:base::ScopedCFTypeRef<SecTrustRef>()
                  hasOnlySecureContent:YES];
  // No certificate.
  EXPECT_FALSE(!!item->GetSSL().certificate);
  // Make sure that security style did change.
  EXPECT_EQ(web::SECURITY_STYLE_UNKNOWN, item->GetSSL().security_style);
  EXPECT_EQ(web::SSLStatus::NORMAL_CONTENT, item->GetSSL().content_status);
}

// Tests that unnecessary cert verification does not happen if SSL status has
// already been calculated and the only change was appearing of mixed content.
TEST_F(CRWSSLStatusUpdaterTest, HttpsItemNoCertReverification) {
  CreateSessionControllerWithEntry(kHttpsUrl);
  web::NavigationItem* item = nav_manager_->GetLastCommittedItem();
  // Set SSL status manually in the way so cert re-verification is not run.
  item->GetSSL().cert_status_host = base::SysNSStringToUTF8(kHostName);
  item->GetSSL().certificate = web::CreateCertFromTrust(trust_);

  // Make sure that item change callback was called.
  [[delegate_ expect] SSLStatusUpdater:ssl_status_updater_
      didChangeSSLStatusForNavigationItem:item];

  [ssl_status_updater_ updateSSLStatusForNavigationItem:item
                                           withCertHost:kHostName
                                                  trust:trust_
                                   hasOnlySecureContent:NO];
  // Make sure that cert verification did not run.
  EXPECT_FALSE([data_source_ certVerificationRequested]);

  // Make sure that security style and content status did change.
  EXPECT_EQ(web::SECURITY_STYLE_UNKNOWN, item->GetSSL().security_style);
  EXPECT_EQ(web::SSLStatus::DISPLAYED_INSECURE_CONTENT,
            item->GetSSL().content_status);
}

// Tests updating https navigation item.
TEST_F(CRWSSLStatusUpdaterTest, HttpsItem) {
  CreateSessionControllerWithEntry(kHttpsUrl);
  web::NavigationItem* item = nav_manager_->GetLastCommittedItem();

  // Make sure that item change callback was called twice for changing
  // content_status and security style.
  [[delegate_ expect] SSLStatusUpdater:ssl_status_updater_
      didChangeSSLStatusForNavigationItem:item];
  [[delegate_ expect] SSLStatusUpdater:ssl_status_updater_
      didChangeSSLStatusForNavigationItem:item];

  [ssl_status_updater_ updateSSLStatusForNavigationItem:item
                                           withCertHost:kHostName
                                                  trust:trust_
                                   hasOnlySecureContent:NO];

  // Make sure that cert verification was requested.
  EXPECT_TRUE([data_source_ certVerificationRequested]);

  // Make sure that security style and cert status are reset during
  // verification.
  EXPECT_EQ(web::SECURITY_STYLE_UNKNOWN, item->GetSSL().security_style);
  EXPECT_FALSE(item->GetSSL().cert_status);

  // Reply with calculated cert verification status.
  [data_source_
      finishVerificationWithCertStatus:net::CERT_STATUS_ALL_ERRORS
                         securityStyle:
                             web::SECURITY_STYLE_AUTHENTICATION_BROKEN];

  // Make sure that security style and content status did change.
  EXPECT_EQ(web::SECURITY_STYLE_AUTHENTICATION_BROKEN,
            item->GetSSL().security_style);
  EXPECT_EQ(web::SSLStatus::DISPLAYED_INSECURE_CONTENT,
            item->GetSSL().content_status);
}

// Tests that SSL status is not changed if navigation item host changed during
// verification (e.g. because of redirect).
TEST_F(CRWSSLStatusUpdaterTest, HttpsItemChangeUrlDuringUpdate) {
  CreateSessionControllerWithEntry(kHttpsUrl);
  web::NavigationItem* item = nav_manager_->GetLastCommittedItem();

  // Make sure that item change callback was called once for changing
  // content_status.
  [[delegate_ expect] SSLStatusUpdater:ssl_status_updater_
      didChangeSSLStatusForNavigationItem:item];

  [ssl_status_updater_ updateSSLStatusForNavigationItem:item
                                           withCertHost:kHostName
                                                  trust:trust_
                                   hasOnlySecureContent:YES];

  // Make sure that cert verification was requested.
  EXPECT_TRUE([data_source_ certVerificationRequested]);

  // Make sure that security style and cert status are reset during
  // verification.
  EXPECT_EQ(web::SECURITY_STYLE_UNKNOWN, item->GetSSL().security_style);
  EXPECT_FALSE(item->GetSSL().cert_status);

  // Change the host during the verification.
  item->SetURL(GURL("www.attacker.org"));

  // Reply with calculated cert verification status.
  [data_source_
      finishVerificationWithCertStatus:0
                         securityStyle:web::SECURITY_STYLE_AUTHENTICATED];

  // Make sure that security style and content status did change.
  EXPECT_EQ(web::SECURITY_STYLE_UNKNOWN, item->GetSSL().security_style);
  EXPECT_EQ(web::SSLStatus::NORMAL_CONTENT, item->GetSSL().content_status);
}

// Tests that SSL status is not changed if navigation item has downgraded to
// http.
TEST_F(CRWSSLStatusUpdaterTest, HttpsItemDowngrade) {
  CreateSessionControllerWithEntry(kHttpsUrl);
  web::NavigationItem* item = nav_manager_->GetLastCommittedItem();

  // Make sure that item change callback was called.
  [[delegate_ expect] SSLStatusUpdater:ssl_status_updater_
      didChangeSSLStatusForNavigationItem:item];

  [ssl_status_updater_ updateSSLStatusForNavigationItem:item
                                           withCertHost:kHostName
                                                  trust:trust_
                                   hasOnlySecureContent:YES];

  // Make sure that cert verification was requested.
  EXPECT_TRUE([data_source_ certVerificationRequested]);

  // Make sure that security style and cert status are reset during
  // verification.
  EXPECT_EQ(web::SECURITY_STYLE_UNKNOWN, item->GetSSL().security_style);
  EXPECT_FALSE(item->GetSSL().cert_status);

  // Downgrade to http.
  item->SetURL(GURL(kHttpUrl));

  // Reply with calculated cert verification status.
  [data_source_
      finishVerificationWithCertStatus:0
                         securityStyle:web::SECURITY_STYLE_AUTHENTICATED];

  // Make sure that security style and content status did change.
  EXPECT_EQ(web::SECURITY_STYLE_UNKNOWN, item->GetSSL().security_style);
  EXPECT_EQ(web::SSLStatus::NORMAL_CONTENT, item->GetSSL().content_status);
}

// Tests that SSL status is not changed if navigation item's cert is changed.
TEST_F(CRWSSLStatusUpdaterTest, CertChanged) {
  CreateSessionControllerWithEntry(kHttpsUrl);
  web::NavigationItem* item = nav_manager_->GetLastCommittedItem();

  // Make sure that item change callback was called.
  [[delegate_ expect] SSLStatusUpdater:ssl_status_updater_
      didChangeSSLStatusForNavigationItem:item];

  [ssl_status_updater_ updateSSLStatusForNavigationItem:item
                                           withCertHost:kHostName
                                                  trust:trust_
                                   hasOnlySecureContent:YES];

  // Make sure that cert verification was requested.
  EXPECT_TRUE([data_source_ certVerificationRequested]);

  // Make sure that security style and cert status are reset during
  // verification.
  EXPECT_EQ(web::SECURITY_STYLE_UNKNOWN, item->GetSSL().security_style);
  EXPECT_FALSE(item->GetSSL().cert_status);

  // Change the cert.
  item->GetSSL().certificate = nullptr;

  // Reply with calculated cert verification status.
  [data_source_
      finishVerificationWithCertStatus:0
                         securityStyle:web::SECURITY_STYLE_AUTHENTICATED];

  // Make sure that security style and content status did change.
  EXPECT_EQ(web::SECURITY_STYLE_UNKNOWN, item->GetSSL().security_style);
  EXPECT_EQ(web::SSLStatus::NORMAL_CONTENT, item->GetSSL().content_status);
}

}  // namespace web
