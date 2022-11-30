// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/updater/extension_installer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class ExtensionInstallerTest : public ExtensionsTest {
 public:
  using UpdateClientCallback =
      extensions::ExtensionInstaller::UpdateClientCallback;
  using ExtensionInstallerCallback =
      ExtensionInstaller::ExtensionInstallerCallback;
  using Result = update_client::CrxInstaller::Result;
  using InstallError = update_client::InstallError;

  ExtensionInstallerTest();

  ExtensionInstallerTest(const ExtensionInstallerTest&) = delete;
  ExtensionInstallerTest& operator=(const ExtensionInstallerTest&) = delete;

  ~ExtensionInstallerTest() override;

  void InstallCompleteCallback(const Result& result);

 protected:
  void RunThreads();

 protected:
  const std::string kExtensionId = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
  const std::string kPublicKey =
      "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC8c4fBSPZ6utYoZ8NiWF/"
      "DSaimBhihjwgOsskyleFGaurhi3TDClTVSGPxNkgCzrz0wACML7M4aNjpd05qupdbR2d294j"
      "kDuI7caxEGUucpP7GJRRHnm8Sx+"
      "y0ury28n8jbN0PnInKKWcxpIXXmNQyC19HBuO3QIeUq9Dqc+7YFQIDAQAB";

  base::RunLoop run_loop_;
  Result result_;
  bool executed_;
};

ExtensionInstallerTest::ExtensionInstallerTest()
    : result_(-1), executed_(false) {}

ExtensionInstallerTest::~ExtensionInstallerTest() = default;

void ExtensionInstallerTest::InstallCompleteCallback(const Result& result) {
  result_ = result;
  executed_ = true;
  run_loop_.Quit();
}

void ExtensionInstallerTest::RunThreads() {
  run_loop_.Run();
}

TEST_F(ExtensionInstallerTest, GetInstalledFile) {
  base::ScopedTempDir root_dir;
  ASSERT_TRUE(root_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::PathExists(root_dir.GetPath()));
  scoped_refptr<ExtensionInstaller> installer =
      base::MakeRefCounted<ExtensionInstaller>(kExtensionId, root_dir.GetPath(),
                                               false /*install_immediately*/,
                                               ExtensionInstallerCallback());

  base::FilePath installed_file;

#ifdef FILE_PATH_USES_DRIVE_LETTERS
  const std::string absolute_path = "C:\\abc\\def";
  const std::string relative_path = "abc\\..\\def\\ghi";
#else
  const std::string absolute_path = "/abc/def";
  const std::string relative_path = "/abc/../def/ghi";
#endif

  installed_file.clear();
  EXPECT_FALSE(installer->GetInstalledFile(absolute_path, &installed_file));
  installed_file.clear();
  EXPECT_FALSE(installer->GetInstalledFile(relative_path, &installed_file));
  installed_file.clear();
  EXPECT_FALSE(installer->GetInstalledFile("extension", &installed_file));

  installed_file.clear();
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(root_dir.GetPath(), &temp_file));
  base::FilePath base_temp_file = temp_file.BaseName();
  EXPECT_TRUE(installer->GetInstalledFile(
      std::string(base_temp_file.value().begin(), base_temp_file.value().end()),
      &installed_file));
#ifndef FILE_PATH_USES_DRIVE_LETTERS
  // On some Win*, this test is flaky because of the way Win* constructs path.
  // For example,
  // "C:\Users\chrome-bot\AppData" is the same as "C:\Users\CHROME~1\AppData"
  EXPECT_EQ(temp_file, installed_file);
#endif
}

