// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/value_builder.h"

namespace utils = extensions::api_test_utils;

namespace extensions {

ApiUnitTest::ApiUnitTest() {}

ApiUnitTest::~ApiUnitTest() {}

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

void ApiUnitTest::CreateBackgroundPage() {
  if (!contents_) {
    GURL url = BackgroundInfo::GetBackgroundURL(extension());
    if (url.is_empty())
      url = GURL(url::kAboutBlankURL);
    contents_ = content::WebContents::Create(content::WebContents::CreateParams(
        browser_context(),
        content::SiteInstance::CreateForURL(browser_context(), url)));
  }
}

std::unique_ptr<base::Value> ApiUnitTest::RunFunctionAndReturnValue(
    ExtensionFunction* function,
    const std::string& args) {
  function->set_extension(extension());
  if (contents_)
    function->SetRenderFrameHost(contents_->GetMainFrame());
  return std::unique_ptr<base::Value>(utils::RunFunctionAndReturnSingleResult(
      function, args, browser_context()));
}

std::unique_ptr<base::DictionaryValue>
ApiUnitTest::RunFunctionAndReturnDictionary(ExtensionFunction* function,
                                            const std::string& args) {
  base::Value* value = RunFunctionAndReturnValue(function, args).release();
  base::DictionaryValue* dict = NULL;

  if (value && !value->GetAsDictionary(&dict))
    delete value;

  // We expect to either have successfuly retrieved a dictionary from the value,
  // or the value to have been NULL.
  EXPECT_TRUE(dict || !value);
  return std::unique_ptr<base::DictionaryValue>(dict);
}

std::unique_ptr<base::ListValue> ApiUnitTest::RunFunctionAndReturnList(
    ExtensionFunction* function,
    const std::string& args) {
  base::Value* value = RunFunctionAndReturnValue(function, args).release();
  base::ListValue* list = NULL;

  if (value && !value->GetAsList(&list))
    delete value;

  // We expect to either have successfuly retrieved a list from the value,
  // or the value to have been NULL.
  EXPECT_TRUE(list || !value);
  return std::unique_ptr<base::ListValue>(list);
}

std::string ApiUnitTest::RunFunctionAndReturnError(ExtensionFunction* function,
                                                   const std::string& args) {
  function->set_extension(extension());
  if (contents_)
    function->SetRenderFrameHost(contents_->GetMainFrame());
  return utils::RunFunctionAndReturnError(function, args, browser_context());
}

void ApiUnitTest::RunFunction(ExtensionFunction* function,
                              const std::string& args) {
  RunFunctionAndReturnValue(function, args);
}

}  // namespace extensions
