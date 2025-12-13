// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/reading/signed_web_bundle_reader.h"

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/expected.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "components/web_package/test_support/mock_web_bundle_parser_factory.h"
#include "components/web_package/test_support/signed_web_bundles/signature_verifier_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/webapps/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/webapps/isolated_web_apps/identity/iwa_identity_validator.h"
#include "components/webapps/isolated_web_apps/test_support/signed_web_bundle_utils.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/test_support/test_iwa_client.h"
#include "components/webapps/isolated_web_apps/test_support/test_signed_web_bundle_builder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using Error = SignedWebBundleReader::ReadResponseError;
using IntegritySignatureErrorForTesting =
    web_package::test::WebBundleSigner::IntegritySignatureErrorForTesting;
using IntegrityBlockErrorForTesting =
    web_package::test::WebBundleSigner::IntegrityBlockErrorForTesting;

using base::test::ErrorIs;
using base::test::HasValue;
using base::test::RunOnceCallback;
using base::test::ValueIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Message;
using ::testing::Ne;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

constexpr std::array<uint8_t, 64> kEd25519Signature = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0};

}  // namespace

class SignedWebBundleReaderWithRealBundlesTest : public testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    ON_CALL(iwa_client_,
            ValidateTrust(_, test::GetDefaultEd25519WebBundleId(), _))
        .WillByDefault(testing::Return(base::ok()));
    IwaIdentityValidator::CreateSingleton();
  }

  void TearDown() override {
    // Allow cleanup tasks posted by the destructor of `web_package::SharedFile`
    // to run.
    task_environment_.RunUntilIdle();
  }

  SignedWebBundleReader::Result CreateReaderAndInitialize(
      const TestSignedWebBundleBuilder::BuildOptions& build_options,
      const std::string test_file_data = kHtmlString) {
    base::FilePath swbn_file_path =
        temp_dir_.GetPath().Append(base::FilePath::FromASCII("bundle.swbn"));
    TestSignedWebBundle bundle =
        TestSignedWebBundleBuilder::BuildDefault(build_options);
    EXPECT_THAT(base::WriteFile(swbn_file_path, bundle.data), IsTrue());

    const GURL base_url = build_options.base_url_.has_value()
                              ? build_options.base_url_.value()
                              : kUrl;

    base::test::TestFuture<SignedWebBundleReader::Result> future;
    SignedWebBundleReader::Create(swbn_file_path, base_url,
                                  /*verify_signatures=*/true,
                                  future.GetCallback());

    auto result = future.Take();
    if (result.has_value()) {
      const auto& reader = *result;
      const auto& integrity_block = reader->GetIntegrityBlock();
      EXPECT_THAT(integrity_block.signature_stack().size(), Eq(1ul));

      auto* ed25519_signature_info =
          std::get_if<web_package::SignedWebBundleSignatureInfoEd25519>(
              &integrity_block.signature_stack().entries()[0].signature_info());
      EXPECT_TRUE(ed25519_signature_info);
      EXPECT_EQ(ed25519_signature_info->public_key(),
                test::GetDefaultEd25519KeyPair().public_key);
    }
    return result;
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;
  const GURL kUrl = GURL("https://example.com");
  constexpr static char kHtmlString[] = "test";
  testing::StrictMock<web_package::test::MockSignatureVerifier>
      signature_verifier_;
  base::AutoReset<web_package::SignedWebBundleSignatureVerifier*>
      reset_signature_verifier_ =
          web_app::SignedWebBundleReader::SetSignatureVerifierForTesting(
              &signature_verifier_);
  testing::NiceMock<test::MockIwaClient> iwa_client_;
};

