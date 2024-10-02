// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller_unittest.h"

#import "base/command_line.h"
#import "base/test/scoped_feature_list.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/search_engines/template_url_data_util.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using TemplateURLPrepopulateData::GetAllPrepopulatedEngines;
using TemplateURLPrepopulateData::PrepopulatedEngine;

namespace {

const char kUmaSelectDefaultSearchEngine[] =
    "Search.iOS.SelectDefaultSearchEngine";

// Unit tests for SearchEngineTableViewController when the choice screen feature
// is disabled (using `kDisableSearchEngineChoiceScreen`).
class SearchEngineTableViewControllerNonEEATest
    : public SearchEngineTableViewControllerTest {
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "US");
    SearchEngineTableViewControllerTest::SetUp();
  }
};

// Tests that no items are shown if TemplateURLService is empty.
TEST_F(SearchEngineTableViewControllerNonEEATest, TestNoUrl) {
  CreateController();
  CheckController();
  EXPECT_EQ(0, NumberOfSections());
}

// Tests that items are displayed correctly when TemplateURLService is filled
// and a prepopulated search engine is selected as default.
TEST_F(SearchEngineTableViewControllerNonEEATest,
       TestUrlsLoadedWithPrepopulatedSearchEngineAsDefault) {
  AddPriorSearchEngine(prepopulated_search_engine_[2], 1003, false);
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001, false);
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002, true);

  AddCustomSearchEngine(custom_search_engine_[3],
                        base::Time::Now() - base::Days(10), false);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10), false);
  AddCustomSearchEngine(custom_search_engine_[2],
                        base::Time::Now() - base::Hours(10), false);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Minutes(10), false);

  CreateController();
  CheckController();

  ASSERT_EQ(2, NumberOfSections());
  ASSERT_EQ(3, NumberOfItemsInSection(0));
  // Assert order of prepopulated hasn't changed.
  CheckPrepopulatedItem(prepopulated_search_engine_[2], false, 0, 0);
  CheckPrepopulatedItem(prepopulated_search_engine_[0], false, 0, 1);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], true, 0, 2);

  ASSERT_EQ(3, NumberOfItemsInSection(1));
  CheckCustomItem(custom_search_engine_[0], false, 1, 0);
  CheckCustomItem(custom_search_engine_[1], false, 1, 1);
  CheckCustomItem(custom_search_engine_[2], false, 1, 2);
}

// Tests that items are displayed correctly when TemplateURLService is filled
// and a custom search engine is selected as default.
TEST_F(SearchEngineTableViewControllerNonEEATest,
       TestUrlsLoadedWithCustomSearchEngineAsDefault) {
  AddPriorSearchEngine(prepopulated_search_engine_[2], 1003, false);
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001, false);
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002, false);

  AddCustomSearchEngine(custom_search_engine_[3],
                        base::Time::Now() - base::Days(10), false);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10), false);
  AddCustomSearchEngine(custom_search_engine_[2],
                        base::Time::Now() - base::Hours(10), false);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Minutes(10), true);

  CreateController();
  CheckController();

  ASSERT_EQ(2, NumberOfSections());
  ASSERT_EQ(4, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(prepopulated_search_engine_[2], false, 0, 0);
  CheckPrepopulatedItem(prepopulated_search_engine_[0], false, 0, 1);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], false, 0, 2);
  CheckCustomItem(custom_search_engine_[1], true, 0, 3);

  ASSERT_EQ(2, NumberOfItemsInSection(1));
  CheckCustomItem(custom_search_engine_[0], false, 1, 0);
  CheckCustomItem(custom_search_engine_[2], false, 1, 1);
}

// Tests that when TemplateURLService add or remove TemplateURLs, or update
// default search engine, the controller will update the displayed items.
TEST_F(SearchEngineTableViewControllerNonEEATest, TestUrlModifiedByService) {
  TemplateURL* url_p1 =
      AddPriorSearchEngine(prepopulated_search_engine_[0], 1001, true);

  CreateController();
  CheckController();

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(1, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(prepopulated_search_engine_[0], true, 0, 0);

  TemplateURL* url_p2 =
      AddPriorSearchEngine(prepopulated_search_engine_[1], 1002, false);

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(prepopulated_search_engine_[0], true, 0, 0);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], false, 0, 1);

  template_url_service_->SetUserSelectedDefaultSearchProvider(url_p2);

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(prepopulated_search_engine_[0], false, 0, 0);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], true, 0, 1);

  template_url_service_->SetUserSelectedDefaultSearchProvider(url_p1);
  template_url_service_->Remove(url_p2);

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(1, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(prepopulated_search_engine_[0], true, 0, 0);
}

