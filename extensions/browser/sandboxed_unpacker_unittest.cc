// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/sandboxed_unpacker.h"

#include <memory>
#include <tuple>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/features.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/crx_file/id_util.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/switches.h"
#include "extensions/common/verifier_formats.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "extensions/test/test_extensions_client.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// Inserts an illegal path into the browser images returned by
// TestExtensionsClient for any extension.
class IllegalImagePathInserter
    : public TestExtensionsClient::BrowserImagePathsFilter {
 public:
  IllegalImagePathInserter(TestExtensionsClient* client) : client_(client) {
    client_->AddBrowserImagePathsFilter(this);
  }

  virtual ~IllegalImagePathInserter() {
    client_->RemoveBrowserImagePathsFilter(this);
  }

  void Filter(const Extension* extension,
              std::set<base::FilePath>* paths) override {
    base::FilePath illegal_path =
        base::FilePath(base::FilePath::kParentDirectory)
            .AppendASCII(kTempExtensionName)
            .AppendASCII("product_logo_128.png");
    paths->insert(illegal_path);
  }

 private:
  raw_ptr<TestExtensionsClient> client_;
};

}  // namespace

class MockSandboxedUnpackerClient : public SandboxedUnpackerClient {
 public:
  explicit MockSandboxedUnpackerClient(
      scoped_refptr<base::SequencedTaskRunner> callback_runner)
      : callback_runner_(callback_runner) {}

  base::FilePath temp_dir() const { return temp_dir_; }
  std::u16string unpack_error_message() const {
    if (error_) {
      return error_->message();
    }
    return std::u16string();
  }
  CrxInstallErrorType unpack_error_type() const {
    if (error_) {
      return error_->type();
    }
    return CrxInstallErrorType::NONE;
  }
  int unpack_error_detail() const {
    if (error_) {
      return error_->type() == CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE
                 ? static_cast<int>(error_->sandbox_failure_detail())
                 : static_cast<int>(error_->detail());
    }
    return 0;
  }

  void set_deleted_tracker(bool* deleted_tracker) {
    deleted_tracker_ = deleted_tracker;
  }

  void set_should_compute_hashes(bool should_compute_hashes) {
    should_compute_hashes_ = should_compute_hashes;
  }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  ~MockSandboxedUnpackerClient() override {
    if (deleted_tracker_)
      *deleted_tracker_ = true;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  void ShouldComputeHashesForOffWebstoreExtension(
      scoped_refptr<const Extension> extension,
      base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(should_compute_hashes_);
  }

  void OnUnpackSuccess(const base::FilePath& temp_dir,
                       const base::FilePath& extension_root,
                       std::unique_ptr<base::Value::Dict> original_manifest,
                       const Extension* extension,
                       const SkBitmap& install_icon,
                       base::Value::Dict ruleset_install_prefs) override {
    temp_dir_ = temp_dir;
    callback_runner_->PostTask(FROM_HERE, std::move(quit_closure_));
  }

  void OnUnpackFailure(const CrxInstallError& error) override {
    error_ = error;
    callback_runner_->PostTask(FROM_HERE, std::move(quit_closure_));
  }

  scoped_refptr<base::SequencedTaskRunner> callback_runner_;
  std::optional<CrxInstallError> error_;
  base::OnceClosure quit_closure_;
  base::FilePath temp_dir_;
  raw_ptr<bool> deleted_tracker_ = nullptr;
  bool should_compute_hashes_ = false;
};

class SandboxedUnpackerTest : public ExtensionsTest {
 public:
  SandboxedUnpackerTest()
      : ExtensionsTest(content::BrowserTaskEnvironment::IO_MAINLOOP),
        unpacker_thread_("Unpacker Thread") {}

  void SetUp() override {
    ExtensionsTest::SetUp();

    unpacker_thread_.Start();
    unpacker_task_runner_ = unpacker_thread_.task_runner();

    ASSERT_TRUE(extensions_dir_.CreateUniqueTempDir());
    in_process_utility_thread_helper_ =
        std::make_unique<content::InProcessUtilityThreadHelper>();
    // It will delete itself.
    client_ = new MockSandboxedUnpackerClient(
        task_environment()->GetMainThreadTaskRunner());

    InitSandboxedUnpacker();

    // By default, we host an in-process UnzipperImpl to support any service
    // clients. Tests may explicitly override the launch callback to prevent
    // this.
    unzip::SetUnzipperLaunchOverrideForTesting(
        base::BindRepeating(&unzip::LaunchInProcessUnzipper));
  }

