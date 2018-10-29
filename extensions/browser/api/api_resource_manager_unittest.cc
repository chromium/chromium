// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/api_resource_manager.h"

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace extensions {

class ApiResourceManagerUnitTest : public ApiUnitTest {};

class FakeApiResource : public ApiResource {
 public:
  explicit FakeApiResource(const std::string& owner_extension_id)
      : ApiResource(owner_extension_id) {}
  ~FakeApiResource() override {}
  static const BrowserThread::ID kThreadId = BrowserThread::UI;
};

TEST_F(ApiResourceManagerUnitTest, TwoAppsCannotShareResources) {
  std::unique_ptr<ApiResourceManager<FakeApiResource>> manager(
      new ApiResourceManager<FakeApiResource>(browser_context()));
  scoped_refptr<const extensions::Extension> extension_one =
      ExtensionBuilder("one").Build();
  scoped_refptr<const extensions::Extension> extension_two =
      ExtensionBuilder("two").Build();

  const std::string extension_one_id(extension_one->id());
  const std::string extension_two_id(extension_two->id());

  int resource_one_id = manager->Add(new FakeApiResource(extension_one_id));
  int resource_two_id = manager->Add(new FakeApiResource(extension_two_id));
  CHECK(resource_one_id);
  CHECK(resource_two_id);

  // Confirm each extension can get its own resource.
  ASSERT_TRUE(manager->Get(extension_one_id, resource_one_id) != NULL);
  ASSERT_TRUE(manager->Get(extension_two_id, resource_two_id) != NULL);

  // Confirm neither extension can get the other's resource.
  ASSERT_TRUE(manager->Get(extension_one_id, resource_two_id) == NULL);
  ASSERT_TRUE(manager->Get(extension_two_id, resource_one_id) == NULL);

  // And make sure we're not susceptible to any Jedi mind tricks.
  ASSERT_TRUE(manager->Get(std::string(), resource_one_id) == NULL);
}

}  // namespace extensions
