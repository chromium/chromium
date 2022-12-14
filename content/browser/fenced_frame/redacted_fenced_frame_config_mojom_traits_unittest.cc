// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

#include "base/test/gtest_util.h"
#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config_mojom_traits.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame_config.mojom.h"

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
// Arguments:
// `type`: `FencedFrameConfig` or `FencedFrameProperties`.
//         An object `config` with this type should already exist in scope.
// `property`: The name of the field to test (e.g. `mapped_url_`)
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
#define TEST_PROPERTY_FOR_ENTITY_IS_DEFINED_IS_OPAQUE(                        \
    type, property, entity, is_defined, is_opaque,                            \
    unredacted_redacted_equality_fn, redacted_redacted_equality_fn)           \
  {                                                                           \
    /* Redact the config. */                                                  \
    Redacted##type redacted_config = config.RedactFor(entity);                \
    if (is_defined) {                                                         \
      /* If the config has a value for the property, check that the redacted  \
       * version does too. */                                                 \
      ASSERT_TRUE(redacted_config.property.has_value());                      \
      if (is_opaque) {                                                        \
        /* If the value should be opaque, check that it is. */                \
        ASSERT_FALSE(                                                         \
            redacted_config.property->potentially_opaque_value.has_value());  \
      } else {                                                                \
        /* If the value should be transparent, check that it is, and that the \
         * value was copied correctly. */                                     \
        ASSERT_TRUE(                                                          \
            redacted_config.property->potentially_opaque_value.has_value());  \
        ASSERT_TRUE(unredacted_redacted_equality_fn(                          \
            config.property->GetValueIgnoringVisibility(),                    \
            redacted_config.property->potentially_opaque_value.value()));     \
      }                                                                       \
    } else {                                                                  \
      /* If the config doesn't have a value for the property, check that the  \
       * redacted version also doesn't. */                                    \
      ASSERT_FALSE(redacted_config.property.has_value());                     \
    }                                                                         \
                                                                              \
    /* Copy the config using mojom serialization/deserialization. */          \
    Redacted##type copy;                                                      \
    mojo::test::SerializeAndDeserialize<blink::mojom::type>(redacted_config,  \
                                                            copy);            \
    /* Check that the value for the property in the copy is the same as the   \
     * original. */                                                           \
    if (is_defined) {                                                         \
      ASSERT_TRUE(copy.property.has_value());                                 \
      if (is_opaque) {                                                        \
        ASSERT_FALSE(copy.property->potentially_opaque_value.has_value());    \
      } else {                                                                \
        ASSERT_TRUE(copy.property->potentially_opaque_value.has_value());     \
        ASSERT_TRUE(redacted_redacted_equality_fn(                            \
            redacted_config.property->potentially_opaque_value.value(),       \
            copy.property->potentially_opaque_value.value()));                \
      }                                                                       \
    } else {                                                                  \
      ASSERT_FALSE(copy.property.has_value());                                \
    }                                                                         \
  }

// This macro generates several test cases for a given property. We test:
// * An empty config (`property` has no value)
// * A config with `dummy_value` for `property`, opaque to embedder and
//   transparent to content.
// * A config with `dummy_value` for `property`, transparent to embedder and
//   opaque to content.
//
// Arguments:
// `type`: `FencedFrameConfig` or `FencedFrameProperties`.
//         An object `config` with this type should already exist in scope.
// `property`: The name of the field to test (e.g. `mapped_url_`)
// `dummy_value`: A value that can be emplaced into `property`.
// `unredacted_redacted_equality_fn`: A comparator function that has the
//     function signature is_eq(`type`, Redacted`type`). A return value of
//     `true` means equal; `false` means not equal.
// `redacted_redacted_equality_fn`: A comparator function that has the function
//     signature is_eq(Redacted`type`, Redacted`type`). A return value of `true`
//     means equal; `false` means not equal.
#define TEST_PROPERTY(type, property, dummy_value,                            \
                      unredacted_redacted_equality_fn,                        \
                      redacted_redacted_equality_fn)                          \
  {                                                                           \
    /* Test an empty config */                                                \
    type config;                                                              \
    if constexpr (std::is_same<FencedFrameConfig, type>::value) {             \
      config.urn_.emplace(GenerateUrnUuid());                                 \
    }                                                                         \
    TEST_PROPERTY_FOR_ENTITY_IS_DEFINED_IS_OPAQUE(                            \
        type, property, Entity::kEmbedder, false, false,                      \
        unredacted_redacted_equality_fn, redacted_redacted_equality_fn);      \
    TEST_PROPERTY_FOR_ENTITY_IS_DEFINED_IS_OPAQUE(                            \
        type, property, Entity::kContent, false, false,                       \
        unredacted_redacted_equality_fn, redacted_redacted_equality_fn);      \
                                                                              \
    /* Test when `property` is opaque to embedder and transparent to content. \
     */                                                                       \
    config.property.emplace(dummy_value, VisibilityToEmbedder::kOpaque,       \
                            VisibilityToContent::kTransparent);               \
    TEST_PROPERTY_FOR_ENTITY_IS_DEFINED_IS_OPAQUE(                            \
        type, property, Entity::kEmbedder, true, true,                        \
        unredacted_redacted_equality_fn, redacted_redacted_equality_fn);      \
    TEST_PROPERTY_FOR_ENTITY_IS_DEFINED_IS_OPAQUE(                            \
        type, property, Entity::kContent, true, false,                        \
        unredacted_redacted_equality_fn, redacted_redacted_equality_fn);      \
                                                                              \
    /* Test when `property` is transparent to embedder and opaque to content. \
     */                                                                       \
    config.property.emplace(dummy_value, VisibilityToEmbedder::kTransparent,  \
                            VisibilityToContent::kOpaque);                    \
    TEST_PROPERTY_FOR_ENTITY_IS_DEFINED_IS_OPAQUE(                            \
        type, property, Entity::kEmbedder, true, false,                       \
        unredacted_redacted_equality_fn, redacted_redacted_equality_fn);      \
    TEST_PROPERTY_FOR_ENTITY_IS_DEFINED_IS_OPAQUE(                            \
        type, property, Entity::kContent, true, true,                         \
        unredacted_redacted_equality_fn, redacted_redacted_equality_fn);      \
  }

