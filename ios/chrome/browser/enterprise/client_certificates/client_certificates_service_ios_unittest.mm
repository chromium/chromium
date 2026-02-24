// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/client_certificates_service_ios.h"

#import <memory>
#import <utility>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/enterprise/client_certificates/core/mock_private_key.h"
#import "components/enterprise/client_certificates/ios/certificate_provisioning_service_ios.h"
#import "components/enterprise/client_certificates/ios/client_identity_ios.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "net/cert/x509_certificate.h"
#import "net/test/cert_test_util.h"
#import "net/test/test_data_directory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using testing::_;
using testing::StrictMock;

namespace client_certificates {

namespace {

const char kRequestingUrl[] = "https://www.example.com";

class MockCertificateProvisioningServiceIOS
    : public CertificateProvisioningServiceIOS {
 public:
  MockCertificateProvisioningServiceIOS() = default;
  ~MockCertificateProvisioningServiceIOS() override = default;

  MOCK_METHOD(void,
              GetManagedIdentity,
              (GetManagedIdentityCallback),
              (override));
  MOCK_METHOD(void,
              DeleteManagedIdentities,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(Status, GetCurrentStatus, (), (const, override));
  MOCK_METHOD(void,
              GetManagedIdentityIOS,
              (GetManagedIdentityIOSCallback),
              (override));
};

}  // namespace

class ClientCertificatesServiceIOSImplTest : public PlatformTest {
 protected:
  void SetUp() override {
    profile_ = TestProfileIOS::Builder().Build();

    profile_provisioning_service_ =
        std::make_unique<StrictMock<MockCertificateProvisioningServiceIOS>>();
    browser_provisioning_service_ =
        std::make_unique<StrictMock<MockCertificateProvisioningServiceIOS>>();
    service_ = ClientCertificatesServiceIOS::Create(
        profile_.get(), profile_provisioning_service_.get(),
        browser_provisioning_service_.get());

    client_1_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
    ASSERT_TRUE(client_1_);
    client_2_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_2.pem");
    ASSERT_TRUE(client_2_);
  }

  void TearDown() override {
    HostContentSettingsMap* m =
        ios::HostContentSettingsMapFactory::GetForProfile(profile_.get());
    m->ClearSettingsForOneType(ContentSettingsType::AUTO_SELECT_CERTIFICATE);
  }

  void SetPolicyValueInContentSettings(base::ListValue filters) {
    HostContentSettingsMap* m =
        ios::HostContentSettingsMapFactory::GetForProfile(profile_.get());

    base::DictValue root;
    root.Set("filters", std::move(filters));

    m->SetWebsiteSettingDefaultScope(
        GURL(kRequestingUrl), GURL(),
        ContentSettingsType::AUTO_SELECT_CERTIFICATE,
        base::Value(std::move(root)));
  }

  base::DictValue CreateFilterValue(const std::string& issuer,
                                    const std::string& subject) {
    base::DictValue filter;
    if (!issuer.empty()) {
      filter.Set("ISSUER", base::DictValue().Set("CN", issuer));
    }

    if (!subject.empty()) {
      filter.Set("SUBJECT", base::DictValue().Set("CN", subject));
    }

    return filter;
  }

  ClientIdentityIOS CreateIdentity(const std::string& name,
                                   scoped_refptr<net::X509Certificate> cert) {
    auto private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
    ClientIdentity core_identity(name, private_key, cert);

    // Manually set identity_ref to make it valid for the test logic.
    base::apple::ScopedCFTypeRef<CFArrayRef> dummy_array(
        CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));

    return ClientIdentityIOS::CreateForTesting(
        core_identity, base::apple::ScopedCFTypeRef<SecIdentityRef>(
                           (SecIdentityRef)dummy_array.release()));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;

  std::unique_ptr<MockCertificateProvisioningServiceIOS>
      profile_provisioning_service_;
  std::unique_ptr<MockCertificateProvisioningServiceIOS>
      browser_provisioning_service_;

  std::unique_ptr<ClientCertificatesServiceIOS> service_;

  scoped_refptr<net::X509Certificate> client_1_;
  scoped_refptr<net::X509Certificate> client_2_;
};

// Tests that when no auto-selection policy is configured, the service returns
// no identity even if certificates are available in the provisioning services.
TEST_F(ClientCertificatesServiceIOSImplTest, NoPolicyAppliedReturnsNoMatch) {
  EXPECT_CALL(*browser_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([](auto callback) { std::move(callback).Run(std::nullopt); });
  EXPECT_CALL(*profile_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("profile", client_1_));
      });

  base::test::TestFuture<std::unique_ptr<ClientIdentityIOS>> test_future;
  service_->GetAutoSelectedIdentity(GURL(kRequestingUrl),
                                    test_future.GetCallback());