// Note that Isolated Web Apps (IWAs) don't support having primary URLs, but the
// reader does, as it can be used for any Signed Web Bundle, even those not
// compatible with IWAs. Also, when baseURL is empty, relative URLs are used.
TEST_F(SignedWebBundleReaderWithRealBundlesTest,
       ReadValidWebBundleWithPrimaryUrlAndRelativeUrls) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));

  ASSERT_OK_AND_ASSIGN(
      auto reader,
      CreateReaderAndInitialize(
          TestSignedWebBundleBuilder::BuildOptions().SetPrimaryUrl(kUrl)));

  EXPECT_THAT(
      *reader,
      AllOf(Property(&SignedWebBundleReader::IsClosed, IsFalse()),
            Property(&SignedWebBundleReader::GetPrimaryURL, Ne(std::nullopt)),
            Property(&SignedWebBundleReader::GetEntries,
                     UnorderedElementsAre(
                         kUrl.Resolve(TestSignedWebBundleBuilder::kTestIconUrl),
                         kUrl.Resolve(
                             TestSignedWebBundleBuilder::kTestManifestUrl)))));
}

TEST_F(SignedWebBundleReaderWithRealBundlesTest, ReadValidResponse) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));

  ASSERT_OK_AND_ASSIGN(
      auto reader,
      CreateReaderAndInitialize(TestSignedWebBundleBuilder::BuildOptions()
                                    .SetBaseUrl(kUrl)
                                    .SetIndexHTMLContent(kHtmlString)));

  EXPECT_THAT(
      *reader,
      AllOf(Property(&SignedWebBundleReader::IsClosed, IsFalse()),
            Property(&SignedWebBundleReader::GetPrimaryURL, Eq(std::nullopt)),
            Property(&SignedWebBundleReader::GetEntries,
                     UnorderedElementsAre(
                         kUrl.Resolve(TestSignedWebBundleBuilder::kTestHtmlUrl),
                         kUrl.Resolve(TestSignedWebBundleBuilder::kTestIconUrl),
                         kUrl.Resolve(
                             TestSignedWebBundleBuilder::kTestManifestUrl)))));

  network::ResourceRequest resource_request;
  resource_request.url = kUrl.Resolve(TestSignedWebBundleBuilder::kTestHtmlUrl);

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr, Error>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  ASSERT_OK_AND_ASSIGN(auto response, response_result.Take());
  EXPECT_EQ(response->payload_length, std::string(kHtmlString).length());
  EXPECT_EQ(response->response_code, 200);
}

TEST_F(SignedWebBundleReaderWithRealBundlesTest,
       ReadIntegrityBlockWithInvalidVersion) {
  auto parse_status = CreateReaderAndInitialize(
      TestSignedWebBundleBuilder::BuildOptions()
          .SetBaseUrl(kUrl)
          .SetIndexHTMLContent(kHtmlString)
          .SetErrorsForTesting(
              {{IntegrityBlockErrorForTesting::kInvalidVersion},
               /*signatures_errors=*/{}}));

  EXPECT_THAT(
      parse_status,
      ErrorIs(Property(
          &UnusableSwbnFileError::value,
          UnusableSwbnFileError::Error::kIntegrityBlockParserVersionError)));
}

TEST_F(SignedWebBundleReaderWithRealBundlesTest,
       ReadIntegrityBlockWithInvalidStructure) {
  auto parse_status = CreateReaderAndInitialize(
      TestSignedWebBundleBuilder::BuildOptions()
          .SetBaseUrl(kUrl)
          .SetErrorsForTesting(
              {{IntegrityBlockErrorForTesting::kInvalidIntegrityBlockStructure},
               /*signatures_errors=*/{}}));

  EXPECT_THAT(
      parse_status,
      ErrorIs(Property(
          &UnusableSwbnFileError::value,
          UnusableSwbnFileError::Error::kIntegrityBlockParserFormatError)));
}

TEST_F(SignedWebBundleReaderWithRealBundlesTest, Close) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));

  ASSERT_OK_AND_ASSIGN(
      auto reader,
      CreateReaderAndInitialize(TestSignedWebBundleBuilder::BuildOptions()
                                    .SetBaseUrl(kUrl)
                                    .SetIndexHTMLContent(kHtmlString)));

  EXPECT_FALSE(reader->IsClosed());

  base::test::TestFuture<void> close_future;
  reader->Close(close_future.GetCallback());
  EXPECT_TRUE(close_future.Wait());
  EXPECT_TRUE(reader->IsClosed());
}