// Compare equality of two lists of nested configs.
// Only compares the `mapped_url` field for convenience. (We don't need an
// equality operator for configs outside of tests, so it would be wasteful to
// declare it as default in the class declaration.)
#define NESTED_CONFIG_EQ_FN(type1, accessor1, type2, accessor2)             \
  [](const std::vector<type1>& a, const std::vector<type2>& b) {            \
    if (a.size() != b.size()) {                                             \
      return false;                                                         \
    }                                                                       \
    for (size_t i = 0; i < a.size(); ++i) {                                 \
      if (!a[i].mapped_url_.has_value() && !b[i].mapped_url_.has_value()) { \
        continue;                                                           \
      } else if (a[i].mapped_url_.has_value() &&                            \
                 b[i].mapped_url_.has_value()) {                            \
        if (a[i].mapped_url_->accessor1 == b[i].mapped_url_->accessor2) {   \
          continue;                                                         \
        }                                                                   \
        return false;                                                       \
      } else {                                                              \
        return false;                                                       \
      }                                                                     \
    }                                                                       \
    return true;                                                            \
  }

// Compare equality of two lists of (urn, nested config) pairs.
// Only compares the `mapped_url` field for convenience.
#define NESTED_URN_CONFIG_PAIR_EQ_FN(type1, accessor1, type2, accessor2) \
  [](const std::vector<std::pair<GURL, type1>>& a,                       \
     const std::vector<std::pair<GURL, type2>>& b) {                     \
    if (a.size() != b.size()) {                                          \
      return false;                                                      \
    }                                                                    \
    for (size_t i = 0; i < a.size(); ++i) {                              \
      if (a[i].first != b[i].first) {                                    \
        return false;                                                    \
      }                                                                  \
      if (!a[i].second.mapped_url_.has_value() &&                        \
          !b[i].second.mapped_url_.has_value()) {                        \
        continue;                                                        \
      } else if (a[i].second.mapped_url_.has_value() &&                  \
                 b[i].second.mapped_url_.has_value()) {                  \
        if (a[i].second.mapped_url_->accessor1 ==                        \
            b[i].second.mapped_url_->accessor2) {                        \
          continue;                                                      \
        }                                                                \
        return false;                                                    \
      } else {                                                           \
        return false;                                                    \
      }                                                                  \
    }                                                                    \
    return true;                                                         \
  }