  auto result = test_future.Take();
  EXPECT_FALSE(result);
}

// Tests that when a policy with an issuer filter is applied, the service
// correctly returns the identity that matches that issuer.
TEST_F(ClientCertificatesServiceIOSImplTest,
       SingleIssuerFilterPolicySelectsMatchingCert) {
  // client_1.pem has "B CA" as its issuer.
  base::ListValue filters;
  filters.Append(CreateFilterValue("B CA", ""));
  SetPolicyValueInContentSettings(std::move(filters));

  EXPECT_CALL(*browser_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("browser", client_2_));
      });
  EXPECT_CALL(*profile_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("profile", client_1_));
      });

  base::test::TestFuture<std::unique_ptr<ClientIdentityIOS>> test_future;
  service_->GetAutoSelectedIdentity(GURL(kRequestingUrl),
                                    test_future.GetCallback());

  auto result = test_future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->name(), "profile");
  EXPECT_EQ(result->certificate(), client_1_);
}

// Tests that when a policy with a subject filter is applied, the service
// correctly returns the identity that matches that subject.
TEST_F(ClientCertificatesServiceIOSImplTest,
       SingleSubjectFilterPolicySelectsMatchingCert) {
  // client_1.pem has "Client Cert A" as its subject.
  base::ListValue filters;
  filters.Append(CreateFilterValue("", "Client Cert A"));
  SetPolicyValueInContentSettings(std::move(filters));

  EXPECT_CALL(*browser_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("browser", client_1_));
      });
  EXPECT_CALL(*profile_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("profile", client_2_));
      });

  base::test::TestFuture<std::unique_ptr<ClientIdentityIOS>> test_future;
  service_->GetAutoSelectedIdentity(GURL(kRequestingUrl),
                                    test_future.GetCallback());

  auto result = test_future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->name(), "browser");
  EXPECT_EQ(result->certificate(), client_1_);
}

// Tests that the service returns a valid identity when multiple certificates
// match the applied filters.
TEST_F(ClientCertificatesServiceIOSImplTest, MultipleCertsMatchPolicy) {
  base::ListValue filters;
  filters.Append(CreateFilterValue("B CA", ""));
  filters.Append(CreateFilterValue("E CA", ""));
  SetPolicyValueInContentSettings(std::move(filters));

  EXPECT_CALL(*browser_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("browser", client_1_));
      });
  EXPECT_CALL(*profile_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("profile", client_2_));
      });

  base::test::TestFuture<std::unique_ptr<ClientIdentityIOS>> test_future;
  service_->GetAutoSelectedIdentity(GURL(kRequestingUrl),
                                    test_future.GetCallback());

  auto result = test_future.Take();
  ASSERT_TRUE(result);
}

// Tests that no identity is returned if the issuer filter in the policy
// doesn't match any of the certificates provided by the services.
TEST_F(ClientCertificatesServiceIOSImplTest,
       IssuerNotMatchingDoesntSelectCerts) {
  base::ListValue filters;
  filters.Append(CreateFilterValue("Bad Issuer", ""));
  SetPolicyValueInContentSettings(std::move(filters));

  EXPECT_CALL(*browser_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("browser", client_1_));
      });
  EXPECT_CALL(*profile_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("profile", client_2_));
      });

  base::test::TestFuture<std::unique_ptr<ClientIdentityIOS>> test_future;
  service_->GetAutoSelectedIdentity(GURL(kRequestingUrl),
                                    test_future.GetCallback());

  auto result = test_future.Take();
  EXPECT_FALSE(result);
}

// Tests that no identity is returned if the subject filter in the policy
// doesn't match any of the certificates provided by the services.
TEST_F(ClientCertificatesServiceIOSImplTest,
       SubjectNotMatchingDoesntSelectCerts) {
  base::ListValue filters;
  filters.Append(CreateFilterValue("", "Bad Subject"));
  SetPolicyValueInContentSettings(std::move(filters));

  EXPECT_CALL(*browser_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("browser", client_1_));
      });
  EXPECT_CALL(*profile_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("profile", client_2_));
      });

  base::test::TestFuture<std::unique_ptr<ClientIdentityIOS>> test_future;
  service_->GetAutoSelectedIdentity(GURL(kRequestingUrl),
                                    test_future.GetCallback());

  auto result = test_future.Take();
  EXPECT_FALSE(result);
}

// Tests that a matching certificate is not selected if the requesting URL
// is different from the one for which the policy was configured.
TEST_F(ClientCertificatesServiceIOSImplTest,
       MatchingCertOnDifferentUrlDoesntSelectCerts) {
  base::ListValue filters;
  filters.Append(CreateFilterValue("B CA", ""));
  SetPolicyValueInContentSettings(std::move(filters));

  EXPECT_CALL(*browser_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("browser", client_1_));
      });
  EXPECT_CALL(*profile_provisioning_service_, GetManagedIdentityIOS(_))
      .WillOnce([this](auto callback) {
        std::move(callback).Run(CreateIdentity("profile", client_2_));
      });

  base::test::TestFuture<std::unique_ptr<ClientIdentityIOS>> test_future;
  service_->GetAutoSelectedIdentity(GURL("https://other.url.com"),
                                    test_future.GetCallback());

  auto result = test_future.Take();
  EXPECT_FALSE(result);
}

}  // namespace client_certificates