class SignedWebBundleReaderTest : public testing::Test {
 protected:
  void SetUp() override {
    parser_factory_ =
        std::make_unique<web_package::MockWebBundleParserFactory>();

    response_ = web_package::mojom::BundleResponse::New();
    response_->response_code = 200;
    response_->payload_offset = 0;
    response_->payload_length = sizeof(kResponseBody) - 1;

    base::flat_map<GURL, web_package::mojom::BundleResponseLocationPtr> items;
    items.insert(
        {kUrl, web_package::mojom::BundleResponseLocation::New(
                   response_->payload_offset, response_->payload_length)});
    metadata_ = web_package::mojom::BundleMetadata::New();
    metadata_->primary_url = kUrl;
    metadata_->requests = std::move(items);

    auto signature_info_ed25519 =
        web_package::mojom::SignatureInfoEd25519::New();
    signature_info_ed25519->public_key =
        test::GetDefaultEd25519KeyPair().public_key;
    signature_info_ed25519->signature =
        web_package::Ed25519Signature::Create(base::span(kEd25519Signature));

    auto signed_web_bundle_id =
        web_package::SignedWebBundleId::CreateForPublicKey(
            signature_info_ed25519->public_key);

    web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr
        signature_stack_entry =
            web_package::mojom::BundleIntegrityBlockSignatureStackEntry::New();
    signature_stack_entry->signature_info =
        web_package::mojom::SignatureInfo::NewEd25519(
            std::move(signature_info_ed25519));

    std::vector<web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr>
        signature_stack;
    signature_stack.push_back(std::move(signature_stack_entry));

    integrity_block_ = web_package::mojom::BundleIntegrityBlock::New();
    integrity_block_->size = 123;
    integrity_block_->signature_stack = std::move(signature_stack);
    integrity_block_->attributes =
        web_package::test::GetAttributesForSignedWebBundleId(
            signed_web_bundle_id.id());
  }

  void TearDown() override {
    // Allow cleanup tasks posted by the destructor of `web_package::SharedFile`
    // to run.
    task_environment_.RunUntilIdle();
  }

  base::test::TestFuture<SignedWebBundleReader::Result>
  CreateReaderAndInitialize(bool verify_signatures = true,
                            const std::optional<GURL>& base_url = std::nullopt,
                            const std::string test_file_data = kResponseBody) {
    // Provide a buffer that contains the contents of just a single
    // response. We do not need to provide an integrity block or metadata
    // here, since reading them is completely mocked. Only response bodies
    // are read from an actual (temporary) file.
    base::FilePath temp_file_path;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file_path));
    EXPECT_TRUE(base::WriteFile(temp_file_path, test_file_data));

    in_process_data_decoder_.SetWebBundleParserFactoryBinder(
        base::BindRepeating(
            &web_package::MockWebBundleParserFactory::AddReceiver,
            base::Unretained(parser_factory_.get())));

    base::test::TestFuture<SignedWebBundleReader::Result> future;
    SignedWebBundleReader::Create(temp_file_path, base_url, verify_signatures,
                                  future.GetCallback());
    return future;
  }

  base::expected<web_package::mojom::BundleResponsePtr, Error>
  ReadAndFulfillResponse(
      SignedWebBundleReader& reader,
      const network::ResourceRequest& resource_request,
      web_package::mojom::BundleResponseLocationPtr expected_read_response_args,
      web_package::mojom::BundleResponsePtr response,
      web_package::mojom::BundleResponseParseErrorPtr error = nullptr) {
    base::test::TestFuture<
        base::expected<web_package::mojom::BundleResponsePtr, Error>>
        response_result;
    reader.ReadResponse(resource_request, response_result.GetCallback());

    parser_factory_->RunResponseCallback(std::move(expected_read_response_args),
                                         std::move(response), std::move(error));

    return response_result.Take();
  }

  void SimulateAndWaitForParserDisconnect() {
    base::RunLoop run_loop;
    parser_factory_->SimulateParserDisconnect();
    run_loop.RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<web_package::MockWebBundleParserFactory> parser_factory_;
  web_package::mojom::BundleIntegrityBlockPtr integrity_block_;

  const GURL kUrl = GURL("https://example.com");
  web_package::mojom::BundleMetadataPtr metadata_;

  constexpr static char kResponseBody[] = "test";
  web_package::mojom::BundleResponsePtr response_;
  testing::StrictMock<web_package::test::MockSignatureVerifier>
      signature_verifier_;
  base::AutoReset<web_package::SignedWebBundleSignatureVerifier*>
      reset_signature_verifier_ =
          web_app::SignedWebBundleReader::SetSignatureVerifierForTesting(
              &signature_verifier_);
};

