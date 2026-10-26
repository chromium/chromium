// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service.h"

#import "components/enterprise/data_controls/core/browser/action_context.h"
#import "components/enterprise/data_controls/core/browser/test_utils.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/signin_test_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace data_controls {

namespace {

constexpr Verdict::TriggeredRuleKey kFirstRuleIndex = {
    .index = 0,
    .machine_scope = true,
};
constexpr char kFirstRuleID[] = "1234";

class IOSRulesServiceTest : public PlatformTest {
 public:
  IOSRulesServiceTest() {}
  void SetUp() override {
    PlatformTest::SetUp();

    // `TestProfileManagerIOS` requires the personal profile to be created
    // before adding any managed profiles.
    profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
    managed_profile1_ = AddManagedProfileWithBuilder(
        profile_manager_, [FakeSystemIdentity fakeManagedIdentity]);
    managed_profile2_ = AddManagedProfileWithBuilder(
        profile_manager_,
        [FakeSystemIdentity identityWithEmail:@"bar@foo.com"]);
  }

  const GURL google_url() const { return GURL("https://google.com"); }

  void ExpectBlockVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kBlock);
    EXPECT_EQ(verdict.triggered_rules().size(), 1u);
    EXPECT_TRUE(verdict.triggered_rules().count(kFirstRuleIndex));
    EXPECT_EQ(verdict.triggered_rules().at(kFirstRuleIndex).rule_name, "block");
    EXPECT_EQ(verdict.triggered_rules().at(kFirstRuleIndex).rule_id,
              kFirstRuleID);
  }

  void ExpectWarnVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kWarn);
    EXPECT_EQ(verdict.triggered_rules().size(), 1u);
    EXPECT_TRUE(verdict.triggered_rules().count(kFirstRuleIndex));
    EXPECT_EQ(verdict.triggered_rules().at(kFirstRuleIndex).rule_name, "warn");
    EXPECT_EQ(verdict.triggered_rules().at(kFirstRuleIndex).rule_id,
              kFirstRuleID);
  }

  void ExpectAllowVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kAllow);
    EXPECT_TRUE(verdict.triggered_rules().empty());
  }

  void ExpectNoVerdict(Verdict verdict) const {
    ASSERT_EQ(verdict.level(), Rule::Level::kNotSet);
    EXPECT_TRUE(verdict.triggered_rules().empty());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  // Add local state to test ApplicationContext. Required by
  // TestProfileManagerIOS.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> managed_profile1_;
  raw_ptr<TestProfileIOS> managed_profile2_;
};

}  // namespace

TEST_F(IOSRulesServiceTest, NoRuleSet) {
  ExpectNoVerdict(
      IOSRulesServiceFactory::GetInstance()
          ->GetForProfile(managed_profile1_)
          ->GetPasteVerdict(google_url(), GURL(), managed_profile1_, nullptr));
  ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                      ->GetForProfile(managed_profile1_)
                      ->GetCopyToOSClipboardVerdict(
                          /*source*/ google_url()));
  ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                      ->GetForProfile(managed_profile1_)
                      ->GetCopyRestrictedBySourceVerdict(
                          /*source*/ google_url()));
}

