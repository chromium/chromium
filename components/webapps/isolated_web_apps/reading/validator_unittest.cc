// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/reading/validator.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/test_support/signed_web_bundles/signature_verifier_test_utils.h"
#include "components/webapps/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/webapps/isolated_web_apps/identity/iwa_identity_validator.h"
#include "components/webapps/isolated_web_apps/test_support/signed_web_bundle_utils.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/test_support/test_iwa_client.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::HasSubstr;
using testing::Property;
using testing::Return;

using Error = UnusableSwbnFileError::Error;

auto UnusableSwbnErrorIs(Error type, const std::string& error) {
  return ErrorIs(
      AllOf(Property(&UnusableSwbnFileError::value, type),
            Property(&UnusableSwbnFileError::message, HasSubstr(error))));
}

}  // namespace

class IsolatedWebAppValidatorTest : public ::testing::Test {
 protected:
  IsolatedWebAppValidatorTest() { IwaIdentityValidator::CreateSingleton(); }

  using IntegrityBlockFuture =
      base::test::TestFuture<base::expected<void, std::string>>;

  web_package::SignedWebBundleIntegrityBlock MakeIntegrityBlock(
      const std::vector<web_package::Ed25519PublicKey>& public_keys = {
          test::GetDefaultEd25519KeyPair().public_key}) {
    auto raw_integrity_block = web_package::mojom::BundleIntegrityBlock::New();
    raw_integrity_block->size = 123;
    raw_integrity_block
        ->signature_stack = base::ToVector(public_keys, [](const auto&
                                                               public_key) {
      auto entry =
          web_package::mojom::BundleIntegrityBlockSignatureStackEntry::New();
      entry->signature_info = web_package::mojom::SignatureInfo::NewEd25519(
          web_package::mojom::SignatureInfoEd25519::New());
      entry->signature_info->get_ed25519()->public_key = public_key;
      return entry;
    });

    auto signed_web_bundle_id =
        web_package::SignedWebBundleId::CreateForPublicKey(public_keys[0]);
    raw_integrity_block->attributes =
        web_package::test::GetAttributesForSignedWebBundleId(
            signed_web_bundle_id.id());
    auto integrity_block = web_package::SignedWebBundleIntegrityBlock::Create(
        std::move(raw_integrity_block));
    CHECK(integrity_block.has_value()) << integrity_block.error();

    return *integrity_block;
  }

  test::MockIwaClient& iwa_client() { return iwa_client_; }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  testing::NiceMock<test::MockIwaClient> iwa_client_;
};

using IsolatedWebAppValidatorIntegrityBlockTest = IsolatedWebAppValidatorTest;

TEST_F(IsolatedWebAppValidatorIntegrityBlockTest,
       WebBundleIdAndPublicKeyDiffer) {
  auto integrity_block = MakeIntegrityBlock();

  EXPECT_THAT(IsolatedWebAppValidator::ValidateIntegrityBlock(
                  &browser_context_, test::GetDefaultEcdsaP256WebBundleId(),
                  integrity_block,
                  /*dev_mode=*/false),
              UnusableSwbnErrorIs(Error::kIntegrityBlockValidationError,
                                  "does not match the expected Web Bundle ID"));
}

TEST_F(IsolatedWebAppValidatorIntegrityBlockTest, IWAIsTrusted) {
  auto integrity_block = MakeIntegrityBlock();

  ON_CALL(iwa_client(),
          ValidateTrust(_, test::GetDefaultEd25519WebBundleId(), _))
      .WillByDefault(Return(base::ok()));

  EXPECT_THAT(IsolatedWebAppValidator::ValidateIntegrityBlock(
                  &browser_context_, test::GetDefaultEd25519WebBundleId(),
                  integrity_block,
                  /*dev_mode=*/false),
              HasValue());
}

TEST_F(IsolatedWebAppValidatorIntegrityBlockTest, IWAIsUntrusted) {
  auto integrity_block = MakeIntegrityBlock();

  ON_CALL(iwa_client(), ValidateTrust)
      .WillByDefault(Return(base::unexpected("public key(s) are not trusted")));

  EXPECT_THAT(IsolatedWebAppValidator::ValidateIntegrityBlock(
                  &browser_context_, test::GetDefaultEd25519WebBundleId(),
                  integrity_block,
                  /*dev_mode=*/false),
              UnusableSwbnErrorIs(Error::kIntegrityBlockValidationError,
                                  "public key(s) are not trusted"));
}

