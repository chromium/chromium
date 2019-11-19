// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "build/buildflag.h"
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

// Content client that exists only to register chrome-extension:// scheme with
// the url module.
// TODO(jamescook): Should this be merged with ShellContentClient? Should this
// be a persistent object available to tests?
class ExtensionsContentClient : public content::ContentClient {
 public:
  ExtensionsContentClient() {}
  ~ExtensionsContentClient() override {}

  // content::ContentClient overrides:
  void AddAdditionalSchemes(Schemes* schemes) override {
    schemes->standard_schemes.push_back(extensions::kExtensionScheme);
    schemes->savable_schemes.push_back(extensions::kExtensionScheme);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionsContentClient);
};

// The test suite for extensions_unittests.
class ExtensionsTestSuite : public content::ContentTestSuiteBase {
 public:
  ExtensionsTestSuite(int argc, char** argv);
  ~ExtensionsTestSuite() override;

 private:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

  std::unique_ptr<extensions::TestExtensionsClient> client_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsTestSuite);
};

ExtensionsTestSuite::ExtensionsTestSuite(int argc, char** argv)
    : content::ContentTestSuiteBase(argc, argv) {}

ExtensionsTestSuite::~ExtensionsTestSuite() {}

void ExtensionsTestSuite::Initialize() {
  content::ContentTestSuiteBase::Initialize();
  gl::GLSurfaceTestSupport::InitializeOneOff();

  // Register the chrome-extension:// scheme via this circuitous path. Note
  // that this does not persistently set up a ContentClient; individual tests
  // must use content::SetContentClient().
  {
    ExtensionsContentClient content_client;
    RegisterContentSchemes(&content_client);
  }
  RegisterInProcessThreads();

  extensions::RegisterPathProvider();

  base::FilePath extensions_shell_and_test_pak_path;
  base::PathService::Get(base::DIR_MODULE, &extensions_shell_and_test_pak_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      extensions_shell_and_test_pak_path.AppendASCII(
          "extensions_shell_and_test.pak"));

  client_.reset(new extensions::TestExtensionsClient());
  extensions::ExtensionsClient::Set(client_.get());
}

void ExtensionsTestSuite::Shutdown() {
  extensions::ExtensionsClient::Set(NULL);
  client_.reset();

  ui::ResourceBundle::CleanupSharedInstance();
  content::ContentTestSuiteBase::Shutdown();
}

}  // namespace

int main(int argc, char** argv) {
  content::UnitTestTestSuite test_suite(new ExtensionsTestSuite(argc, argv));
  return base::LaunchUnitTests(argc,
                               argv,
                               base::Bind(&content::UnitTestTestSuite::Run,
                                          base::Unretained(&test_suite)));
}
