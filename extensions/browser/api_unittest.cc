// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api_unittest.h"

#include "base/values.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"

namespace utils = extensions::api_test_utils;

namespace extensions {

ApiUnitTest::ApiUnitTest() = default;

ApiUnitTest::~ApiUnitTest() = default;

void ApiUnitTest::SetUp() {
  ExtensionsTest::SetUp();

  user_prefs::UserPrefs::Set(browser_context(), &testing_pref_service_);

  extension_ = ExtensionBuilder("Test").Build();
}

void ApiUnitTest::TearDown() {
  extension_ = nullptr;
  contents_.reset();
  ExtensionsTest::TearDown();
}

void ApiUnitTest::CreateExtensionPage() {
  if (!contents_) {
    contents_ = content::WebContents::Create(content::WebContents::CreateParams(
        browser_context(), content::SiteInstance::CreateForURL(
                               browser_context(), GURL(url::kAboutBlankURL))));
  }
}

std::optional<base::Value> ApiUnitTest::RunFunctionAndReturnValue(
    ExtensionFunction* function,
    api_test_utils::ArgsType args) {
  function->set_extension(extension());
  if (contents_) {
    function->SetRenderFrameHost(contents_->GetPrimaryMainFrame());
  }
  return utils::RunFunctionAndReturnSingleResult(function, std::move(args),
                                                 browser_context());
}

std::string ApiUnitTest::RunFunctionAndReturnError(
    ExtensionFunction* function,
    api_test_utils::ArgsType args) {
  function->set_extension(extension());
  if (contents_) {
    function->SetRenderFrameHost(contents_->GetPrimaryMainFrame());
  }
  return utils::RunFunctionAndReturnError(function, std::move(args),
                                          browser_context());
}

void ApiUnitTest::RunFunction(ExtensionFunction* function,
                              api_test_utils::ArgsType args) {
  RunFunctionAndReturnValue(function, std::move(args));
}

}  // namespace extensions