  void InitSandboxedUnpacker() {
    sandboxed_unpacker_ = new SandboxedUnpacker(
        mojom::ManifestLocation::kInternal, Extension::NO_FLAGS,
        extensions_dir_.GetPath(), unpacker_task_runner_, client_);
  }

  void TearDown() override {
    unzip::SetUnzipperLaunchOverrideForTesting(base::NullCallback());
    // Need to destruct SandboxedUnpacker before the message loop since
    // it posts a task to it.
    sandboxed_unpacker_ = nullptr;
    base::RunLoop().RunUntilIdle();
    ExtensionsTest::TearDown();
    in_process_utility_thread_helper_.reset();

    unpacker_thread_.Stop();
  }

  base::FilePath GetCrxFullPath(const std::string& crx_name) {
    base::FilePath full_path;
    EXPECT_TRUE(base::PathService::Get(extensions::DIR_TEST_DATA, &full_path));
    full_path = full_path.AppendASCII("unpacker").AppendASCII(crx_name);
    EXPECT_TRUE(base::PathExists(full_path)) << full_path.value();
    return full_path;
  }

  void SetupUnpacker(const std::string& crx_name,
                     const std::string& package_hash) {
    base::FilePath crx_path = GetCrxFullPath(crx_name);
    extensions::CRXFileInfo crx_info(crx_path, GetTestVerifierFormat());
    crx_info.expected_hash = package_hash;

    base::RunLoop run_loop;
    client_->SetQuitClosure(run_loop.QuitClosure());

    unpacker_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SandboxedUnpacker::StartWithCrx,
                                  sandboxed_unpacker_, crx_info));
    // Wait for unpack
    run_loop.Run();
  }

  void SetupUnpackerWithDirectory(const std::string& crx_name) {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath crx_path = GetCrxFullPath(crx_name);
    ASSERT_TRUE(zip::Unzip(crx_path, temp_dir.GetPath()));

    std::string fake_id = crx_file::id_util::GenerateId(crx_name);
    std::string fake_public_key = base::Base64Encode(std::string(2048, 'k'));

    base::RunLoop run_loop;
    client_->SetQuitClosure(run_loop.QuitClosure());

    unpacker_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SandboxedUnpacker::StartWithDirectory,
                                  sandboxed_unpacker_, fake_id, fake_public_key,
                                  temp_dir.Take()));

    // Wait for unpack
    run_loop.Run();
  }

  bool InstallSucceeded() const { return !client_->temp_dir().empty(); }

  base::FilePath GetInstallPath() const {
    return client_->temp_dir().AppendASCII(kTempExtensionName);
  }

  std::u16string GetInstallErrorMessage() const {
    return client_->unpack_error_message();
  }

  CrxInstallErrorType GetInstallErrorType() const {
    return client_->unpack_error_type();
  }

  int GetInstallErrorDetail() const { return client_->unpack_error_detail(); }

  void ExpectInstallErrorContains(const std::string& error) {
    std::string full_error = base::UTF16ToUTF8(client_->unpack_error_message());
    EXPECT_TRUE(base::Contains(full_error, error))
        << "Error message " << full_error << " does not contain " << error;
  }

  // Unpacks the package |package_name| and checks that |sandboxed_unpacker_|
  // gets deleted.
  void TestSandboxedUnpackerDeleted(const std::string& package_name,
                                    bool expect_success) {
    bool client_deleted = false;
    client_->set_deleted_tracker(&client_deleted);
    SetupUnpacker(package_name, "");
    EXPECT_EQ(GetInstallErrorMessage().empty(), expect_success);

    base::RunLoop run_loop;
    client_->SetQuitClosure(run_loop.QuitClosure());

    // Remove our reference to |sandboxed_unpacker_|, it should get deleted
    // since/ it's the last reference.
    sandboxed_unpacker_ = nullptr;

    // Wait for |client_| dtor.
    run_loop.Run();

    // The SandboxedUnpacker should have been deleted and deleted the client.
    EXPECT_TRUE(client_deleted);
  }

  void SetPublicKey(const std::string& key) {
    sandboxed_unpacker_->public_key_ = key;
  }

  void SetExtensionRoot(const base::FilePath& path) {
    sandboxed_unpacker_->extension_root_ = path;
  }

  std::optional<base::Value::Dict> RewriteManifestFile(
      const base::Value::Dict& manifest) {
    return sandboxed_unpacker_->RewriteManifestFile(manifest);
  }

  data_decoder::test::InProcessDataDecoder& in_process_data_decoder() {
    return in_process_data_decoder_;
  }

 protected:
  base::ScopedTempDir extensions_dir_;
  raw_ptr<MockSandboxedUnpackerClient, AcrossTasksDanglingUntriaged> client_;
  scoped_refptr<SandboxedUnpacker> sandboxed_unpacker_;
  std::unique_ptr<content::InProcessUtilityThreadHelper>
      in_process_utility_thread_helper_;

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

 private:
  // The thread where the sandboxed unpacker runs. This provides test coverage
  // in an environment similar to what we use in production.
  base::Thread unpacker_thread_;

  scoped_refptr<base::SequencedTaskRunner> unpacker_task_runner_;
};

