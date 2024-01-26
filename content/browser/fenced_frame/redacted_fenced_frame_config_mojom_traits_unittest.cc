// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config_mojom_traits.h"

#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/test/gtest_util.h"
#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame_config.mojom.h"
#include "url/origin.h"

namespace content {

using RedactedFencedFrameConfig = blink::FencedFrame::RedactedFencedFrameConfig;
using RedactedFencedFrameProperties =
    blink::FencedFrame::RedactedFencedFrameProperties;
using Entity = content::FencedFrameEntity;

// This macro creates the following test pattern:
// * Redact a config.
// * Check that the desired property was redacted as expected.
// * Serialize and deserialize the redacted config into a copy (using mojom type
//   mappings implicitly).
// * Check that the desired property was copied correctly.
//
// Template Arguments:
// `ClassName`: `FencedFrameConfig` or `FencedFrameProperties`
//              Which base class to use for this test.
// `RedactedClassName`: `RedactedFencedFrameConfig` or
//                       `RedactedFencedFrameProperties`
//                       The redacted version of `ClassName`.
// `TestType`: The type of the value being tested.
// `RedactedTestType`: The type of the redacted value being tested. Sometimes
//                     the type of variable being stored in the non-redacted and
//                     its redacted equivalent can differ.
// `UnredactedToRedactedCompare`: The shape of the function that will compare
//                                an unredacted config value to a redacted
//                                config value.
// `RedactedToRedactedCompare`: The shape of the function that will compare
//                              two redacted config values.
//
// Arguments:
// `config`: The FencedFrameConfig or FencedFrameProperties object being tested.
// `property`: A pointer to the class's field to test (e.g. `mapped_url_`)
// `entity`: The entity (kEmbedder or kContent) that the config should be
//           redacted for in the test.
// `is_defined`: Whether we expect the property to have a defined value.
// `is_opaque`: Whether we expect the property's defined value to be opaque.
// `unredacted_redacted_equality_fn`: A comparator function that has the
//     function signature is_eq(`type`, Redacted`type`). A return value of
//     `true` means equal; `false` means not equal.
// `redacted_redacted_equality_fn`: A comparator function that has the function
//     signature is_eq(Redacted`type`, Redacted`type`). A return value of `true`
//     means equal; `false` means not equal.
template <typename ClassName,
          typename RedactedClassName,
          typename TestType,
          typename RedactedTestType,
          typename UnredactedToRedactedCompare,
          typename RedactedToRedactedCompare>
void TestPropertyForEntityIsDefinedIsOpaque(
    ClassName config,
    std::optional<FencedFrameProperty<TestType>> ClassName::*property,
    const std::optional<
        blink::FencedFrame::RedactedFencedFrameProperty<RedactedTestType>>& (
        RedactedClassName::*redacted_property)() const,
    Entity entity,
    bool is_defined,
    bool is_opaque,
    UnredactedToRedactedCompare unredacted_redacted_equality_fn,
    RedactedToRedactedCompare redacted_redacted_equality_fn) {
  // Redact the config.
  RedactedClassName redacted_config = config.RedactFor(entity);
  if (is_defined) {
    // If the config has a value for the property, check that the redacted
    // version does too.
    ASSERT_TRUE((redacted_config.*redacted_property)().has_value());
    if (is_opaque) {
      // If the value should be opaque, check that it is.
      ASSERT_FALSE((redacted_config.*redacted_property)()
                       ->potentially_opaque_value.has_value());
    } else {
      // If the value should be transparent, check that it is, and that the
      // value was copied correctly.
      ASSERT_TRUE((redacted_config.*redacted_property)()
                      ->potentially_opaque_value.has_value());
      ASSERT_TRUE(unredacted_redacted_equality_fn(
          (config.*property)->GetValueIgnoringVisibility(),
          (redacted_config.*redacted_property)()
              ->potentially_opaque_value.value()));
    }
  } else {
    // If the config doesn't have a value for the property, check that the
    // redacted version also doesn't.
    ASSERT_FALSE((redacted_config.*redacted_property)().has_value());
  }

  // Copy the config using mojom serialization/deserialization.
  RedactedClassName copy;

  // deduce the mojo class being used
  using MojoClassName =
      std::conditional_t<std::is_same<FencedFrameConfig, ClassName>::value,
                         blink::mojom::FencedFrameConfig,
                         blink::mojom::FencedFrameProperties>;

  mojo::test::SerializeAndDeserialize<MojoClassName>(redacted_config, copy);
  // Check that the value for the property in the copy is the same as the
  // original.
  if (is_defined) {
    ASSERT_TRUE((copy.*redacted_property)().has_value());
    if (is_opaque) {
      ASSERT_FALSE(
          (copy.*redacted_property)()->potentially_opaque_value.has_value());
    } else {
      ASSERT_TRUE(
          (copy.*redacted_property)()->potentially_opaque_value.has_value());
      ASSERT_TRUE(redacted_redacted_equality_fn(
          (redacted_config.*redacted_property)()
              ->potentially_opaque_value.value(),
          (copy.*redacted_property)()->potentially_opaque_value.value()));
    }
  } else {
    ASSERT_FALSE((copy.*redacted_property)().has_value());
  }
}

class FencedFrameConfigMojomTraitsTest : public RenderViewHostTestHarness {
 public:
  // This helper function generates several test cases for a given property:
  // * An empty config (`property` has no value)
  // * A config with `dummy_value` for `property`, opaque to embedder and
  //   transparent to content.
  // * A config with `dummy_value` for `property`, transparent to embedder and
  //   opaque to content.
  //
  // Template Arguments:
  // `ClassName`: `FencedFrameConfig` or `FencedFrameProperties`
  //              Which base class to use for this test.
  // `RedactedClassName`: `RedactedFencedFrameConfig` or
  //                       `RedactedFencedFrameProperties`
  //                       The redacted version of `ClassName`.
  // `TestType`: The type of the value being tested.
  // `RedactedTestType`: The type of the redacted value being tested. Sometimes
  //                     the type of variable being stored in the non-redacted
  //                     and its redacted equivalent can differ.
  // `UnredactedToRedactedCompare`: The shape of the function that will compare
  //                                an unredacted config value to a redacted
  //                                config value.
  // `RedactedToRedactedCompare`: The shape of the function that will compare
  //                              two redacted config values.
  //
  // Arguments:
  // `property`: A pointer to the name of the field to test (e.g. `mapped_url_`)
  // `redacted_property`: The redacted equivalent of the `property` field.
  // `dummy_value`: A value that can be emplaced into `property`.
  // `unredacted_redacted_equality_fn`: A comparator function that has the
  //     function signature is_eq(`type`, Redacted`type`). A return value of
  //     `true` means equal; `false` means not equal.
  // `redacted_redacted_equality_fn`: A comparator function that has the
  // function
  //     signature is_eq(Redacted`type`, Redacted`type`). A return value of
  //     `true` means equal; `false` means not equal.
  template <typename ClassName,
            typename RedactedClassName,
            typename TestType,
            typename RedactedTestType,
            typename UnredactedToRedactedCompare,
            typename RedactedToRedactedCompare>
  void TestProperty(
      std::optional<FencedFrameProperty<TestType>> ClassName::*property,
      const std::optional<
          blink::FencedFrame::RedactedFencedFrameProperty<RedactedTestType>>& (
          RedactedClassName::*redacted_property)() const,
      TestType dummy_value,
      UnredactedToRedactedCompare unredacted_redacted_equality_fn,
      RedactedToRedactedCompare redacted_redacted_equality_fn) {
    // Test an empty config
    ClassName config;
    if constexpr (std::is_same<FencedFrameConfig, ClassName>::value) {
      config.urn_uuid_.emplace(GenerateUrnUuid());
    }
    TestPropertyForEntityIsDefinedIsOpaque<ClassName, RedactedClassName,
                                           TestType, RedactedTestType>(
        config, property, redacted_property, Entity::kEmbedder, false, false,
        unredacted_redacted_equality_fn, redacted_redacted_equality_fn);
    TestPropertyForEntityIsDefinedIsOpaque<ClassName, RedactedClassName,
                                           TestType, RedactedTestType>(
        config, property, redacted_property, Entity::kSameOriginContent, false,
        false, unredacted_redacted_equality_fn, redacted_redacted_equality_fn);
    TestPropertyForEntityIsDefinedIsOpaque<ClassName, RedactedClassName,
                                           TestType, RedactedTestType>(
        config, property, redacted_property, Entity::kCrossOriginContent, false,
        false, unredacted_redacted_equality_fn, redacted_redacted_equality_fn);

    // Test when `property` is opaque to embedder and transparent to content.
    (config.*property)
        .emplace(dummy_value, VisibilityToEmbedder::kOpaque,
                 VisibilityToContent::kTransparent);
    TestPropertyForEntityIsDefinedIsOpaque<ClassName, RedactedClassName,
                                           TestType, RedactedTestType>(
        config, property, redacted_property, Entity::kEmbedder, true, true,
        unredacted_redacted_equality_fn, redacted_redacted_equality_fn);
    TestPropertyForEntityIsDefinedIsOpaque<ClassName, RedactedClassName,
                                           TestType, RedactedTestType>(
        config, property, redacted_property, Entity::kSameOriginContent, true,
        false, unredacted_redacted_equality_fn, redacted_redacted_equality_fn);
    TestPropertyForEntityIsDefinedIsOpaque<ClassName, RedactedClassName,
                                           TestType, RedactedTestType>(
        config, property, redacted_property, Entity::kCrossOriginContent, true,
        true, unredacted_redacted_equality_fn, redacted_redacted_equality_fn);

    // Test when `property` is transparent to embedder and opaque to content.
    (config.*property)
        .emplace(dummy_value, VisibilityToEmbedder::kTransparent,
                 VisibilityToContent::kOpaque);
    TestPropertyForEntityIsDefinedIsOpaque<ClassName, RedactedClassName,
                                           TestType, RedactedTestType>(
        config, property, redacted_property, Entity::kEmbedder, true, false,
        unredacted_redacted_equality_fn, redacted_redacted_equality_fn);
    TestPropertyForEntityIsDefinedIsOpaque<ClassName, RedactedClassName,
                                           TestType, RedactedTestType>(
        config, property, redacted_property, Entity::kSameOriginContent, true,
        true, unredacted_redacted_equality_fn, redacted_redacted_equality_fn);
    TestPropertyForEntityIsDefinedIsOpaque<ClassName, RedactedClassName,
                                           TestType, RedactedTestType>(
        config, property, redacted_property, Entity::kCrossOriginContent, true,
        true, unredacted_redacted_equality_fn, redacted_redacted_equality_fn);
  }
};

TEST_F(FencedFrameConfigMojomTraitsTest, ConfigMojomTraitsInternalUrnTest) {
  GURL test_url("test_url");

  struct TestCase {
    GURL urn;
    bool pass = false;
  } test_cases[] = {
      {GURL(), false},
      {GURL("https://example.com"), false},
      {GURL("data:text/html<h1>MyWebsite"), false},
      {GURL("urn:abcd:f81d4fae-7dec-11d0-a765-00a0c91e6bf6"), false},
      {GURL("urn:uuid:foo"), false},
      {GURL("urn:uuid:f81d4faea7deca11d0aa765a00a0c91e6bf6"), false},
      {GURL("urn:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6"), true},
      {GenerateUrnUuid(), true},
  };

  for (const TestCase& test_case : test_cases) {
    FencedFrameConfig browser_config(test_case.urn, test_url);
    RedactedFencedFrameConfig input_config =
        browser_config.RedactFor(FencedFrameEntity::kEmbedder);
    RedactedFencedFrameConfig output_config;

    if (test_case.pass) {
      ASSERT_TRUE(
          mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameConfig>(
              input_config, output_config));
    } else {
      ASSERT_FALSE(
          mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameConfig>(
              input_config, output_config));
    }
  }
}

TEST_F(FencedFrameConfigMojomTraitsTest, ConfigMojomTraitsModeTest) {
  std::vector<blink::FencedFrame::DeprecatedFencedFrameMode> modes = {
      blink::FencedFrame::DeprecatedFencedFrameMode::kDefault,
      blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds,
  };
  std::vector<FencedFrameEntity> entities = {
      FencedFrameEntity::kEmbedder,
      FencedFrameEntity::kSameOriginContent,
  };
  GURL test_url("test_url");
  GURL test_urn = GenerateUrnUuid();
  for (blink::FencedFrame::DeprecatedFencedFrameMode& mode : modes) {
    FencedFrameConfig browser_config(test_urn, test_url);
    browser_config.mode_ = mode;
    FencedFrameProperties browser_properties(browser_config);
    for (FencedFrameEntity& entity : entities) {
      RedactedFencedFrameConfig input_config = browser_config.RedactFor(entity);
      ASSERT_TRUE(browser_config.mode_ == input_config.mode());

      RedactedFencedFrameConfig output_config;
      mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameConfig>(
          input_config, output_config);
      ASSERT_TRUE(input_config.mode() == output_config.mode());

      RedactedFencedFrameProperties input_properties =
          browser_properties.RedactFor(entity);
      ASSERT_TRUE(browser_properties.mode() == input_properties.mode());

      RedactedFencedFrameProperties output_properties;
      mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameProperties>(
          input_properties, output_properties);
      ASSERT_TRUE(input_properties.mode() == output_properties.mode());
    }
  }
}

TEST_F(FencedFrameConfigMojomTraitsTest, ConfigMojomTraitsNullInternalUrnTest) {
  FencedFrameConfig browser_config;
  RedactedFencedFrameConfig input_config =
      browser_config.RedactFor(FencedFrameEntity::kEmbedder);
  RedactedFencedFrameConfig output_config;
  EXPECT_DEATH_IF_SUPPORTED(
      mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameConfig>(
          input_config, output_config),
      "");
}

// C++23: Can replace AndThen(opt, acc) with opt.and_then(acc).
template <typename Opt, typename Acc>
std::optional<GURL> AndThen(const Opt& opt, Acc acc) {
  return opt ? std::invoke(acc, *opt) : std::nullopt;
}

// Projections for [un]redacted fenced frame configs to allow comparisons. These
// only compare the `mapped_url` field for convenience. (We don't need an
// equality operator for configs outside of tests, so it would be wasteful to
// declare it as default in the class declaration.)
std::optional<GURL> Project(const FencedFrameConfig& config) {
  return AndThen(config.mapped_url(), [](const FencedFrameProperty<GURL>& url) {
    return url.GetValueForEntity(Entity::kEmbedder);
  });
}
std::optional<GURL> Project(const RedactedFencedFrameConfig& config) {
  return AndThen(config.mapped_url(),
                 &blink::FencedFrame::RedactedFencedFrameProperty<
                     GURL>::potentially_opaque_value);
}

TEST_F(FencedFrameConfigMojomTraitsTest, ConfigMojomTraitsTest) {
  GURL test_url("test_url");

  // See the above tests for `urn`.

  // Test `mapped_url`.
  {
    auto eq_fn = [](const GURL& a, const GURL& b) { return a == b; };
    TestProperty(&FencedFrameConfig::mapped_url_,
                 &RedactedFencedFrameConfig::mapped_url, test_url, eq_fn,
                 eq_fn);
    TestProperty(&FencedFrameProperties::mapped_url_,
                 &RedactedFencedFrameProperties::mapped_url, test_url, eq_fn,
                 eq_fn);
  }

  // Test `container_size` and `content_size`.
  {
    gfx::Size test_size(100, 200);
    auto eq_fn = [](const gfx::Size& a, const gfx::Size& b) { return a == b; };

    TestProperty(&FencedFrameConfig::container_size_,
                 &RedactedFencedFrameConfig::container_size, test_size, eq_fn,
                 eq_fn);
    TestProperty(&FencedFrameProperties::container_size_,
                 &RedactedFencedFrameProperties::container_size, test_size,
                 eq_fn, eq_fn);

    TestProperty(&FencedFrameConfig::content_size_,
                 &RedactedFencedFrameConfig::content_size, test_size, eq_fn,
                 eq_fn);
    TestProperty(&FencedFrameProperties::content_size_,
                 &RedactedFencedFrameProperties::content_size, test_size, eq_fn,
                 eq_fn);
  }

  // Test `deprecated_should_freeze_initial_size`.
  {
    auto eq_fn = [](const bool a, const bool b) { return a == b; };
    TestProperty(
        &FencedFrameConfig::deprecated_should_freeze_initial_size_,
        &RedactedFencedFrameConfig::deprecated_should_freeze_initial_size, true,
        eq_fn, eq_fn);
    TestProperty(
        &FencedFrameProperties::deprecated_should_freeze_initial_size_,
        &RedactedFencedFrameProperties::deprecated_should_freeze_initial_size,
        true, eq_fn, eq_fn);

    TestProperty(
        &FencedFrameConfig::deprecated_should_freeze_initial_size_,
        &RedactedFencedFrameConfig::deprecated_should_freeze_initial_size,
        false, eq_fn, eq_fn);
    TestProperty(
        &FencedFrameProperties::deprecated_should_freeze_initial_size_,
        &RedactedFencedFrameProperties::deprecated_should_freeze_initial_size,
        false, eq_fn, eq_fn);
  }

  // Test `ad_auction_data`.
  {
    AdAuctionData test_ad_auction_data = {url::Origin::Create(test_url),
                                          std::string("test_name")};
    auto eq_fn = [](const AdAuctionData& a, const AdAuctionData& b) {
      return a.interest_group_owner == b.interest_group_owner &&
             a.interest_group_name == b.interest_group_name;
    };

    TestProperty(&FencedFrameConfig::ad_auction_data_,
                 &RedactedFencedFrameConfig::ad_auction_data,
                 test_ad_auction_data, eq_fn, eq_fn);
    TestProperty(&FencedFrameProperties::ad_auction_data_,
                 &RedactedFencedFrameProperties::ad_auction_data,
                 test_ad_auction_data, eq_fn, eq_fn);
  }

  // Test `nested_configs`.
  {
    FencedFrameConfig test_nested_config(GenerateUrnUuid(), test_url);
    // Returns a lambda that compares two ranges using the given `proj`.
    const auto cmp = [](const auto& proj) {
      return [&](const auto& a, const auto& b) {
        return base::ranges::equal(a, b, {}, proj, proj);
      };
    };

    {
      std::vector<FencedFrameConfig> test_nested_configs = {test_nested_config};
      const auto eq = cmp([](const auto& elem) { return Project(elem); });
      TestProperty(&FencedFrameConfig::nested_configs_,
                   &RedactedFencedFrameConfig::nested_configs,
                   test_nested_configs, eq, eq);
    }

    {
      GURL test_urn("urn:uuid:abcd");
      std::vector<std::pair<GURL, FencedFrameConfig>>
          test_nested_urn_config_pairs = {{test_urn, test_nested_config}};
      const auto eq = cmp([](const auto& elem) {
        return std::make_pair(elem.first, Project(elem.second));
      });
      TestProperty(&FencedFrameProperties::nested_urn_config_pairs_,
                   &RedactedFencedFrameProperties::nested_urn_config_pairs,
                   test_nested_urn_config_pairs, eq, eq);
    }
  }

  // Test `shared_storage_budget_metadata`.
  {
    SharedStorageBudgetMetadata test_shared_storage_budget_metadata = {
        net::SchemefulSite(test_url), 0.5, /*top_navigated=*/true};
    const auto eq = [](const SharedStorageBudgetMetadata& a,
                       const SharedStorageBudgetMetadata& b) {
      return std::tie(a.site, a.budget_to_charge, a.top_navigated) ==
             std::tie(b.site, b.budget_to_charge, b.top_navigated);
    };
    TestProperty(&FencedFrameConfig::shared_storage_budget_metadata_,
                 &RedactedFencedFrameConfig::shared_storage_budget_metadata,
                 test_shared_storage_budget_metadata, eq, eq);

    const auto ptr_eq = [&](const SharedStorageBudgetMetadata* a,
                            const SharedStorageBudgetMetadata& b) {
      return eq(*a, b);
    };
    TestProperty(&FencedFrameProperties::shared_storage_budget_metadata_,
                 &RedactedFencedFrameProperties::shared_storage_budget_metadata,
                 static_cast<raw_ptr<const SharedStorageBudgetMetadata>>(
                     &test_shared_storage_budget_metadata),
                 ptr_eq, eq);
  }
}

// Test `has_fenced_frame_reporting`, which only appears in
// FencedFrameProperties, and does not use the redacted mechanism used by other
// fields.
TEST_F(FencedFrameConfigMojomTraitsTest,
       PropertiesHasFencedFrameReportingTest) {
  FencedFrameProperties properties;
  RedactedFencedFrameProperties input_properties =
      properties.RedactFor(FencedFrameEntity::kEmbedder);
  EXPECT_FALSE(input_properties.has_fenced_frame_reporting());
  RedactedFencedFrameProperties output_properties;
  mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameProperties>(
      input_properties, output_properties);
  EXPECT_FALSE(output_properties.has_fenced_frame_reporting());

  // Create a reporting service with a dummy SharedURLLoaderFactory.
  properties.fenced_frame_reporter_ = FencedFrameReporter::CreateForFledge(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(nullptr),
      /*browser_context=*/browser_context(),
      /*direct_seller_is_seller=*/false,
      /*private_aggregation_manager=*/nullptr,
      /*main_frame_origin=*/url::Origin(),
      /*winner_origin=*/url::Origin(),
      /*winner_aggregation_coordinator_origin=*/std::nullopt);
  input_properties = properties.RedactFor(FencedFrameEntity::kEmbedder);
  EXPECT_TRUE(input_properties.has_fenced_frame_reporting());
  mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameProperties>(
      input_properties, output_properties);
  EXPECT_TRUE(output_properties.has_fenced_frame_reporting());
}

// Test `can_disable_untrusted_network`, which only appears in
// FencedFrameProperties, and does not use the redacted mechanism used by other
// fields.
TEST_F(FencedFrameConfigMojomTraitsTest,
       PropertiesCanDisableUntrustedNetworkTest) {
  FencedFrameProperties properties;
  RedactedFencedFrameProperties input_properties;
  RedactedFencedFrameProperties output_properties;

  properties.can_disable_untrusted_network_ = true;
  input_properties =
      properties.RedactFor(FencedFrameEntity::kSameOriginContent);
  EXPECT_TRUE(input_properties.can_disable_untrusted_network());
  mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameProperties>(
      input_properties, output_properties);
  EXPECT_TRUE(output_properties.can_disable_untrusted_network());
  // `can_disable_untrusted_network` is always redacted to false for
  // cross-origin contexts.
  input_properties =
      properties.RedactFor(FencedFrameEntity::kCrossOriginContent);
  EXPECT_FALSE(input_properties.can_disable_untrusted_network());
  mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameProperties>(
      input_properties, output_properties);
  EXPECT_FALSE(output_properties.can_disable_untrusted_network());

  properties.can_disable_untrusted_network_ = false;
  input_properties =
      properties.RedactFor(FencedFrameEntity::kSameOriginContent);
  EXPECT_FALSE(input_properties.can_disable_untrusted_network());
  mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameProperties>(
      input_properties, output_properties);
  EXPECT_FALSE(output_properties.can_disable_untrusted_network());
  input_properties =
      properties.RedactFor(FencedFrameEntity::kCrossOriginContent);
  EXPECT_FALSE(input_properties.can_disable_untrusted_network());
  mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameProperties>(
      input_properties, output_properties);
  EXPECT_FALSE(output_properties.can_disable_untrusted_network());
}

}  // namespace content
