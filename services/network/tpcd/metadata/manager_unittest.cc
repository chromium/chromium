// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tpcd/metadata/manager.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_enums.mojom-shared.h"
#include "components/content_settings/core/common/features.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network::tpcd::metadata {

class ManagerTest : public ::testing::Test,
                    public ::testing::WithParamInterface<
                        /*kTpcdMetadataGrants*/ bool> {
 public:
  ManagerTest() = default;
  ~ManagerTest() override = default;

  bool IsTpcdMetadataGrantsEnabled() const { return GetParam(); }

  ContentSettingPatternSource CreateContentSetting(
      const std::string& primary_pattern_spec,
      const std::string& secondary_pattern_spec) {
    const auto primary_pattern =
        ContentSettingsPattern::FromString(primary_pattern_spec);
    const auto secondary_pattern =
        ContentSettingsPattern::FromString(secondary_pattern_spec);

    EXPECT_TRUE(primary_pattern.IsValid());
    EXPECT_TRUE(secondary_pattern.IsValid());

    base::Value value(ContentSetting::CONTENT_SETTING_ALLOW);

    content_settings::RuleMetaData rule_metadata =
        content_settings::RuleMetaData();
    rule_metadata.set_tpcd_metadata_rule_source(
        content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST);

    return ContentSettingPatternSource(
        primary_pattern, secondary_pattern, std::move(value),
        content_settings::ProviderType::kNone, /*incognito=*/false,
        std::move(rule_metadata));
  }

  Manager* GetManager() {
    if (!manager_) {
      manager_ = std::make_unique<Manager>();
    }
    return manager_.get();
  }

 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsTpcdMetadataGrantsEnabled()) {
      enabled_features.push_back(net::features::kTpcdMetadataGrants);
    } else {
      disabled_features.push_back(net::features::kTpcdMetadataGrants);
    }

    scoped_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
  std::unique_ptr<Manager> manager_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    ManagerTest,
    ::testing::Bool());

TEST_P(ManagerTest, SetGrants) {
  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  ContentSettingsForOneType grants;
  grants.emplace_back(
      CreateContentSetting(primary_pattern_spec, secondary_pattern_spec));
  EXPECT_EQ(grants.size(), 1u);

  GetManager()->SetGrants(grants);

  if (!IsTpcdMetadataGrantsEnabled()) {
    EXPECT_TRUE(GetManager()->GetGrants().empty());
  } else {
    EXPECT_EQ(GetManager()->GetGrants().size(), 1u);
    EXPECT_EQ(GetManager()->GetGrants().front().primary_pattern.ToString(),
              primary_pattern_spec);
    EXPECT_EQ(GetManager()->GetGrants().front().secondary_pattern.ToString(),
              secondary_pattern_spec);
    EXPECT_EQ(
        GetManager()->GetGrants().front().metadata.tpcd_metadata_rule_source(),
        content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST);
  }
}

TEST_P(ManagerTest, GetContentSetting) {
  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  GURL first_party_url = GURL(secondary_pattern_spec);
  GURL third_party_url = GURL(primary_pattern_spec);
  GURL third_party_url_no_grants = GURL("https://www.bar.com");

  ContentSettingsForOneType grants;
  grants.emplace_back(
      CreateContentSetting(primary_pattern_spec, secondary_pattern_spec));
  EXPECT_EQ(grants.size(), 1u);

  GetManager()->SetGrants(grants);

  {
    content_settings::SettingInfo out_info;
    EXPECT_EQ(GetManager()->GetContentSetting(third_party_url, first_party_url,
                                              &out_info),
              IsTpcdMetadataGrantsEnabled() ? CONTENT_SETTING_ALLOW
                                            : CONTENT_SETTING_BLOCK);
    EXPECT_EQ(out_info.primary_pattern.ToString(),
              IsTpcdMetadataGrantsEnabled() ? primary_pattern_spec : "*");
    EXPECT_EQ(out_info.secondary_pattern.ToString(),
              IsTpcdMetadataGrantsEnabled() ? secondary_pattern_spec : "*");
    EXPECT_EQ(out_info.metadata.tpcd_metadata_rule_source(),
              IsTpcdMetadataGrantsEnabled()
                  ? content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST
                  : content_settings::mojom::TpcdMetadataRuleSource::
                        SOURCE_UNSPECIFIED);
  }

  {
    content_settings::SettingInfo out_info;
    EXPECT_EQ(GetManager()->GetContentSetting(third_party_url_no_grants,
                                              first_party_url, &out_info),
              CONTENT_SETTING_BLOCK);
    EXPECT_EQ(out_info.primary_pattern.ToString(), "*");
    EXPECT_EQ(out_info.secondary_pattern.ToString(), "*");
    EXPECT_EQ(
        out_info.metadata.tpcd_metadata_rule_source(),
        content_settings::mojom::TpcdMetadataRuleSource::SOURCE_UNSPECIFIED);
  }
}

}  // namespace network::tpcd::metadata