TEST_F(IOSRulesServiceTest, SourceURL) {
  {
    // Restriction level is set as `BLOCK`.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                        "name": "block",
                        "rule_id": "1234",
                        "sources": {
                          "urls": ["google.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "BLOCK"}
                        ]
                      })"});

    ExpectBlockVerdict(IOSRulesServiceFactory::GetInstance()
                           ->GetForProfile(managed_profile1_)
                           ->GetPasteVerdict(google_url(), GURL(),
                                             managed_profile1_, nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                          managed_profile1_));
    ExpectBlockVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectBlockVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }

  {
    // Restriction level is set as `WARN`.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                        "name": "warn",
                        "rule_id": "1234",
                        "sources": {
                          "urls": ["google.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "WARN"}
                        ]
                      })"});

    ExpectWarnVerdict(IOSRulesServiceFactory::GetInstance()
                          ->GetForProfile(managed_profile1_)
                          ->GetPasteVerdict(google_url(), GURL(),
                                            managed_profile1_, nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                          managed_profile1_));
    ExpectWarnVerdict(IOSRulesServiceFactory::GetInstance()
                          ->GetForProfile(managed_profile1_)
                          ->GetCopyToOSClipboardVerdict(
                              /*source*/ google_url()));
    ExpectWarnVerdict(IOSRulesServiceFactory::GetInstance()
                          ->GetForProfile(managed_profile1_)
                          ->GetCopyRestrictedBySourceVerdict(
                              /*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                              "name": "allow",
                              "rule_id": "1234",
                              "sources": {
                                "urls": ["google.com"]
                              },
                              "restrictions": [
                                {"class": "CLIPBOARD", "level": "ALLOW"}
                              ]
                            })",
                                                    R"({
                              "name": "warn",
                              "rule_id": "5678",
                              "sources": {
                                "urls": ["google.com"]
                              },
                              "restrictions": [
                                {"class": "CLIPBOARD", "level": "WARN"}
                              ]
                            })"});
    ExpectAllowVerdict(IOSRulesServiceFactory::GetInstance()
                           ->GetForProfile(managed_profile1_)
                           ->GetPasteVerdict(google_url(), GURL(),
                                             managed_profile1_, nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                          managed_profile1_));
    ExpectAllowVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectAllowVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }
}

TEST_F(IOSRulesServiceTest, DestinationURL) {
  {
    // Restriction level is set as "BLOCK".
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                              "name": "block",
                              "rule_id": "1234",
                              "destinations": {
                                "urls": ["google.com"]
                              },
                              "restrictions": [
                                {"class": "CLIPBOARD", "level": "BLOCK"}
                              ]
                            })"});
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(google_url(), GURL(),
                                          managed_profile1_, nullptr));
    ExpectBlockVerdict(IOSRulesServiceFactory::GetInstance()
                           ->GetForProfile(managed_profile1_)
                           ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                             managed_profile1_));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }

  {
    // Restriction level is set as "WARN".
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                              "name": "warn",
                              "rule_id": "1234",
                              "destinations": {
                                "urls": ["google.com"]
                              },
                              "restrictions": [
                                {"class": "CLIPBOARD", "level": "WARN"}
                              ]
                            })"});
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(google_url(), GURL(),
                                          managed_profile1_, nullptr));
    ExpectWarnVerdict(IOSRulesServiceFactory::GetInstance()
                          ->GetForProfile(managed_profile1_)
                          ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                            managed_profile1_));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                             "name": "allow",
                             "rule_id": "1234",
                             "destinations": {
                               "urls": ["google.com"]
                             },
                             "restrictions": [
                               {"class": "CLIPBOARD", "level": "ALLOW"}
                             ]
                           })",
                                                    R"({
                             "name": "warn",
                             "rule_id": "5678",
                             "destinations": {
                               "urls": ["google.com"]
                             },
                             "restrictions": [
                               {"class": "CLIPBOARD", "level": "WARN"}
                             ]
                           })"});
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(google_url(), GURL(),
                                          managed_profile1_, nullptr));
    ExpectAllowVerdict(IOSRulesServiceFactory::GetInstance()
                           ->GetForProfile(managed_profile1_)
                           ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                             managed_profile1_));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }
}

TEST_F(IOSRulesServiceTest, SourceIncognito) {
  {
    // Restriction level is set as `BLOCK`.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                                  "name": "block",
                                  "rule_id": "1234",
                                  "sources": {
                                    "incognito": true
                                  },
                                  "restrictions": [
                                    {"class": "CLIPBOARD", "level": "BLOCK"}
                                  ]
                                })"});
    ExpectBlockVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetPasteVerdict(google_url(), GURL(),
                              managed_profile1_->GetOffTheRecordProfile(),
                              nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                          managed_profile1_));
    ExpectBlockVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectBlockVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }

  {
    // Restriction level is set as `WARN`.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                                    "name": "warn",
                                    "rule_id": "1234",
                                    "sources": {
                                      "incognito": true
                                    },
                                    "restrictions": [
                                      {"class": "CLIPBOARD", "level": "WARN"}
                                    ]
                                  })"});
    ExpectWarnVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetPasteVerdict(google_url(), GURL(),
                              managed_profile1_->GetOffTheRecordProfile(),
                              nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                          managed_profile1_));
    ExpectWarnVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectWarnVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                              "name": "allow",
                              "rule_id": "1234",
                              "sources": {
                                "incognito": true
                              },
                              "restrictions": [
                                {"class": "CLIPBOARD", "level": "ALLOW"}
                              ]
                            })",
                                                    R"({
                              "name": "warn",
                              "rule_id": "5678",
                              "sources": {
                                "incognito": true
                              },
                              "restrictions": [
                                {"class": "CLIPBOARD", "level": "WARN"}
                              ]
                            })"});
    ExpectAllowVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetPasteVerdict(google_url(), GURL(),
                              managed_profile1_->GetOffTheRecordProfile(),
                              nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                          managed_profile1_));
    ExpectAllowVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectAllowVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }
}

TEST_F(IOSRulesServiceTest, DestinationIncognito) {
  {
    // Restriction level is set as `BLOCK`.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                                      "name": "block",
                                      "rule_id": "1234",
                                      "destinations": {
                                        "incognito": true
                                      },
                                      "restrictions": [
                                        {"class": "CLIPBOARD", "level": "BLOCK"}
                                      ]
                                    })"});
    ExpectBlockVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetPasteVerdict(GURL(), google_url(), nullptr,
                              managed_profile1_->GetOffTheRecordProfile()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetPasteVerdict(google_url(), GURL(),
                              managed_profile1_->GetOffTheRecordProfile(),
                              nullptr));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }

  {
    // Restriction level is set as `WARN`.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                                        "name": "warn",
                                        "rule_id": "1234",
                                        "destinations": {
                                          "incognito": true
                                        },
                                        "restrictions": [
                                          {"class": "CLIPBOARD", "level": "WARN"}
                                        ]
                                      })"});
    ExpectWarnVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetPasteVerdict(GURL(), google_url(), nullptr,
                              managed_profile1_->GetOffTheRecordProfile()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetPasteVerdict(google_url(), GURL(),
                              managed_profile1_->GetOffTheRecordProfile(),
                              nullptr));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                                  "name": "allow",
                                  "rule_id": "1234",
                                  "destinations": {
                                    "incognito": true
                                  },
                                  "restrictions": [
                                    {"class": "CLIPBOARD", "level": "ALLOW"}
                                  ]
                                })",
                                                    R"({
                                  "name": "warn",
                                  "rule_id": "5678",
                                  "destinations": {
                                    "incognito": true
                                  },
                                  "restrictions": [
                                    {"class": "CLIPBOARD", "level": "WARN"}
                                  ]
                                })"});
    ExpectAllowVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetPasteVerdict(GURL(), google_url(), nullptr,
                              managed_profile1_->GetOffTheRecordProfile()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_)
            ->GetPasteVerdict(google_url(), GURL(),
                              managed_profile1_->GetOffTheRecordProfile(),
                              nullptr));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile1_->GetOffTheRecordProfile())
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }
}

TEST_F(IOSRulesServiceTest, SourceOtherProfile) {
  {
    // Restriction level is set as `BLOCK`.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                            "name": "block",
                            "rule_id": "1234",
                            "sources": {
                              "other_profile": true
                            },
                            "restrictions": [
                              {"class": "CLIPBOARD", "level": "BLOCK"}
                            ]
                          })"});

    ExpectBlockVerdict(IOSRulesServiceFactory::GetInstance()
                           ->GetForProfile(managed_profile1_)
                           ->GetPasteVerdict(google_url(), GURL(),
                                             managed_profile2_, nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                          managed_profile2_));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile2_)
                        ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile2_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }

  {
    // Restriction level is set as `WARN`.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                               "name": "warn",
                               "rule_id": "1234",
                               "sources": {
                                 "other_profile": true
                               },
                               "restrictions": [
                                 {"class": "CLIPBOARD", "level": "WARN"}
                               ]
                             })"});

    ExpectWarnVerdict(IOSRulesServiceFactory::GetInstance()
                          ->GetForProfile(managed_profile1_)
                          ->GetPasteVerdict(google_url(), GURL(),
                                            managed_profile2_, nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                          managed_profile2_));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile2_)
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile2_)
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                                     "name": "allow",
                                     "rule_id": "1234",
                                     "sources": {
                                       "other_profile": true
                                     },
                                     "restrictions": [
                                       {"class": "CLIPBOARD", "level": "ALLOW"}
                                     ]
                                   })",
                                                    R"({
                                     "name": "warn",
                                     "rule_id": "5678",
                                     "sources": {
                                       "other_profile": true
                                     },
                                     "restrictions": [
                                       {"class": "CLIPBOARD", "level": "WARN"}
                                     ]
                                   })"});
    ExpectAllowVerdict(IOSRulesServiceFactory::GetInstance()
                           ->GetForProfile(managed_profile1_)
                           ->GetPasteVerdict(google_url(), GURL(),
                                             managed_profile2_, nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                          managed_profile2_));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile2_)
                        ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile2_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }
}

TEST_F(IOSRulesServiceTest, DestinationOtherProfile) {
  {
    // Restriction level is set as `BLOCK`.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                            "name": "block",
                            "rule_id": "1234",
                            "destinations": {
                              "other_profile": true
                            },
                            "restrictions": [
                              {"class": "CLIPBOARD", "level": "BLOCK"}
                            ]
                          })"});

    ExpectBlockVerdict(IOSRulesServiceFactory::GetInstance()
                           ->GetForProfile(managed_profile1_)
                           ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                             managed_profile2_));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(google_url(), GURL(),
                                          managed_profile2_, nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile2_)
                        ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile2_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }

  {
    // Restriction level is set as `WARN`.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                               "name": "warn",
                               "rule_id": "1234",
                               "destinations": {
                                 "other_profile": true
                               },
                               "restrictions": [
                                 {"class": "CLIPBOARD", "level": "WARN"}
                               ]
                             })"});

    ExpectWarnVerdict(IOSRulesServiceFactory::GetInstance()
                          ->GetForProfile(managed_profile1_)
                          ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                            managed_profile2_));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(google_url(), GURL(),
                                          managed_profile2_, nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile2_)
                        ->GetCopyToOSClipboardVerdict(
                            /*source*/ google_url()));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile2_)
                        ->GetCopyRestrictedBySourceVerdict(
                            /*source*/ google_url()));
  }

  {
    // When multiple rules are triggered, "ALLOW" should have precedence over
    // any other value.
    SetDataControls(managed_profile1_->GetPrefs(), {R"({
                                     "name": "allow",
                                     "rule_id": "1234",
                                     "destinations": {
                                       "other_profile": true
                                     },
                                     "restrictions": [
                                       {"class": "CLIPBOARD", "level": "ALLOW"}
                                     ]
                                   })",
                                                    R"({
                                     "name": "warn",
                                     "rule_id": "5678",
                                     "destinations": {
                                       "other_profile": true
                                     },
                                     "restrictions": [
                                       {"class": "CLIPBOARD", "level": "WARN"}
                                     ]
                                   })"});
    ExpectAllowVerdict(IOSRulesServiceFactory::GetInstance()
                           ->GetForProfile(managed_profile1_)
                           ->GetPasteVerdict(GURL(), google_url(), nullptr,
                                             managed_profile2_));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile1_)
                        ->GetPasteVerdict(google_url(), GURL(),
                                          managed_profile2_, nullptr));
    ExpectNoVerdict(IOSRulesServiceFactory::GetInstance()
                        ->GetForProfile(managed_profile2_)
                        ->GetCopyToOSClipboardVerdict(/*source*/ google_url()));
    ExpectNoVerdict(
        IOSRulesServiceFactory::GetInstance()
            ->GetForProfile(managed_profile2_)
            ->GetCopyRestrictedBySourceVerdict(/*source*/ google_url()));
  }
}

}  // namespace data_controls