// Tests that when user change default search engine, all items can be displayed
// correctly and the change can be synced to the prefs.
TEST_F(SearchEngineTableViewControllerNonEEATest, TestChangeProvider) {
  // This test also needs to test the UMA, so load some real prepopulated search
  // engines to ensure the SearchEngineType is logged correctly. Don't use any
  // literal symbol(e.g. "google" or "AOL") from
  // "components/search_engines/prepopulated_engines.h" since it's a generated
  // file.
  const auto prepopulated_engines = GetAllPrepopulatedEngines();
  ASSERT_LE(2UL, prepopulated_engines.size());

  TemplateURL* url_p1 =
      template_url_service_->Add(std::make_unique<TemplateURL>(
          *TemplateURLDataFromPrepopulatedEngine(*prepopulated_engines[0])));
  ASSERT_TRUE(url_p1);
  TemplateURL* url_p2 =
      template_url_service_->Add(std::make_unique<TemplateURL>(
          *TemplateURLDataFromPrepopulatedEngine(*prepopulated_engines[1])));
  ASSERT_TRUE(url_p2);

  // Also add some custom search engines.
  TemplateURL* url_c1 =
      AddCustomSearchEngine(custom_search_engine_[0], base::Time::Now(), false);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Seconds(10), false);

  CreateController();
  CheckController();

  // Choose url_p1 as default.
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];

  ASSERT_EQ(2, NumberOfSections());
  // Check first list.
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckRealItem(url_p1, true, 0, 0);
  CheckRealItem(url_p2, false, 0, 1);
  // Check second list.
  ASSERT_EQ(2, NumberOfItemsInSection(1));
  CheckCustomItem(custom_search_engine_[0], false, 1, 0);
  CheckCustomItem(custom_search_engine_[1], false, 1, 1);
  // Check default search engine.
  EXPECT_EQ(url_p1, template_url_service_->GetDefaultSearchProvider());
  // Check UMA.
  histogram_tester_.ExpectUniqueSample(
      kUmaSelectDefaultSearchEngine,
      url_p1->GetEngineType(template_url_service_->search_terms_data()), 1);

  // Choose url_p2 as default.
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:0]];

  ASSERT_EQ(2, NumberOfSections());
  // Check first list.
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckRealItem(url_p1, false, 0, 0);
  CheckRealItem(url_p2, true, 0, 1);
  // Check second list.
  ASSERT_EQ(2, NumberOfItemsInSection(1));
  CheckCustomItem(custom_search_engine_[0], false, 1, 0);
  CheckCustomItem(custom_search_engine_[1], false, 1, 1);
  // Check default search engine.
  EXPECT_EQ(url_p2, template_url_service_->GetDefaultSearchProvider());
  // Check UMA.
  histogram_tester_.ExpectBucketCount(
      kUmaSelectDefaultSearchEngine,
      url_p1->GetEngineType(template_url_service_->search_terms_data()), 1);
  histogram_tester_.ExpectBucketCount(
      kUmaSelectDefaultSearchEngine,
      url_p2->GetEngineType(template_url_service_->search_terms_data()), 1);
  histogram_tester_.ExpectTotalCount(kUmaSelectDefaultSearchEngine, 2);

  // Choose url_c1 as default.
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:1]];

  ASSERT_EQ(2, NumberOfSections());
  // The selected Custom search engine is moved to the first section.
  // Check first list.
  ASSERT_EQ(3, NumberOfItemsInSection(0));
  CheckRealItem(url_p1, false, 0, 0);
  CheckRealItem(url_p2, false, 0, 1);
  // Check second list.
  ASSERT_EQ(1, NumberOfItemsInSection(1));
  CheckCustomItem(custom_search_engine_[0], true, 0, 2);
  CheckCustomItem(custom_search_engine_[1], false, 1, 0);
  // Check default search engine.
  EXPECT_EQ(url_c1, template_url_service_->GetDefaultSearchProvider());
  // Check UMA.
  histogram_tester_.ExpectBucketCount(
      kUmaSelectDefaultSearchEngine,
      url_p1->GetEngineType(template_url_service_->search_terms_data()), 1);
  histogram_tester_.ExpectBucketCount(
      kUmaSelectDefaultSearchEngine,
      url_p2->GetEngineType(template_url_service_->search_terms_data()), 1);
  histogram_tester_.ExpectBucketCount(kUmaSelectDefaultSearchEngine,
                                      SEARCH_ENGINE_OTHER, 1);
  histogram_tester_.ExpectTotalCount(kUmaSelectDefaultSearchEngine, 3);

  // Check that the selection was written back to the prefs.
  const base::Value::Dict& searchProviderDict =
      profile_->GetTestingPrefService()->GetDict(
          DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  const std::string* short_name =
      searchProviderDict.FindString(DefaultSearchManager::kShortName);
  ASSERT_TRUE(short_name);
  EXPECT_EQ(url_c1->short_name(), base::ASCIIToUTF16(*short_name));
}