TEST(SignedWebBundleReaderFileFalureTest, CantOpenFile) {
  base::test::TaskEnvironment env;
  base::FilePath file_path = base::FilePath::FromASCII("does-not-exist.swbn");

  base::test::TestFuture<SignedWebBundleReader::Result> future;
  SignedWebBundleReader::Create(file_path, /*base_url=*/std::nullopt,
                                /*verify_signatures=*/true,
                                future.GetCallback());

  EXPECT_THAT(
      future.Take(),
      ErrorIs(Property(
          &UnusableSwbnFileError::value,
          UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError)));
}

TEST_F(SignedWebBundleReaderTest, ReadValidIntegrityBlockAndMetadata) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  base::HistogramTester histogram_tester;

  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  EXPECT_EQ(reader->GetPrimaryURL(), kUrl);
  EXPECT_EQ(reader->GetEntries().size(), 1ul);
  EXPECT_EQ(reader->GetEntries()[0], kUrl);

  histogram_tester.ExpectTotalCount(
      "WebApp.Isolated.SignatureVerificationDuration", 1);
  histogram_tester.ExpectTotalCount(
      "WebApp.Isolated.SignatureVerificationFileLength", 1);
}

TEST_F(SignedWebBundleReaderTest, ReadIntegrityBlockError) {
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(
      nullptr, web_package::mojom::BundleIntegrityBlockParseError::New());

  auto parse_status = future.Take();

  EXPECT_THAT(
      parse_status,
      ErrorIs(Property(
          &UnusableSwbnFileError::value,
          UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError)));
}

TEST_F(SignedWebBundleReaderTest, ReadIntegrityBlockWithParserCrash) {
  parser_factory_->SimulateParseIntegrityBlockCrash();

  auto future = CreateReaderAndInitialize();

  auto parse_status = future.Take();

  EXPECT_THAT(
      parse_status,
      ErrorIs(Property(
          &UnusableSwbnFileError::value,
          UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError)));
}

class SignedWebBundleReaderSignatureVerificationErrorTest
    : public SignedWebBundleReaderTest,
      public ::testing::WithParamInterface<
          web_package::SignedWebBundleSignatureVerifier::Error> {};

