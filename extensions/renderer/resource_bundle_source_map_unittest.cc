// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/resource_bundle_source_map.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "extensions/grit/extensions_renderer_generated_resources.h"
#include "gin/converter.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

class ResourceBundleSourceMapV8Test : public gin::V8Test {
 public:
  void SetUp() override {
    gin::V8Test::SetUp();
    source_map_ = std::make_unique<ResourceBundleSourceMap>(
        &ui::ResourceBundle::GetSharedInstance());
  }

  void TearDown() override {
    gin::V8Test::TearDown();
    source_map_.reset();
  }

 protected:
  v8::Isolate* isolate() { return instance_->isolate(); }
  ResourceBundleSourceMap& source_map() { return *source_map_; }

 private:
  std::unique_ptr<ResourceBundleSourceMap> source_map_;
};

TEST(ResourceBundleSourceMapTest, FindsSourcesAcrossStaticTables) {
  static constexpr auto kFirstSources =
      base::MakeFixedFlatMap<std::string_view, int>(
          {{"alpha", 1}, {"beta", 2}});
  static constexpr auto kSecondSources =
      base::MakeFixedFlatMap<std::string_view, int>(
          {{"delta", 4}, {"gamma", 3}});

  ResourceBundleSourceMap source_map(nullptr);
  source_map.RegisterSources(kFirstSources);
  source_map.RegisterSources(kSecondSources);

  EXPECT_TRUE(source_map.Contains("alpha"));
  EXPECT_TRUE(source_map.Contains("gamma"));

  const std::string name_with_suffix = "delta suffix";
  EXPECT_TRUE(
      source_map.Contains(std::string_view(name_with_suffix)
                              .substr(0, std::string_view("delta").size())));
  EXPECT_FALSE(source_map.Contains("missing"));
}

// Lookup walks the tables in registration order, so a name that appears in
// more than one table resolves to the first table that registered it.
TEST(ResourceBundleSourceMapTest, FirstRegisteredTableWinsForDuplicateNames) {
  static constexpr auto kFirstSources =
      base::MakeFixedFlatMap<std::string_view, int>({{"alpha", 1}});
  static constexpr auto kSecondSources =
      base::MakeFixedFlatMap<std::string_view, int>({{"alpha", 99}});

  ResourceBundleSourceMap source_map(nullptr);
  source_map.RegisterSources(kFirstSources);
  source_map.RegisterSources(kSecondSources);

  base::AutoLock lock(source_map.lock_);
  EXPECT_EQ(1, source_map.FindResourceId("alpha"));
}

// A provider whose entries are all disabled by build flags registers an empty
// table; there is nothing to search, so it is not retained.
TEST(ResourceBundleSourceMapTest, IgnoresEmptyTables) {
  static constexpr base::fixed_flat_map<std::string_view, int, 0> kEmptySources;
  static constexpr auto kSources =
      base::MakeFixedFlatMap<std::string_view, int>({{"alpha", 1}});

  ResourceBundleSourceMap source_map(nullptr);
  source_map.RegisterSources(kEmptySources);
  source_map.RegisterSources(kSources);

  EXPECT_TRUE(source_map.Contains("alpha"));
  EXPECT_FALSE(source_map.Contains("missing"));

  base::AutoLock lock(source_map.lock_);
  EXPECT_EQ(1u, source_map.source_tables_.size());
}

TEST_F(ResourceBundleSourceMapV8Test, LoadsAndCachesGzippedSource) {
  constexpr int kResourceId = IDR_EXTENSIONS_RENDERER_GENERATED_UTILS_JS;
  static constexpr auto kSources =
      base::MakeFixedFlatMap<std::string_view, int>({{"utils", kResourceId}});
  const ui::ResourceBundle& resource_bundle =
      ui::ResourceBundle::GetSharedInstance();
  ASSERT_TRUE(resource_bundle.IsGzipped(kResourceId));
  ResourceBundleSourceMap& source_map = this->source_map();
  source_map.RegisterSources(kSources);

  v8::HandleScope handle_scope(isolate());
  const std::string expected =
      resource_bundle.LoadDataResourceString(kResourceId);

  v8::Local<v8::String> source = source_map.GetSource(isolate(), "utils");
  ASSERT_FALSE(source.IsEmpty());
  EXPECT_EQ(expected, gin::V8ToString(isolate(), source));
  {
    base::AutoLock lock(source_map.lock_);
    EXPECT_EQ(1u, source_map.cached_sources_.size());
  }

  source = source_map.GetSource(isolate(), "utils");
  ASSERT_FALSE(source.IsEmpty());
  EXPECT_EQ(expected, gin::V8ToString(isolate(), source));
  {
    base::AutoLock lock(source_map.lock_);
    EXPECT_EQ(1u, source_map.cached_sources_.size());
  }
}

}  // namespace extensions
