// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/command_line.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller_unittest.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

class SearchEngineTableViewControllerEEATest
    : public SearchEngineTableViewControllerTest {
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");
    SearchEngineTableViewControllerTest::SetUp();
  }
};

// Tests that the table's first section subtitle is correctly set.
TEST_F(SearchEngineTableViewControllerEEATest, TestSectionSubtitle) {
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001, true);

  CreateController();
  CheckController();

  TableViewHeaderFooterItem* header =
      [[controller() tableViewModel] headerForSectionIndex:0];
  ASSERT_TRUE([header respondsToSelector:@selector(subtitle)]);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_SEARCH_ENGINE_CHOICE_SETTINGS_SUBTITLE),
      [(id)header subtitle]);
}

// Tests that items are displayed correctly when a prepopulated search engine is
// selected as default.
TEST_F(SearchEngineTableViewControllerEEATest,
       TestUrlsLoadedWithPrepopulatedSearchEngineAsDefault) {
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001,
                       /*set_default=*/true);
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002,
                       /*set_default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[2], 1003,
                       /*set_default=*/false);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*set_default=*/false);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Hours(10),
                        /*set_default=*/false);

  CreateController();
  CheckController();

  // There are 3 required prepopulated search engines, and 2 custom ones.
  ASSERT_EQ(2, NumberOfSections());
  ASSERT_EQ(3, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/true,
                        /*section=*/0,
                        /*row=*/0, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/1, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[2], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/2, /*enabled=*/true);
  ASSERT_EQ(2, NumberOfItemsInSection(1));
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/1,
                  /*row=*/0);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/1);

  // Select another default engine by user interaction.
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:2 inSection:0]];
  SInt64 written_pref = pref_service_->GetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp);
  // We don't care about the specific value, we just need to check that
  // something was written.
  ASSERT_FALSE(written_pref == 0);
  // Select another default engine by user interaction.
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:2 inSection:0]];
  // This time make sure that the pref was not re-written.
  ASSERT_EQ(written_pref,
            pref_service_->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
}

// Tests that items are displayed correctly when a custom search engine is
// selected as default.
TEST_F(SearchEngineTableViewControllerEEATest,
       TestChoiceScreenUrlsLoadedWithCustomSearchEngineAsDefault) {
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001,
                       /*set_default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002,
                       /*set_default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[2], 1003,
                       /*set_default=*/false);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*set_default=*/true);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Hours(10),
                        /*set_default=*/false);

  CreateController();
  CheckController();

  ASSERT_EQ(2, NumberOfSections());
  // There are 3 required prepopulated search egines plus the default custom
  // one, which should be the first in the list.
  ASSERT_EQ(4, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/0, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/1, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[2], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/2, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[0], /*checked=*/true, /*section=*/0,
                  /*row=*/3, /*enabled=*/true);
  ASSERT_EQ(1, NumberOfItemsInSection(1));
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);

  // Select another default engine by user interaction.
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:1]];
  // We don't care about the value, we just need to check that something was
  // written.
  ASSERT_FALSE(
      pref_service_->GetInt64(
          prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp) == 0);
}

// Tests that prepopulated engines are correctly displayed when enabling and
// disabling edit mode.
// The scenario:
// + Add P#0, P#1, P#2 as prepopulated search engines.
// + Add custom search engine C#1 as selected.
// + Add custom search engine C#0.
// + Test the custom search engine #1 is selected in section 0, at the bottom.
// + Test the add button being enabled.
// + Select P#0 from the first section as the default search engine.
// + Test C#1 has been removed from the first section.
// + Start edit mode.
// + Test the 3 prepopulated search engine in the first section (disabled).
// + Test the 2 custom search engine in the second section (enabled).
// + Stop edit mode.
// + Test the 3 prepopulated search engine in the first section (enabled).
// + Test the 2 custom search engine in the second section (enabled).
TEST_F(SearchEngineTableViewControllerEEATest, EditingMode) {
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001,
                       /*set_default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002,
                       /*set_default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[2], 1003,
                       /*set_default=*/false);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Minutes(10),
                        /*set_default=*/true);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*set_default=*/false);

  CreateController();
  CheckController();

  // Test that C#1 is at the last position of the first section.
  CheckCustomItem(custom_search_engine_[1], /*checked=*/true, /*section=*/0,
                  /*row=*/3, /*enabled=*/true);

  SearchEngineTableViewController* searchEngineController =
      static_cast<SearchEngineTableViewController*>(controller());
  EXPECT_TRUE([searchEngineController editButtonEnabled]);

  // In the background, set the first prepopulated engine as default engine
  // using `template_url_service_`. This will reload the table and move C#1 to
  // the second list.
  std::vector<raw_ptr<TemplateURL, VectorExperimental>> template_urls =
      template_url_service_->GetTemplateURLs();
  // The first engine in the list is P#0.
  template_url_service_->SetUserSelectedDefaultSearchProvider(
      template_urls[0].get());
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/true,
                        /*section=*/0,
                        /*row=*/0, /*enabled=*/true);

  // Enable editing mode
  [searchEngineController setEditing:YES animated:NO];
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/0, /*enabled=*/false);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/1, /*enabled=*/false);
  CheckPrepopulatedItem(prepopulated_search_engine_[2], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/2, /*enabled=*/false);
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/1, /*enabled=*/true);

  // Disable editing mode
  [searchEngineController setEditing:NO animated:NO];
  // Prepopulated engines should be re-enabled and the checkmark should be back.
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/true,
                        /*section=*/0,
                        /*row=*/0, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/1, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[2], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/2, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/1, /*enabled=*/true);
}

