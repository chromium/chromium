// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller_unittest.h"

#import "components/search_engines/search_engines_pref_names.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Unit tests for SearchEngineTableViewController when
// `kSearchEngineChoiceTrigger` is enabled.
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
       TestChoiceScreenUrlsLoadedWithPrepopulatedSearchEngineAsDefault) {
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*set_default=*/false);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Hours(10),
                        /*set_default=*/false);

  // GetTemplateURLForChoiceScreen checks for the default search engine. Since
  // default fallback is disabled for testing, we set a fake prepopulated one
  // then change it.
  AddPriorSearchEngine(prepopulated_search_engine_[0], 1001, true);
  std::vector<std::unique_ptr<TemplateURL>> prepopulated_engines =
      template_url_service_->GetTemplateURLsForChoiceScreen();
  // Set a real prepopulated engine as the default.
  TemplateURL* default_search_engine = prepopulated_engines[0].get();
  template_url_service_->SetUserSelectedDefaultSearchProvider(
      default_search_engine);

  CreateController();
  CheckController();

  ASSERT_EQ(2, NumberOfSections());
  // There are twelve required prepopulated search engines plus the previously
  // added default one.
  ASSERT_EQ(13, NumberOfItemsInSection(0));
  CheckRealItem(default_search_engine, /*checked=*/true, /*section=*/0,
                /*row=*/0);
  for (size_t i = 1; i < prepopulated_engines.size(); i++) {
    CheckRealItem(prepopulated_engines[i].get(), /*checked=*/false,
                  /*section=*/0, /*row=*/i);
  }
  ASSERT_EQ(2, NumberOfItemsInSection(1));
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/1,
                  /*row=*/0);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/1);

  // Select another default engine by user interaction.
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:0]];
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
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*set_default=*/true);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Hours(10),
                        /*set_default=*/false);

  std::vector<std::unique_ptr<TemplateURL>> prepopulated_engines =
      template_url_service_->GetTemplateURLsForChoiceScreen();

  CreateController();
  CheckController();

  ASSERT_EQ(2, NumberOfSections());
  // There are twelve required prepopulated search egines plus the default
  // custom one, which should be the first in the list.
  ASSERT_EQ(13, NumberOfItemsInSection(0));
  CheckCustomItem(custom_search_engine_[0], true, 0, 0);
  for (size_t i = 1; i < prepopulated_engines.size(); i++) {
    CheckRealItem(prepopulated_engines[i].get(), /*checked=*/false,
                  /*section=*/0, /*row=*/i);
  }
  ASSERT_EQ(1, NumberOfItemsInSection(1));
  CheckCustomItem(custom_search_engine_[1], false, 1, 0);

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
// + Add custom search engine C#1 as selected.
// + Add custom search engine C#0.
// + Test the custom search engine #1 is selected in section 0, at the top.
// + Test the add button being enabled.
// + Select the second search engine from the first section (so the first
//   prepopulated search engine).
// + Test the custom search engine #1 has been removed from the first section.
// + Start edit mode.
// + Test the 12 prepopulated search engine in the first section (disabled).
// + Test the 2 custom search engine in the second section (enabled).
// + Stop edit mode.
// + Test the 12 prepopulated search engine in the first section (enabled).
// + Test the 2 custom search engine in the second section (enabled).
TEST_F(SearchEngineTableViewControllerEEATest, EditingMode) {
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Minutes(10),
                        /*set_default=*/true);
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*set_default=*/false);

  CreateController();
  CheckController();

  // Test that the first search engine of the first section is the custom search
  // engine #1.
  CheckCustomItem(custom_search_engine_[1], /*checked=*/true, /*section=*/0,
                  /*row=*/0, /*enabled=*/true);

  SearchEngineTableViewController* searchEngineController =
      static_cast<SearchEngineTableViewController*>(controller());
  EXPECT_TRUE([searchEngineController editButtonEnabled]);

  // Set the first prepopulated engine as default engine using
  // `template_url_service_`. This will reload the table and move C2 to the
  // second list.
  std::vector<std::unique_ptr<TemplateURL>> urls_for_choice_screen =
      template_url_service_->GetTemplateURLsForChoiceScreen();
  // The first engine in the list is C2.
  template_url_service_->SetUserSelectedDefaultSearchProvider(
      urls_for_choice_screen[1].get());
  CheckRealItem(urls_for_choice_screen[1].get(), /*checked=*/true,
                /*section=*/0, /*row=*/0);

  // Enable editing mode
  [searchEngineController setEditing:YES animated:NO];
  // Items in the first list should be disabled with checkmark removed.
  for (size_t i = 1; i < urls_for_choice_screen.size(); i++) {
    CheckRealItem(urls_for_choice_screen[i].get(), /*checked=*/false,
                  /*section=*/0, /*row=*/i - 1, /*enabled=*/false);
  }
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/1,
                  /*row=*/0);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/1);

  // Disable editing mode
  [searchEngineController setEditing:NO animated:NO];
  // Prepopulated engines should be re-enabled and the checkmark should be back.
  CheckRealItem(urls_for_choice_screen[1].get(), /*checked=*/true,
                /*section=*/0, /*row=*/0);
  for (size_t i = 2; i < urls_for_choice_screen.size(); i++) {
    CheckRealItem(urls_for_choice_screen[i].get(), /*checked=*/false,
                  /*section=*/0, /*row=*/i - 1);
  }
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/1, /*enabled=*/true);
}