TEST_P(SignedWebBundleReaderSignatureVerificationErrorTest,
       SignatureVerificationError) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::unexpected(GetParam())));
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());

  auto parse_status = future.Take();

  EXPECT_THAT(parse_status, ErrorIs(UnusableSwbnFileError(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignedWebBundleReaderSignatureVerificationErrorTest,
    ::testing::Values(
        web_package::SignedWebBundleSignatureVerifier::Error::ForInternalError(
            "internal error"),
        web_package::SignedWebBundleSignatureVerifier::Error::
            ForInvalidSignature("invalid signature")));

#if BUILDFLAG(IS_CHROMEOS)

// Test that signatures are not verified when the
// `integrity_block_callback` asks to skip signature verification and
// thus the provided `web_package::SignedWebBundleSignatureVerifier::Error` is
// never triggered.
TEST_F(SignedWebBundleReaderTest,
       ReadIntegrityBlockAndSkipSignatureVerification) {
  auto future = CreateReaderAndInitialize(/*verify_signatures=*/false);

  parser_factory_->RunIntegrityBlockCallback(integrity_block_.Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = future.Take();
  EXPECT_THAT(parse_status, HasValue());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(SignedWebBundleReaderTest, ReadMetadataError) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(
      integrity_block_->size, nullptr,
      web_package::mojom::BundleMetadataParseError::New());

  auto parse_status = future.Take();

  EXPECT_THAT(parse_status,
              ErrorIs(Property(
                  &UnusableSwbnFileError::value,
                  UnusableSwbnFileError::Error::kMetadataParserInternalError)));
}

TEST_F(SignedWebBundleReaderTest, ReadMetadataWithParserCrash) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  parser_factory_->SimulateParseMetadataCrash();

  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());

  auto parse_status = future.Take();

  EXPECT_THAT(parse_status,
              ErrorIs(Property(
                  &UnusableSwbnFileError::value,
                  UnusableSwbnFileError::Error::kMetadataParserInternalError)));
}

TEST_F(SignedWebBundleReaderTest, ReadResponse) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  ASSERT_OK_AND_ASSIGN(
      auto response, ReadAndFulfillResponse(*reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->payload_offset, response_->payload_offset);
  EXPECT_EQ(response->payload_length, response_->payload_length);
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithFragment) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetRefStr("baz");
  resource_request.url = kUrl.ReplaceComponents(replacements);

  ASSERT_OK_AND_ASSIGN(
      auto response, ReadAndFulfillResponse(*reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->payload_offset, response_->payload_offset);
  EXPECT_EQ(response->payload_length, response_->payload_length);
}

TEST_F(SignedWebBundleReaderTest, ReadNonExistingResponseWithPath) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetPathStr("/foo");
  resource_request.url = kUrl.ReplaceComponents(replacements);

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr, Error>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  EXPECT_THAT(
      response,
      ErrorIs(AllOf(Field(&Error::type, Error::Type::kResponseNotFound),
                    Field(&Error::message,
                          "The Web Bundle does not contain a response for the "
                          "provided URL: "
                          "https://example.com/foo"))));
}

TEST_F(SignedWebBundleReaderTest, ReadNonExistingResponseWithQuery) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetQueryStr("foo");
  resource_request.url = kUrl.ReplaceComponents(replacements);

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr, Error>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  EXPECT_THAT(
      response,
      ErrorIs(AllOf(Field(&Error::type, Error::Type::kResponseNotFound),
                    Field(&Error::message,
                          "The Web Bundle does not contain a response for the "
                          "provided URL: "
                          "https://example.com/?foo"))));
}

TEST_F(SignedWebBundleReaderTest, ReadResponseError) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  auto response = ReadAndFulfillResponse(
      *reader.get(), resource_request, metadata_->requests[kUrl]->Clone(),
      nullptr,
      web_package::mojom::BundleResponseParseError::New(
          web_package::mojom::BundleParseErrorType::kFormatError, "test"));
  EXPECT_THAT(response,
              ErrorIs(AllOf(Field(&Error::type, Error::Type::kFormatError),
                            Field(&Error::message, "test"))));
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithParserDisconnect) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  SimulateAndWaitForParserDisconnect();
  {
    ASSERT_OK_AND_ASSIGN(auto response, ReadAndFulfillResponse(
                                            *reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));
    EXPECT_EQ(response->response_code, 200);
    EXPECT_EQ(response->payload_offset, response_->payload_offset);
    EXPECT_EQ(response->payload_length, response_->payload_length);
  }

  EXPECT_EQ(parser_factory_->GetParserCreationCount(), 2);

  // Simulate another disconnect to verify that the reader can recover from
  // multiple disconnects over the course of its lifetime.
  SimulateAndWaitForParserDisconnect();
  {
    ASSERT_OK_AND_ASSIGN(auto response, ReadAndFulfillResponse(
                                            *reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));
    EXPECT_EQ(response->response_code, 200);
    EXPECT_EQ(response->payload_offset, response_->payload_offset);
    EXPECT_EQ(response->payload_length, response_->payload_length);
  }

  EXPECT_EQ(parser_factory_->GetParserCreationCount(), 3);
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithParserCrash) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  parser_factory_->SimulateParseResponseCrash();
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr, Error>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  EXPECT_THAT(response,
              ErrorIs(Field(&Error::type, Error::Type::kParserInternalError)));
}