struct RelativeURL {
  web_package::SignedWebBundleId web_bundle_id;
  std::optional<std::string> relative_url;
};

struct FullURL {
  std::string full_url;
};

using WebBundleEntry = std::variant<RelativeURL, FullURL>;

class IsolatedWebAppValidatorMetadataTest
    : public IsolatedWebAppValidatorTest,
      public ::testing::WithParamInterface<
          std::tuple<std::optional<web_package::SignedWebBundleId>,
                     std::vector<WebBundleEntry>,
                     base::expected<void, UnusableSwbnFileError>>> {
 public:
  IsolatedWebAppValidatorMetadataTest()
      : primary_url_(std::get<0>(GetParam())),
        status_(std::get<2>(GetParam())) {
    for (const WebBundleEntry& entry : std::get<1>(GetParam())) {
      entries_.emplace_back(std::visit(
          absl::Overload{
              [](const FullURL& entry) { return entry.full_url; },
              [](const RelativeURL& entry) {
                auto base_url =
                    IwaOrigin(entry.web_bundle_id).origin().GetURL();
                if (entry.relative_url) {
                  return base_url.Resolve(*entry.relative_url).spec();
                }
                return base_url.spec();
              }},
          entry));
    }
  }

 protected:
  std::optional<web_package::SignedWebBundleId> primary_url_;
  std::vector<GURL> entries_;
  base::expected<void, UnusableSwbnFileError> status_;
};

TEST_P(IsolatedWebAppValidatorMetadataTest, Validate) {
  EXPECT_EQ(IsolatedWebAppValidator::ValidateMetadata(
                test::GetDefaultEd25519WebBundleId(),
                [&]() -> std::optional<GURL> {
                  if (!primary_url_) {
                    return std::nullopt;
                  }
                  return IwaOrigin(*primary_url_).origin().GetURL();
                }(),
                entries_),
            status_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppValidatorMetadataTest,
    ::testing::Values(
        std::make_tuple(
            std::nullopt,
            std::vector<WebBundleEntry>({RelativeURL{
                .web_bundle_id = test::GetDefaultEd25519WebBundleId()}}),
            base::ok()),
        std::make_tuple(
            std::nullopt,
            std::vector<WebBundleEntry>(
                {RelativeURL{
                     .web_bundle_id = test::GetDefaultEd25519WebBundleId()},
                 RelativeURL{
                     .web_bundle_id = test::GetDefaultEd25519WebBundleId(),
                     .relative_url = "/foo#bar"}}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "The URL of an exchange is invalid: URLs must not have "
                "a fragment part."))),
        std::make_tuple(
            std::nullopt,
            std::vector<WebBundleEntry>(
                {RelativeURL{
                     .web_bundle_id = test::GetDefaultEd25519WebBundleId()},
                 RelativeURL{
                     .web_bundle_id = test::GetDefaultEd25519WebBundleId(),
                     .relative_url = "/foo?bar"}}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "The URL of an exchange is invalid: URLs must not have "
                "a query part."))),
        std::make_tuple(
            test::GetDefaultEd25519WebBundleId(),
            std::vector<WebBundleEntry>({RelativeURL{
                .web_bundle_id = test::GetDefaultEd25519WebBundleId()}}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "Primary URL must not be present, but was isolated-app://"
                "4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic/"))),
        std::make_tuple(
            std::nullopt,
            std::vector<WebBundleEntry>(
                {RelativeURL{
                     .web_bundle_id = test::GetDefaultEd25519WebBundleId()},
                 FullURL{.full_url = "https://foo/"}}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "The URL of an exchange is invalid: The URL scheme must be "
                "isolated-app, but was https"))),
        std::make_tuple(
            std::nullopt,
            std::vector<WebBundleEntry>(
                {RelativeURL{
                     .web_bundle_id = test::GetDefaultEd25519WebBundleId()},
                 RelativeURL{
                     .web_bundle_id = test::GetDefaultEcdsaP256WebBundleId()}}),
            base::unexpected(UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kMetadataValidationError,
                "The URL of an exchange contains the wrong Signed Web "
                "Bundle "
                "ID: "
                "amfcf7c4bmpbjbmq4h4yptcobves56hfdyr7tm3doxqvfmsk5ss6maaca"
                "i")))));

}  // namespace web_app