TEST_F(SandboxedUnpackerTest, EmptyDefaultLocale) {
  SetupUnpacker("empty_default_locale.crx", "");
  ExpectInstallErrorContains(manifest_errors::kInvalidDefaultLocale);
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(
      static_cast<int>(SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED),
      GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, HasDefaultLocaleMissingLocalesFolder) {
  SetupUnpacker("has_default_missing_locales.crx", "");
  ExpectInstallErrorContains(manifest_errors::kLocalesTreeMissing);
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(
      static_cast<int>(SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED),
      GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, InvalidDefaultLocale) {
  SetupUnpacker("invalid_default_locale.crx", "");
  ExpectInstallErrorContains(manifest_errors::kInvalidDefaultLocale);
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(
      static_cast<int>(SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED),
      GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, MissingDefaultData) {
  SetupUnpacker("missing_default_data.crx", "");
  ExpectInstallErrorContains(manifest_errors::kLocalesNoDefaultMessages);
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(
      static_cast<int>(SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED),
      GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, MissingDefaultLocaleHasLocalesFolder) {
  SetupUnpacker("missing_default_has_locales.crx", "");
  ExpectInstallErrorContains(l10n_util::GetStringUTF8(
      IDS_EXTENSION_LOCALES_NO_DEFAULT_LOCALE_SPECIFIED));
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(
      static_cast<int>(SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED),
      GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, MissingMessagesFile) {
  SetupUnpacker("missing_messages_file.crx", "");
  EXPECT_TRUE(base::MatchPattern(
      GetInstallErrorMessage(),
      u"*" + std::u16string(manifest_errors::kLocalesMessagesFileMissing) +
          u"*_locales?en_US?messages.json'."))
      << GetInstallErrorMessage();
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(
      static_cast<int>(SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED),
      GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, NoLocaleData) {
  SetupUnpacker("no_locale_data.crx", "");
  ExpectInstallErrorContains(manifest_errors::kLocalesNoDefaultMessages);
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(
      static_cast<int>(SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED),
      GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, ImageDecodingError) {
  const char16_t kExpected[] = u"Could not decode image: ";
  SetupUnpacker("bad_image.crx", "");
  EXPECT_TRUE(base::StartsWith(GetInstallErrorMessage(), kExpected,
                               base::CompareCase::INSENSITIVE_ASCII))
      << "Expected prefix: \"" << kExpected << "\", actual error: \""
      << GetInstallErrorMessage() << "\"";
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(
      static_cast<int>(SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED),
      GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, BadPathError) {
  IllegalImagePathInserter inserter(
      static_cast<TestExtensionsClient*>(ExtensionsClient::Get()));
  SetupUnpacker("good_package.crx", "");
  // Install should have failed with an error.
  EXPECT_FALSE(InstallSucceeded());
  EXPECT_FALSE(GetInstallErrorMessage().empty());
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(static_cast<int>(
                SandboxedUnpackerFailureReason::INVALID_PATH_FOR_BROWSER_IMAGE),
            GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, NoCatalogsSuccess) {
  SetupUnpacker("no_l10n.crx", "");
  // Check that there is no _locales folder.
  base::FilePath install_path = GetInstallPath().Append(kLocaleFolder);
  EXPECT_FALSE(base::PathExists(install_path));
  EXPECT_EQ(CrxInstallErrorType::NONE, GetInstallErrorType());
}

TEST_F(SandboxedUnpackerTest, FromDirNoCatalogsSuccess) {
  SetupUnpackerWithDirectory("no_l10n.crx");
  // Check that there is no _locales folder.
  base::FilePath install_path = GetInstallPath().Append(kLocaleFolder);
  EXPECT_FALSE(base::PathExists(install_path));
  EXPECT_EQ(CrxInstallErrorType::NONE, GetInstallErrorType());
}

TEST_F(SandboxedUnpackerTest, WithCatalogsSuccess) {
  SetupUnpacker("good_l10n.crx", "");
  // Check that there is _locales folder.
  base::FilePath install_path = GetInstallPath().Append(kLocaleFolder);
  EXPECT_TRUE(base::PathExists(install_path));
  EXPECT_EQ(CrxInstallErrorType::NONE, GetInstallErrorType());
}

TEST_F(SandboxedUnpackerTest, FromDirWithCatalogsSuccess) {
  SetupUnpackerWithDirectory("good_l10n.crx");
  // Check that there is _locales folder.
  base::FilePath install_path = GetInstallPath().Append(kLocaleFolder);
  EXPECT_TRUE(base::PathExists(install_path));
  EXPECT_EQ(CrxInstallErrorType::NONE, GetInstallErrorType());
}

TEST_F(SandboxedUnpackerTest, FailHashCheck) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableCrxHashCheck);
  SetupUnpacker("good_l10n.crx", std::string(64, '0'));
  // Check that there is an error message.
  EXPECT_FALSE(GetInstallErrorMessage().empty());
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(static_cast<int>(
                SandboxedUnpackerFailureReason::CRX_HASH_VERIFICATION_FAILED),
            GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, TestRewriteManifestInjections) {
  constexpr char kTestKey[] = "test_key";
  constexpr char kTestVersion[] = "1.2.3";
  constexpr char kVersionStr[] = "version";
  SetPublicKey(kTestKey);
  SetExtensionRoot(extensions_dir_.GetPath());
  std::string fingerprint = "1.0123456789abcdef";
  base::WriteFile(extensions_dir_.GetPath().Append(
                      FILE_PATH_LITERAL("manifest.fingerprint")),
                  fingerprint);
  std::optional<base::Value::Dict> manifest(
      RewriteManifestFile(base::Value::Dict().Set(kVersionStr, kTestVersion)));
  auto* key = manifest->FindString("key");
  auto* version = manifest->FindString(kVersionStr);
  auto* differential_fingerprint =
      manifest->FindString("differential_fingerprint");
  ASSERT_NE(nullptr, key);
  ASSERT_NE(nullptr, version);
  ASSERT_NE(nullptr, differential_fingerprint);
  EXPECT_EQ(kTestKey, *key);
  EXPECT_EQ(kTestVersion, *version);
  EXPECT_EQ(fingerprint, *differential_fingerprint);
}

TEST_F(SandboxedUnpackerTest, InvalidMessagesFile) {
  SetupUnpackerWithDirectory("invalid_messages_file.crx");
  // Check that there is no _locales folder.
  base::FilePath install_path = GetInstallPath().Append(kLocaleFolder);
  EXPECT_FALSE(base::PathExists(install_path));
  if (base::JSONReader::UsingRust()) {
    EXPECT_TRUE(base::MatchPattern(GetInstallErrorMessage(),
                                   u"*_locales?en_US?messages.json': EOF while "
                                   u"parsing a string at line 4*"))
        << GetInstallErrorMessage();
  } else {
    EXPECT_TRUE(base::MatchPattern(
        GetInstallErrorMessage(),
        u"*_locales?en_US?messages.json': Line: 4, column: 1,*"))
        << GetInstallErrorMessage();
  }
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(static_cast<int>(
                SandboxedUnpackerFailureReason::COULD_NOT_LOCALIZE_EXTENSION),
            GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, PassHashCheck) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableCrxHashCheck);
  SetupUnpacker(
      "good_l10n.crx",
      "614AE3D608F4C2185E9173293AB3F93EE7C7C79C9A2C3CF71F633386A3296A6C");
  // Check that there is no error message.
  EXPECT_THAT(GetInstallErrorMessage(), testing::IsEmpty());
  EXPECT_EQ(CrxInstallErrorType::NONE, GetInstallErrorType());
}

TEST_F(SandboxedUnpackerTest, SkipHashCheck) {
  SetupUnpacker("good_l10n.crx", "badhash");
  // Check that there is no error message.
  EXPECT_THAT(GetInstallErrorMessage(), testing::IsEmpty());
  EXPECT_EQ(CrxInstallErrorType::NONE, GetInstallErrorType());
}

// The following tests simulate the utility services failling.
TEST_F(SandboxedUnpackerTest, UnzipperServiceFails) {
  // We override the Unzipper's launching behavior to drop the interface
  // receiver, effectively simulating a crashy service process.
  unzip::SetUnzipperLaunchOverrideForTesting(base::BindRepeating([]() -> auto {
    mojo::PendingRemote<unzip::mojom::Unzipper> remote;
    std::ignore = remote.InitWithNewPipeAndPassReceiver();
    return remote;
  }));

  InitSandboxedUnpacker();
  SetupUnpacker("good_package.crx", "");
  EXPECT_FALSE(InstallSucceeded());
  EXPECT_FALSE(GetInstallErrorMessage().empty());
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(static_cast<int>(SandboxedUnpackerFailureReason::UNZIP_FAILED),
            GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, JsonParserFails) {
  // Disable the Rust JSON parser, as it is in-process and cannot crash.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(base::features::kUseRustJsonParser, false);

  in_process_data_decoder().SimulateJsonParserCrash(true);
  InitSandboxedUnpacker();

  SetupUnpacker("good_package.crx", "");
  EXPECT_FALSE(InstallSucceeded());
  EXPECT_FALSE(GetInstallErrorMessage().empty());
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
}

TEST_F(SandboxedUnpackerTest, ImageDecoderFails) {
  in_process_data_decoder().SimulateImageDecoderCrash(true);
  InitSandboxedUnpacker();
  SetupUnpacker("good_package.crx", "");
  EXPECT_FALSE(InstallSucceeded());
  EXPECT_FALSE(GetInstallErrorMessage().empty());
  ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            GetInstallErrorType());
  EXPECT_EQ(
      static_cast<int>(SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED),
      GetInstallErrorDetail());
}

TEST_F(SandboxedUnpackerTest, NoComputeHashes) {
  client_->set_should_compute_hashes(false);
  SetupUnpacker("good_package.crx", "");
  EXPECT_TRUE(InstallSucceeded());
  EXPECT_TRUE(GetInstallErrorMessage().empty());
  EXPECT_FALSE(
      base::PathExists(file_util::GetComputedHashesPath(GetInstallPath())));
}

TEST_F(SandboxedUnpackerTest, ComputeHashes) {
  client_->set_should_compute_hashes(true);
  SetupUnpacker("good_package.crx", "");
  EXPECT_TRUE(InstallSucceeded());
  EXPECT_TRUE(GetInstallErrorMessage().empty());
  EXPECT_TRUE(
      base::PathExists(file_util::GetComputedHashesPath(GetInstallPath())));
}

// SandboxedUnpacker is ref counted and is reference by callbacks and
// InterfacePtrs. This tests that it gets deleted as expected (so that no extra
// refs are left).
TEST_F(SandboxedUnpackerTest, DeletedOnSuccess) {
  TestSandboxedUnpackerDeleted("good_l10n.crx", /*expect_success=*/true);
}

TEST_F(SandboxedUnpackerTest, DeletedOnFailure) {
  TestSandboxedUnpackerDeleted("bad_image.crx", /*expect_success=*/false);
}

}  // namespace extensions