// Tests that custom search engines can be deleted, except it if is selected as
// the default search engine.
TEST_F(SearchEngineTableViewControllerEEATest, DeleteItems) {
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001,
                       /*set_default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002,
                       /*set_default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[2], 1003,
                       /*set_default=*/false);
  const int number_of_prepopulated_items = 3;
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*set_default=*/false);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Minutes(10),
                        /*set_default=*/false);
  AddCustomSearchEngine(custom_search_engine_[2],
                        base::Time::Now() - base::Hours(10),
                        /*set_default=*/true);
  AddCustomSearchEngine(custom_search_engine_[3],
                        base::Time::Now() - base::Days(1),
                        /*set_default=*/false);

  CreateController();
  CheckController();

  ASSERT_EQ(2, NumberOfSections());
  ASSERT_EQ(number_of_prepopulated_items + 1, NumberOfItemsInSection(0));
  ASSERT_EQ(3, NumberOfItemsInSection(1));

  // Remove C1 from second list.
  ASSERT_TRUE(
      DeleteItemsAndWait(@[ [NSIndexPath indexPathForRow:0 inSection:1] ], ^{
        // the first section should still contain the selected custom search
        // engine.
        return NumberOfItemsInSection(0) == number_of_prepopulated_items + 1;
      }));
  ASSERT_TRUE(NumberOfItemsInSection(1) == 2);

  // Select prepopulated search engine as default engine by user interaction.
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:0]];
  // We don't care about the value, we just need to check that something was
  // written.
  ASSERT_NE(0,
            pref_service_->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));

  ASSERT_EQ(number_of_prepopulated_items, NumberOfItemsInSection(0));
  ASSERT_EQ(3, NumberOfItemsInSection(1));

  // Remove all custom search engines.
  ASSERT_TRUE(DeleteItemsAndWait(
      @[
        [NSIndexPath indexPathForRow:0 inSection:1],
        [NSIndexPath indexPathForRow:1 inSection:1],
        [NSIndexPath indexPathForRow:2 inSection:1]
      ],
      ^{
        return NumberOfSections() == 1;
      }));
  ASSERT_TRUE(NumberOfItemsInSection(0) == number_of_prepopulated_items);
}

// Tests all prepopulated items and the selected custom search engine are
// disabled when the table view is in edit mode.
// Tests that all custom search engines not selected are enabled when the table
// view is in edit mode.
// The scenario:
// + Add P#0 and P#1 search engines.
// + Add custom search engine C#0 and C#1 with C#0 selected.
// + Test all items in section 0 (P#0, P#1 and C#0).
// + Test the C#1 in section 1.
// + Start edit mode.
// + Test all items disabled in section 0.
// + Test the C#1 enabled in section 1.
TEST_F(SearchEngineTableViewControllerEEATest,
       EditModeWithCustomSearchEngineAsDefault) {
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001,
                       /*set_default=*/false);
  AddPriorSearchEngine(prepopulated_search_engine_[1], 1002,
                       /*set_default=*/false);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*default=*/true);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Minutes(10),
                        /*default=*/false);
  CreateController();
  CheckController();
  ASSERT_EQ(3, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/0, /*enabled=*/true);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/1, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[0], /*checked=*/true, /*section=*/0,
                  /*row=*/2, /*enabled=*/true);
  ASSERT_EQ(1, NumberOfItemsInSection(1));
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  // Start edit mode.
  [controller() setEditing:YES animated:NO];
  CheckPrepopulatedItem(prepopulated_search_engine_[0], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/0, /*enabled=*/false);
  CheckPrepopulatedItem(prepopulated_search_engine_[1], /*checked=*/false,
                        /*section=*/0,
                        /*row=*/1, /*enabled=*/false);
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/0,
                  /*row=*/2, /*enabled=*/false);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
}

// Test the edit button is disabled when having no custom search engine.
TEST_F(SearchEngineTableViewControllerEEATest,
       EditButtonWithNoCustomSearchEngine) {
  CreateController();
  CheckController();
  SearchEngineTableViewController* searchEngineController =
      static_cast<SearchEngineTableViewController*>(controller());
  EXPECT_FALSE([searchEngineController editButtonEnabled]);
}

// Test the edit button is enabled when having one custom search engine.
TEST_F(SearchEngineTableViewControllerEEATest,
       EditButtonWithOneCustomSearchEngine) {
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*default=*/false);
  CreateController();
  CheckController();
  SearchEngineTableViewController* searchEngineController =
      static_cast<SearchEngineTableViewController*>(controller());
  EXPECT_TRUE([searchEngineController editButtonEnabled]);
}

// Test the edit button is disabled when having only one custom search engine,
// and it is selected.
TEST_F(SearchEngineTableViewControllerEEATest,
       EditButtonWithSelectedCustomSearchEngine) {
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