TEST_F(SignedWebBundleReaderTest, ReadResponseBody) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  ASSERT_OK_AND_ASSIGN(
      auto response, ReadAndFulfillResponse(*reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));

  std::string response_body =
      ReadAndFulfillResponseBody(*reader.get(), std::move(response));
  EXPECT_EQ(response_body, kResponseBody);
}

TEST_F(SignedWebBundleReaderTest, CloseWhileReadingResponseBody) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  ASSERT_OK_AND_ASSIGN(
      auto response, ReadAndFulfillResponse(*reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));

  const uint64_t response_body_length = response->payload_length;
  auto read_response_body_callable =
      base::BindOnce(&SignedWebBundleReader::ReadResponseBody,
                     base::Unretained(reader.get()), std::move(response));

  base::test::TestFuture<net::Error> on_response_read_callback;
  mojo::ScopedDataPipeConsumerHandle response_body_consumer = ReadResponseBody(
      response_body_length, std::move(read_response_body_callable),
      on_response_read_callback.GetCallback());

  base::test::TestFuture<void> close_future;
  reader->Close(close_future.GetCallback());

  EXPECT_EQ(net::OK, on_response_read_callback.Get());
  std::string buffer(response_body_length, '\0');
  size_t actually_read_bytes = 0;
  MojoResult read_result = response_body_consumer->ReadData(
      MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
      actually_read_bytes);
  EXPECT_EQ(MOJO_RESULT_OK, read_result);
  EXPECT_EQ(buffer.size(), actually_read_bytes);
  EXPECT_EQ(buffer.substr(0, actually_read_bytes), kResponseBody);

  ASSERT_TRUE(close_future.Wait());
}

TEST_F(SignedWebBundleReaderTest, ResponseBodyEndDoesntFitInUint64) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  auto future = CreateReaderAndInitialize();

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  auto response = web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  // End of the response (which is offset + length) should not fit into
  // unit64_t.
  response->payload_offset = std::numeric_limits<uint64_t>::max() - 10;
  response->payload_length = 11;
  const uint64_t response_body_length = response->payload_length;

  auto read_response_body_callable =
      base::BindOnce(&SignedWebBundleReader::ReadResponseBody,
                     base::Unretained(reader.get()), std::move(response));

  base::test::TestFuture<net::Error> on_response_read_callback;
  mojo::ScopedDataPipeConsumerHandle response_body_consumer = ReadResponseBody(
      response_body_length, std::move(read_response_body_callable),
      on_response_read_callback.GetCallback());

  EXPECT_NE(net::OK, on_response_read_callback.Get());
}

class SignedWebBundleReaderBaseUrlTest
    : public SignedWebBundleReaderTest,
      public ::testing::WithParamInterface<std::optional<std::string>> {
 public:
  SignedWebBundleReaderBaseUrlTest() {
    if (GetParam().has_value()) {
      base_url_ = GURL(*GetParam());
    }
  }

 protected:
  std::optional<GURL> base_url_;
};