TEST_F(ExtensionInstallerTest, Install_InvalidUnpackedDir) {
  // The unpacked folder is not valid, the installer will return an error.
  base::ScopedTempDir root_dir;
  ASSERT_TRUE(root_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::PathExists(root_dir.GetPath()));
  scoped_refptr<ExtensionInstaller> installer =
      base::MakeRefCounted<ExtensionInstaller>(
          kExtensionId, root_dir.GetPath(), true /*install_immediately*/,
          base::BindRepeating(
              [](const std::string& extension_id, const std::string& public_key,
                 const base::FilePath& unpacked_dir, bool install_immediately,
                 UpdateClientCallback update_client_callback) {
                // This function should never be executed.
                EXPECT_TRUE(false);
              }));

  // Non-existing unpacked dir
  base::ScopedTempDir unpacked_dir;
  ASSERT_TRUE(unpacked_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::PathExists(unpacked_dir.GetPath()));
  ASSERT_TRUE(base::DeletePathRecursively(unpacked_dir.GetPath()));
  ASSERT_FALSE(base::PathExists(unpacked_dir.GetPath()));
  installer->Install(
      unpacked_dir.GetPath(), kPublicKey, nullptr, base::DoNothing(),
      base::BindOnce(&ExtensionInstallerTest::InstallCompleteCallback,
                     base::Unretained(this)));

  RunThreads();

  EXPECT_TRUE(executed_);
  EXPECT_EQ(static_cast<int>(InstallError::GENERIC_ERROR), result_.error);
}

TEST_F(ExtensionInstallerTest, Install_BasicInstallOperation_Error) {
  base::ScopedTempDir root_dir;
  ASSERT_TRUE(root_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::PathExists(root_dir.GetPath()));
  scoped_refptr<ExtensionInstaller> installer =
      base::MakeRefCounted<ExtensionInstaller>(
          kExtensionId, root_dir.GetPath(), false /*install_immediately*/,
          base::BindRepeating([](const std::string& extension_id,
                                 const std::string& public_key,
                                 const base::FilePath& unpacked_dir,
                                 bool install_immediately,
                                 UpdateClientCallback update_client_callback) {
            EXPECT_FALSE(install_immediately);
            std::move(update_client_callback)
                .Run(Result(InstallError::GENERIC_ERROR));
          }));

  base::ScopedTempDir unpacked_dir;
  ASSERT_TRUE(unpacked_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::PathExists(unpacked_dir.GetPath()));

  installer->Install(
      unpacked_dir.GetPath(), kPublicKey, nullptr, base::DoNothing(),
      base::BindOnce(&ExtensionInstallerTest::InstallCompleteCallback,
                     base::Unretained(this)));

  RunThreads();

  EXPECT_TRUE(executed_);
  EXPECT_EQ(static_cast<int>(InstallError::GENERIC_ERROR), result_.error);
}

TEST_F(ExtensionInstallerTest, Install_BasicInstallOperation_Success) {
  base::ScopedTempDir root_dir;
  ASSERT_TRUE(root_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::PathExists(root_dir.GetPath()));
  scoped_refptr<ExtensionInstaller> installer =
      base::MakeRefCounted<ExtensionInstaller>(
          kExtensionId, root_dir.GetPath(), true /*install_immediately*/,
          base::BindRepeating([](const std::string& extension_id,
                                 const std::string& public_key,
                                 const base::FilePath& unpacked_dir,
                                 bool install_immediately,
                                 UpdateClientCallback update_client_callback) {
            EXPECT_TRUE(install_immediately);
            std::move(update_client_callback).Run(Result(InstallError::NONE));
          }));

  base::ScopedTempDir unpacked_dir;
  ASSERT_TRUE(unpacked_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::PathExists(unpacked_dir.GetPath()));

  installer->Install(
      unpacked_dir.GetPath(), kPublicKey, nullptr, base::DoNothing(),
      base::BindOnce(&ExtensionInstallerTest::InstallCompleteCallback,
                     base::Unretained(this)));

  RunThreads();

  EXPECT_TRUE(executed_);
  EXPECT_EQ(static_cast<int>(InstallError::NONE), result_.error);
}

}  // namespace

}  // namespace extensions
