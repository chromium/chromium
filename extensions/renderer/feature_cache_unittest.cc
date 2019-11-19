// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/feature_cache.h"

#include "base/stl_util.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/scoped_web_frame.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8.h"

#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

namespace {

struct FakeContext {
  Feature::Context context_type;
  const Extension* extension;
  const GURL url;
};

bool HasFeature(FeatureCache& cache,
                const FakeContext& context,
                const std::string& feature) {
  return base::Contains(
      cache.GetAvailableFeatures(context.context_type, context.extension,
                                 context.url),
      feature);
}

}  // namespace

using FeatureCacheTest = testing::Test;

TEST_F(FeatureCacheTest, Basic) {
  FeatureCache cache;
  scoped_refptr<const Extension> extension_a = ExtensionBuilder("a").Build();
  scoped_refptr<const Extension> extension_b =
      ExtensionBuilder("b").AddPermission("storage").Build();

  FakeContext context_a = {Feature::BLESSED_EXTENSION_CONTEXT,
                           extension_a.get(), extension_a->url()};
  FakeContext context_b = {Feature::BLESSED_EXTENSION_CONTEXT,
                           extension_b.get(), extension_b->url()};
  // To start, context a should not have access to storage, but context b
  // should.
  EXPECT_FALSE(HasFeature(cache, context_a, "storage"));
  EXPECT_TRUE(HasFeature(cache, context_b, "storage"));

  // Update extension b's permissions and invalidate the cache.
  extension_b->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(), std::make_unique<PermissionSet>());
  cache.InvalidateExtension(extension_b->id());

  // Now, neither context should have storage access.
  EXPECT_FALSE(HasFeature(cache, context_a, "storage"));
  EXPECT_FALSE(HasFeature(cache, context_b, "storage"));
}

TEST_F(FeatureCacheTest, WebUIContexts) {
  FeatureCache cache;
  scoped_refptr<const Extension> extension_a = ExtensionBuilder("a").Build();

  // The chrome://extensions page is whitelisted for the management API.
  FakeContext webui_context = {Feature::WEBUI_CONTEXT, nullptr,
                               content::GetWebUIURL("extensions")};
  // chrome://baz is not whitelisted, and should not have access.
  FakeContext webui_context_without_access = {Feature::WEBUI_CONTEXT, nullptr,
                                              content::GetWebUIURL("baz")};

  EXPECT_TRUE(HasFeature(cache, webui_context, "management"));
  EXPECT_FALSE(HasFeature(cache, webui_context_without_access, "management"));
  // No webui context is whitelisted for, e.g., the idle API, so neither should
  // have access.
  EXPECT_FALSE(HasFeature(cache, webui_context, "idle"));
  EXPECT_FALSE(HasFeature(cache, webui_context_without_access, "idle"));
}

}  // namespace extensions