TEST_P(SignedWebBundleReaderBaseUrlTest, IsPassedThroughCorrectly) {
  EXPECT_CALL(signature_verifier_, VerifySignatures)
      .WillOnce(RunOnceCallback<2>(base::ok()));
  base::test::RepeatingTestFuture<std::optional<GURL>> on_create_parser_future;
  parser_factory_ = std::make_unique<web_package::MockWebBundleParserFactory>(
      on_create_parser_future.GetCallback());

  auto future =
      CreateReaderAndInitialize(/*verify_signatures=*/true, base_url_);

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  ASSERT_OK_AND_ASSIGN(auto reader, future.Take());
  EXPECT_FALSE(reader->IsClosed());

  EXPECT_EQ(on_create_parser_future.Take(), base_url_);
  EXPECT_TRUE(on_create_parser_future.IsEmpty());

  SimulateAndWaitForParserDisconnect();
  network::ResourceRequest resource_request;
  resource_request.url = kUrl;
  auto response = ReadAndFulfillResponse(*reader.get(), resource_request,
                                         metadata_->requests[kUrl]->Clone(),
                                         response_->Clone());

  EXPECT_EQ(on_create_parser_future.Take(), base_url_);
  EXPECT_TRUE(on_create_parser_future.IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SignedWebBundleReaderBaseUrlTest,
                         ::testing::Values(std::nullopt,
                                           "https://example.com"));

class UnsecureSignedWebBundleReaderTest : public testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    ON_CALL(iwa_client_,
            ValidateTrust(_, test::GetDefaultEd25519WebBundleId(), _))
        .WillByDefault(testing::Return(base::ok()));
  }

  void TearDown() override {
    // Allow cleanup tasks posted by the destructor of `web_package::SharedFile`
    // to run.
    task_environment_.RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;
  testing::NiceMock<test::MockIwaClient> iwa_client_;
};

TEST_F(UnsecureSignedWebBundleReaderTest, ReadValidId) {
  base::FilePath path =
      temp_dir_.GetPath().Append(base::FilePath::FromASCII("test-0.swbn"));
  TestSignedWebBundle bundle = TestSignedWebBundleBuilder::BuildDefault();

  ASSERT_THAT(base::WriteFile(path, bundle.data), IsTrue());

  base::test::TestFuture<
      base::expected<web_package::SignedWebBundleId, UnusableSwbnFileError>>
      read_web_bundle_id_future;

  UnsecureSignedWebBundleIdReader::GetWebBundleId(
      path, read_web_bundle_id_future.GetCallback());
  base::expected<web_package::SignedWebBundleId, UnusableSwbnFileError>
      bundle_id_result = read_web_bundle_id_future.Take();

  EXPECT_THAT(bundle_id_result, ValueIs(test::GetDefaultEd25519WebBundleId()));
}

TEST_F(UnsecureSignedWebBundleReaderTest, ErrorId) {
  for (auto error : {IntegritySignatureErrorForTesting::kInvalidSignatureLength,
                     IntegritySignatureErrorForTesting::kInvalidPublicKeyLength,
                     IntegritySignatureErrorForTesting::
                         kWrongSignatureStackEntryAttributeName,
                     IntegritySignatureErrorForTesting::
                         kNoPublicKeySignatureStackEntryAttribute,
                     IntegritySignatureErrorForTesting::
                         kAdditionalSignatureStackEntryElement}) {
    std::string swbn_file_name =
        base::NumberToString(base::to_underlying(error)) + "_test.swbn";
    SCOPED_TRACE(Message() << "Running testcase: "
                           << " " << swbn_file_name);

    TestSignedWebBundle bundle = TestSignedWebBundleBuilder::BuildDefault(
        TestSignedWebBundleBuilder::BuildOptions().SetErrorsForTesting(
            {/*integrity_block_errors=*/{}, {{error}}}));
    base::FilePath path =
        temp_dir_.GetPath().Append(base::FilePath::FromASCII(swbn_file_name));
    ASSERT_THAT(base::WriteFile(path, bundle.data), IsTrue());

    base::test::TestFuture<
        base::expected<web_package::SignedWebBundleId, UnusableSwbnFileError>>
        read_web_bundle_id_future;

    UnsecureSignedWebBundleIdReader::GetWebBundleId(
        path, read_web_bundle_id_future.GetCallback());
    base::expected<web_package::SignedWebBundleId, UnusableSwbnFileError>
        bundle_id_result = read_web_bundle_id_future.Take();

    EXPECT_THAT(bundle_id_result, ErrorIs(_));
    EXPECT_THAT(
        bundle_id_result,
        ErrorIs(Property(
            &UnusableSwbnFileError::value,
            UnusableSwbnFileError::Error::kIntegrityBlockParserFormatError)));
  }
}

}  // namespace web_app