// Tests that prepopulated engines are disabled with checkmark removed in
// editing mode, and that toolbar is displayed as expected.
// The scenario:
// + Add prepopulated search engine: P#2, P#0, P#1 (selected).
// + Test the edit button is disabled.
// + Add custom search engine: C#1, C#0.
// + Test the edit button is enabled.
// + Test all search engine in section 0 (all enabled, and P#1 selected).
// + Enable edit mode.
// + Test all prepopulated search engine (all disabled and unselected).
// + Test all custom search engine (all enabled).
// + Select custom search engine C#1.
// + Test the edit button is enabled.
// + Test the tool bar is visible.
// + Unselect custom search engine C#1.
// + Stop edit mode.
// + Test all prepopulated search engine (all enabled and P#1 selected).
// + Test all custom search engine (all enabled).
TEST_F(SearchEngineTableViewControllerNonEEATest, EditingMode) {
  AddPriorSearchEngine(prepopulated_search_engine_[2], 1003,
                       /*default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001,
                       /*default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002,
                       /*default=*/true);

  SearchEngineTableViewController* searchEngineController =
      static_cast<SearchEngineTableViewController*>(controller());

  // Edit button should be disabled since there is no custom engine.
  EXPECT_FALSE([searchEngineController editButtonEnabled]);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Minutes(10),
                        /*checked=*/false);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*checked=*/false);

  EXPECT_TRUE([searchEngineController editButtonEnabled]);
  CheckPrepopulatedItem(prepopulated_search_engine_[2], /*checked=*/false,
                        /*section=*/0, /*row=*/0, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/false,
                        /*section=*/0, /*row=*/1, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/true,
                        /*section=*/0, /*row=*/2, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/1, /*enabled=*/true);
  [searchEngineController setEditing:YES animated:NO];

  // Prepopulated engines should be disabled with checkmark removed.
  CheckPrepopulatedItem(prepopulated_search_engine_[2], /*checked=*/false,
                        /*section=*/0, /*row=*/0, /*enabled=*/false);
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/false,
                        /*section=*/0, /*row=*/1, /*enabled=*/false);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/false,
                        /*section=*/0, /*row=*/2, /*enabled=*/false);
  // Custom engines should enabled.
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/1, /*enabled=*/true);

  // Select custom engine C1.
  [controller().tableView selectRowAtIndexPath:[NSIndexPath indexPathForRow:0
                                                                  inSection:1]
                                      animated:NO
                                scrollPosition:UITableViewScrollPositionNone];

  // Toolbar should be displayed.
  EXPECT_TRUE([searchEngineController editButtonEnabled]);
  EXPECT_FALSE([searchEngineController shouldHideToolbar]);

  // Deselect custom engine C1.
  [controller().tableView deselectRowAtIndexPath:[NSIndexPath indexPathForRow:0
                                                                    inSection:1]
                                        animated:NO];
  [searchEngineController setEditing:NO animated:NO];

  EXPECT_TRUE([searchEngineController editButtonEnabled]);

  CheckPrepopulatedItem(prepopulated_search_engine_[2], /*checked=*/false,
                        /*section=*/0, /*row=*/0, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/false,
                        /*section=*/0, /*row=*/1, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/true,
                        /*section=*/0, /*row=*/2, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/1, /*enabled=*/true);
}

