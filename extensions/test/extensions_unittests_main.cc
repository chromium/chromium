// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base_paths.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "build/android_buildflags.h"
#include "build/buildflag.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/common/content_client.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/unittest_test_suite.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_paths.h"
#include "extensions/test/test_extensions_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "url/url_util.h"

namespace {

const char* const kNonWildcardDomainNonPortSchemes[] = {
    extensions::kExtensionScheme};

// Content client that exists only to register chrome-extension:// scheme with
// the url module.
// TODO(jamescook): Should this be merged with ShellContentClient? Should this
// be a persistent object available to tests?
class ExtensionsContentClient : public content::ContentClient {
 public:
  ExtensionsContentClient() = default;
  ExtensionsContentClient(const ExtensionsContentClient&) = delete;
  ExtensionsContentClient& operator=(const ExtensionsContentClient&) = delete;
  ~ExtensionsContentClient() override = default;

  // content::ContentClient overrides:
  void AddAdditionalSchemes(Schemes* schemes) override {
    schemes->standard_schemes.push_back(extensions::kExtensionScheme);
    schemes->savable_schemes.push_back(extensions::kExtensionScheme);
  }
};

// The test suite for extensions_unittests.
class ExtensionsTestSuite : public content::ContentTestSuiteBase {
 public:
  ExtensionsTestSuite(int argc, char** argv);
  ExtensionsTestSuite(const ExtensionsTestSuite&) = delete;
  ExtensionsTestSuite& operator=(const ExtensionsTestSuite&) = delete;
  ~ExtensionsTestSuite() override;

 private:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

  std::unique_ptr<extensions::TestExtensionsClient> client_;
};

ExtensionsTestSuite::ExtensionsTestSuite(int argc, char** argv)
    : content::ContentTestSuiteBase(argc, argv) {}

ExtensionsTestSuite::~ExtensionsTestSuite() = default;

void ExtensionsTestSuite::Initialize() {
  content::ContentTestSuiteBase::Initialize();
  gl::GLSurfaceTestSupport::InitializeOneOff();

  // Register the chrome-extension:// scheme via this circuitous path.
  {
    ExtensionsContentClient content_client;
    RegisterContentSchemes(&content_client);
    ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(
        kNonWildcardDomainNonPortSchemes,
        std::size(kNonWildcardDomainNonPortSchemes));
  }
  RegisterInProcessThreads();

  extensions::RegisterPathProvider();

  base::FilePath extensions_shell_and_test_pak_path;
  base::PathService::Get(base::DIR_ASSETS, &extensions_shell_and_test_pak_path);
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  // On Android all pak files are inside the paks folder.
  extensions_shell_and_test_pak_path =
      extensions_shell_and_test_pak_path.Append(FILE_PATH_LITERAL("paks"));
#endif
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      extensions_shell_and_test_pak_path.AppendASCII(
          "extensions_shell_and_test.pak"));

  client_ = std::make_unique<extensions::TestExtensionsClient>();
  extensions::ExtensionsClient::Set(client_.get());
}

void ExtensionsTestSuite::Shutdown() {
  extensions::ExtensionsClient::Set(nullptr);
  client_.reset();

  ui::ResourceBundle::CleanupSharedInstance();
  content::ContentTestSuiteBase::Shutdown();
}

}  // namespace

int main(int argc, char** argv) {
  content::UnitTestTestSuite test_suite(
      new ExtensionsTestSuite(argc, argv),
      base::BindRepeating(
          content::UnitTestTestSuite::CreateTestContentClients));
  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&content::UnitTestTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