// Tests that custom search engines can be deleted, except it if is selected as
// the default search engine.
TEST_F(SearchEngineTableViewControllerEEATest, DeleteItems) {
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

  // This method returns the list of prepopulated items and the custom search
  // engine that was set as default.
  int number_of_prepopulated_items =
      static_cast<int>(
          template_url_service_->GetTemplateURLsForChoiceScreen().size()) -
      1;
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
// + Add custom search engine C#0 and C#1 with C#0 selected.
// + Test all items in section 0 (C#0 first, and the 12 prepopulated search
//   engines).
// + Test the C#1 in section 1.
// + Start edit mode.
// + Test all items disabled in section 0.
// + Test the C#1 disabled in section 1.
TEST_F(SearchEngineTableViewControllerEEATest,
       EditModeWithCustomSearchEngineAsDefault) {
  // Get all prepopulated search engines.
  std::vector<std::unique_ptr<TemplateURL>> urls_for_choice_screen =
      template_url_service_->GetTemplateURLsForChoiceScreen();
  AddCustomSearchEngine(custom_search_engine_[0],
                        base::Time::Now() - base::Seconds(10),
                        /*default=*/true);
  AddCustomSearchEngine(custom_search_engine_[1],
                        base::Time::Now() - base::Minutes(10),
                        /*default=*/false);
  CreateController();
  CheckController();
  ASSERT_EQ(13, NumberOfItemsInSection(0));
  ASSERT_EQ(1, NumberOfItemsInSection(1));
  CheckCustomItem(custom_search_engine_[0], /*checked=*/true, /*section=*/0,
                  /*row=*/0, /*enabled=*/true);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  // Start edit mode.
  [controller() setEditing:YES animated:NO];
  CheckCustomItem(custom_search_engine_[0], /*checked=*/false, /*section=*/0,
                  /*row=*/0, /*enabled=*/false);
  CheckCustomItem(custom_search_engine_[1], /*checked=*/false, /*section=*/1,
                  /*row=*/0, /*enabled=*/true);
  for (size_t i = 0; i < urls_for_choice_screen.size(); i++) {
    // Add +1 to the row, to skip C#0.
    CheckRealItem(urls_for_choice_screen[i].get(), /*checked=*/false,
                  /*section=*/0, /*row=*/i + 1, /*enabled=*/false);
  }
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