// Tests to remove custom search engiens that are not selected:
// - Add C#0, C#1, C#2 and C#3 with C#2 selected.
// - Test that the second section contains C#0, C#1 and C#3
// - Remove C#0
// - Test that the second section contains C#1 and C#3
// - Remove C#1 and C#3.
// - Test that the second section doesn't exist.
TEST_F(SearchEngineTableViewControllerNonEEATest,
       DeleteItems_SearchEngineChoiceTriggerEnabled) {
  AddPriorSearchEngine(prepopulated_search_engine_[2], 1003,
                       /*set_default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001,
                       /*set_default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002,
                       /*set_default=*/false);

  AddCustomSearchEngine(custom_search_engine_[3],
                        base::Time::Now() - base::Days(1),
                        /*set_default=*/false);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*set_default=*/false);
  AddCustomSearchEngine(custom_search_engine_[2],
                        base::Time::Now() - base::Hours(10),
                        /*set_default=*/true);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Minutes(10),
                        /*set_default=*/false);

  CreateController();
  CheckController();

  ASSERT_EQ(2, NumberOfSections());
  ASSERT_EQ(4, NumberOfItemsInSection(0));
  ASSERT_EQ(3, NumberOfItemsInSection(1));

  // Remove C1 from second list.
  ASSERT_TRUE(DeleteItemsAndWait(
      @[
        [NSIndexPath indexPathForRow:0 inSection:1],
      ],
      ^{
        return NumberOfItemsInSection(1) == 2;
      }));
  ASSERT_EQ(4, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(prepopulated_search_engine_[2], /*checked=*/false,
                        /*section=*/0, /*row=*/0, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/false,
                        /*section=*/0, /*row=*/1, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/false,
                        /*section=*/0, /*row=*/2, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[2], /*checked=*/true, /*section=*/0,
                  /*row=*/3, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[3], /*checked=*/false, /*section=*/1,
                  /*row=*/1, /*enabled=*/true);

  // Remove all custom search engines that are not selected.
  ASSERT_TRUE(DeleteItemsAndWait(
      @[
        [NSIndexPath indexPathForRow:0 inSection:1],
        [NSIndexPath indexPathForRow:1 inSection:1],
      ],
      ^{
        return NumberOfSections() == 1;
      }));
  ASSERT_EQ(4, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(prepopulated_search_engine_[2], /*checked=*/false,
                        /*section=*/0, /*row=*/0, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/false,
                        /*section=*/0, /*row=*/1, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/false,
                        /*section=*/0, /*row=*/2, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[2], /*checked=*/true, /*section=*/0,
                  /*row=*/3, /*enabled=*/true);
}

// Tests all prepopulated items and the selected custom search engine are
// disabled when the table view is in edit mode.
// Tests that unselected custom search engines are enabled when the table view
// is in edit mode.
// The scenario:
// + Add prepopulated search engine P#0 and P#1.
// + Add custom search engine C#0 and C#1 with C#1 selected.
// + Test in section 0: P#0 and P#1 not selected, and C#1 selected
// + Test in section 1: C#0 not selected.
// + Start edit mode.
// + Test in section 0: P#0, P#1 and C#1 disabled.
// + Test in section 1: C#0 enabled.
TEST_F(
    SearchEngineTableViewControllerNonEEATest,
    EditModeWithCustomSearchEngineAsDefault_SearchEngineChoiceTriggerEnabled) {
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001,
                       /*default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002,
                       /*default=*/false);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*default=*/false);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Minutes(10),
                        /*default=*/true);
  CreateController();
  CheckController();
  ASSERT_EQ(3, NumberOfItemsInSection(0));
  ASSERT_EQ(1, NumberOfItemsInSection(1));
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/false,
                        /*section=*/0, /*row=*/0, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/false,
                        /*section=*/0, /*row=*/1, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/true, /*section=*/0,
                  /*row=*/2, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  // Start edit mode.
  [controller() setEditing:YES animated:NO];
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/false,
                        /*section=*/0, /*row=*/0, /*enabled=*/false);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/false,
                        /*section=*/0, /*row=*/1, /*enabled=*/false);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/0,
                  /*row=*/2, /*enabled=*/false);
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  // Select C4 as default engine by user interaction.
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:2 inSection:0]];
}

// Test the edit button is disabled when having no custom search engine.
TEST_F(SearchEngineTableViewControllerNonEEATest,
       EditButtonWithNoCustomSearchEngine) {
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002,
                       /*default=*/true);
  CreateController();
  CheckController();
  SearchEngineTableViewController* searchEngineController =
      static_cast<SearchEngineTableViewController*>(controller());
  EXPECT_FALSE([searchEngineController editButtonEnabled]);
}

// Test the edit button is enabled when having one custom search engine.
TEST_F(SearchEngineTableViewControllerNonEEATest,
       EditButtonWithOneCustomSearchEngine) {
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002,
                       /*default=*/true);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*default=*/false);
  CreateController();
  CheckController();
  SearchEngineTableViewController* searchEngineController =
      static_cast<SearchEngineTableViewController*>(controller());
  EXPECT_TRUE([searchEngineController editButtonEnabled]);
}

// Tests that when the only custom search engine is set as default, the edit
// button is disabled.
TEST_F(
    SearchEngineTableViewControllerNonEEATest,
    EditButtonWithSelectedCustomSearchEngine_SearchEngineChoiceTriggerEnabled) {
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002,
                       /*default=*/false);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*default=*/true);
  CreateController();
  CheckController();
  SearchEngineTableViewController* searchEngineController =
      static_cast<SearchEngineTableViewController*>(controller());
  EXPECT_FALSE([searchEngineController editButtonEnabled]);
}

}  // namespace
