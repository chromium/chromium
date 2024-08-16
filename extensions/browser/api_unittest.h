// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_UNITTEST_H_
#define EXTENSIONS_BROWSER_API_UNITTEST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extensions_test.h"

namespace base {
class Value;
}

namespace content {
class WebContents;
}

class ExtensionFunction;

namespace extensions {

// Use this class to enable calling API functions in a unittest.
// By default, this class will create and load an empty unpacked |extension_|,
// which will be used in all API function calls. This extension can be
// overridden using set_extension().
// When calling RunFunction[AndReturn*], |args| should be in JSON format,
// wrapped in a list. See also RunFunction* in api_test_utils.h.
class ApiUnitTest : public ExtensionsTest {
 public:
  ApiUnitTest();
  ~ApiUnitTest() override;

  content::WebContents* contents() { return contents_.get(); }
  const Extension* extension() const { return extension_.get(); }
  scoped_refptr<const Extension> extension_ref() { return extension_; }
  void set_extension(scoped_refptr<const Extension> extension) {
    extension_ = extension;
  }

 protected:
  // SetUp creates and loads an empty, unpacked Extension.
  void SetUp() override;
  void TearDown() override;

  // Creates a page for |extension_|, and sets it for the WebContents to be used
  // in API calls. If |contents_| is already set, this does nothing.
  void CreateExtensionPage();

  // Various ways of running an API function. These methods take ownership of
  // |function|. |args| should be in JSON format, wrapped in a list.

  // Return the function result as a base::Value.
  std::optional<base::Value> RunFunctionAndReturnValue(
      ExtensionFunction* function,
      api_test_utils::ArgsType args);

  // Return an error thrown from the function, if one exists.
  // This will EXPECT-fail if any result is returned from the function.
  std::string RunFunctionAndReturnError(ExtensionFunction* function,
                                        api_test_utils::ArgsType args);

  // Run the function and ignore any result.
  void RunFunction(ExtensionFunction* function, api_test_utils::ArgsType args);

 private:
  sync_preferences::TestingPrefServiceSyncable testing_pref_service_;

  // The WebContents used to associate a RenderFrameHost with API function
  // calls, or null.
  std::unique_ptr<content::WebContents> contents_;

  // The Extension used when running API function calls.
  scoped_refptr<const Extension> extension_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_UNITTEST_H_