TEST(FencedFrameConfigMojomTraitsTest, ConfigMojomTraitsInternalUrnTest) {
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

TEST(FencedFrameConfigMojomTraitsTest, ConfigMojomTraitsNullInternalUrnTest) {
  FencedFrameConfig browser_config;
  RedactedFencedFrameConfig input_config =
      browser_config.RedactFor(FencedFrameEntity::kEmbedder);
  RedactedFencedFrameConfig output_config;
  EXPECT_DEATH(
      mojo::test::SerializeAndDeserialize<blink::mojom::FencedFrameConfig>(
          input_config, output_config),
      "");
}

TEST(FencedFrameConfigMojomTraitsTest, ConfigMojomTraitsTest) {
  GURL test_url("test_url");

  // See the above tests for `urn`.

  // Test `mapped_url`.
  {
    auto eq_fn = [](const GURL& a, const GURL& b) { return a == b; };
    TEST_PROPERTY(FencedFrameConfig, mapped_url_, test_url, eq_fn, eq_fn);
    TEST_PROPERTY(FencedFrameProperties, mapped_url_, test_url, eq_fn, eq_fn);
  }

  // Test `container_size` and `content_size`.
  {
    gfx::Size test_size(100, 200);
    auto eq_fn = [](const gfx::Size& a, const gfx::Size& b) { return a == b; };

    TEST_PROPERTY(FencedFrameConfig, container_size_, test_size, eq_fn, eq_fn);
    TEST_PROPERTY(FencedFrameProperties, container_size_, test_size, eq_fn,
                  eq_fn);

    TEST_PROPERTY(FencedFrameConfig, content_size_, test_size, eq_fn, eq_fn);
    TEST_PROPERTY(FencedFrameProperties, content_size_, test_size, eq_fn,
                  eq_fn);
  }

  // Test `deprecated_should_freeze_initial_size`.
  {
    auto eq_fn = [](const bool a, const bool b) { return a == b; };
    TEST_PROPERTY(FencedFrameConfig, deprecated_should_freeze_initial_size_,
                  true, eq_fn, eq_fn);
    TEST_PROPERTY(FencedFrameProperties, deprecated_should_freeze_initial_size_,
                  true, eq_fn, eq_fn);
    TEST_PROPERTY(FencedFrameConfig, deprecated_should_freeze_initial_size_,
                  false, eq_fn, eq_fn);
    TEST_PROPERTY(FencedFrameProperties, deprecated_should_freeze_initial_size_,
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
    TEST_PROPERTY(FencedFrameConfig, ad_auction_data_, test_ad_auction_data,
                  eq_fn, eq_fn);
    TEST_PROPERTY(FencedFrameProperties, ad_auction_data_, test_ad_auction_data,
                  eq_fn, eq_fn);
  }

  // Test `nested_configs`.
  {
    FencedFrameConfig test_nested_config(GenerateUrnUuid(), test_url);

    {
      std::vector<FencedFrameConfig> test_nested_configs = {test_nested_config};
      auto unredacted_redacted_eq_fn = NESTED_CONFIG_EQ_FN(
          FencedFrameConfig, GetValueForEntity(Entity::kEmbedder),
          RedactedFencedFrameConfig, potentially_opaque_value);
      auto redacted_redacted_eq_fn = NESTED_CONFIG_EQ_FN(
          RedactedFencedFrameConfig, potentially_opaque_value,
          RedactedFencedFrameConfig, potentially_opaque_value);
      TEST_PROPERTY(FencedFrameConfig, nested_configs_, test_nested_configs,
                    unredacted_redacted_eq_fn, redacted_redacted_eq_fn);
    }

    {
      GURL test_urn("urn:uuid:abcd");
      std::vector<std::pair<GURL, FencedFrameConfig>>
          test_nested_urn_config_pairs = {{test_urn, test_nested_config}};
      auto unredacted_redacted_eq_fn = NESTED_URN_CONFIG_PAIR_EQ_FN(
          FencedFrameConfig, GetValueForEntity(Entity::kEmbedder),
          RedactedFencedFrameConfig, potentially_opaque_value);
      auto redacted_redacted_eq_fn = NESTED_URN_CONFIG_PAIR_EQ_FN(
          RedactedFencedFrameConfig, potentially_opaque_value,
          RedactedFencedFrameConfig, potentially_opaque_value);
      TEST_PROPERTY(FencedFrameProperties, nested_urn_config_pairs_,
                    test_nested_urn_config_pairs, unredacted_redacted_eq_fn,
                    redacted_redacted_eq_fn);
    }
  }

  {
    SharedStorageBudgetMetadata test_shared_storage_budget_metadata = {
        url::Origin::Create(test_url), 0.5};
    auto eq_fn = [](const SharedStorageBudgetMetadata& a,
                    const SharedStorageBudgetMetadata& b) {
      return a.origin == b.origin && a.budget_to_charge == b.budget_to_charge;
    };
    TEST_PROPERTY(FencedFrameConfig, shared_storage_budget_metadata_,
                  test_shared_storage_budget_metadata, eq_fn, eq_fn);

    auto pointer_value_eq_fn =
        [](const raw_ptr<const SharedStorageBudgetMetadata>& a,
           const SharedStorageBudgetMetadata& b) {
          return a->origin == b.origin &&
                 a->budget_to_charge == b.budget_to_charge;
        };
    TEST_PROPERTY(FencedFrameProperties, shared_storage_budget_metadata_,
                  &test_shared_storage_budget_metadata, pointer_value_eq_fn,
                  eq_fn);
  }

  // Test `reporting_metadata`.
  {
    auto test_reporting_metadata = blink::FencedFrame::FencedFrameReporting();
    test_reporting_metadata
        .metadata[blink::FencedFrame::ReportingDestination::kBuyer]["test"] =
        test_url;
    auto eq_fn = [](const ReportingMetadata& a, const ReportingMetadata& b) {
      return a.metadata == b.metadata;
    };
    TEST_PROPERTY(FencedFrameConfig, reporting_metadata_,
                  test_reporting_metadata, eq_fn, eq_fn);
    TEST_PROPERTY(FencedFrameProperties, reporting_metadata_,
                  test_reporting_metadata, eq_fn, eq_fn);
  }
}

}  // namespace content
