// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/task_environment.h"
#import "build/branding_buildflags.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_data.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/favicon/model/mock_favicon_loader.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/test/ios/ui_image_test_utils.h"
#import "ui/gfx/image/image.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;
using ::ui::test::uiimage_utils::UIImagesAreEqual;

namespace {

// Whether the image is the placeholder image used in the `placeholder_service`.
bool IsPlaceholderImage(UIImage* image, CGFloat image_point_size) {
  UIImage* placeholder_image =
      DefaultSymbolWithPointSize(kSearchSymbol, image_point_size);
  return UIImagesAreEqual(placeholder_image, image);
}

// Test fixture for PlaceholderService.
class PlaceholderServiceTest : public PlatformTest {
 protected:
  PlaceholderServiceTest()
      : search_engines_test_environment_(),
        mock_favicon_loader_(
            std::make_unique<::testing::StrictMock<MockFaviconLoader>>()),
        placeholder_service_(mock_favicon_loader_.get(),
                             &template_url_service()) {
    // Set up a default search provider.
    TemplateURLData data;
    data.SetShortName(u"TestEngine");
    data.SetKeyword(u"test");
    data.SetURL("http://test.com/search?q={searchTerms}");
    data.favicon_url = GURL("http://test.com/favicon.ico");
    default_search_provider_ =
        template_url_service().Add(std::make_unique<TemplateURL>(data));
    template_url_service().SetUserSelectedDefaultSearchProvider(
        default_search_provider_);
    template_url_service().Load();
  }

  void TearDown() override {
    placeholder_service_.Shutdown();
    PlatformTest::TearDown();
  }

  TemplateURLService& template_url_service() {
    return *search_engines_test_environment_.template_url_service();
  }

  // Helper to create a UIImage from an SF Symbol for testing.
  UIImage* CreateTestSymbolImage(CGFloat size = 16.0) {
    return DefaultSymbolWithPointSize(kInfoCircleSymbol, size);
  }

  base::test::TaskEnvironment task_environment_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<MockFaviconLoader> mock_favicon_loader_;
  PlaceholderService placeholder_service_;
  raw_ptr<TemplateURL> default_search_provider_;
};

// Test that a bundled icon is returned for Google.
TEST_F(PlaceholderServiceTest, TestFetchingBundledIcon) {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  const CGFloat icon_size = kDesiredSmallFaviconSizePt;
  TemplateURLData google_data;
  google_data.SetShortName(u"Google");
  google_data.SetKeyword(u"google.com");
  google_data.SetURL("https://www.google.com/search?q={searchTerms}");
  google_data.prepopulate_id = 1;  // Indicates Google
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(std::make_unique<TemplateURL>(google_data)));

  __block UIImage* received_icon_final = nil;
  __block int callback_count = 0;
  __block std::unique_ptr<base::RunLoop> run_loop =
      std::make_unique<base::RunLoop>();

  EXPECT_CALL(*mock_favicon_loader_, FaviconForPageUrl(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*mock_favicon_loader_, FaviconForIconUrl(_, _, _, _)).Times(0);

  placeholder_service_.FetchDefaultSearchEngineIcon(
      icon_size, base::BindRepeating(^(UIImage* icon) {
        callback_count++;
        if (!IsPlaceholderImage(icon, icon_size)) {
          received_icon_final = icon;
          run_loop->Quit();
        }
        run_loop->QuitWhenIdle();
      }));
  run_loop->Run();

  ASSERT_NE(received_icon_final, nil);
  UIImage* expected_bundled_icon = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleIconSymbol, icon_size));
  EXPECT_TRUE(UIImagesAreEqual(received_icon_final, expected_bundled_icon));
  // Callback is invoked once with bundled icon.
  EXPECT_EQ(callback_count, 1);
#endif
}

// Test fetching an icon using FaviconLoader for a non-prepopulated DSE.
TEST_F(PlaceholderServiceTest, TestFetchingIconFromFaviconLoader) {
  const CGFloat icon_size = kDesiredMediumFaviconSizePt;
  UIImage* fetched_image = CreateTestSymbolImage(icon_size);
  FaviconAttributes* fetched_attributes =
      [FaviconAttributes attributesWithImage:fetched_image];

  FaviconLoader::FaviconAttributesCompletionBlock favicon_callback_block;
  EXPECT_CALL(*mock_favicon_loader_,
              FaviconForIconUrl(GURL("http://test.com/favicon.ico"), icon_size,
                                icon_size, _))
      .WillOnce(SaveArg<3>(&favicon_callback_block));

  __block UIImage* received_icon_final = nil;
  __block int callback_count = 0;
  __block std::unique_ptr<base::RunLoop> run_loop =
      std::make_unique<base::RunLoop>();
  placeholder_service_.FetchDefaultSearchEngineIcon(
      icon_size, base::BindRepeating(^(UIImage* icon) {
        callback_count++;
        if (callback_count == 1) {
          EXPECT_TRUE(IsPlaceholderImage(icon, icon_size));
        } else if (callback_count == 2) {
          received_icon_final = icon;
          run_loop->Quit();
        }
        run_loop->QuitWhenIdle();
      }));

  EXPECT_EQ(callback_count, 1);
  ASSERT_TRUE(favicon_callback_block);

  favicon_callback_block(fetched_attributes, /*cached*/ false);
  run_loop->Run();

  EXPECT_EQ(received_icon_final, fetched_image);
  EXPECT_EQ(callback_count, 2);
}

// Test that callback is not called if DSE changes during fetch,
// but future calls for the new DSE work.
TEST_F(PlaceholderServiceTest, TestDSESwitchesDuringFetch) {
  const CGFloat icon_size = kDesiredSmallFaviconSizePt;

  // DSE1 is `default_search_provider_` with favicon
  // "http://test.com/favicon.ico"
  UIImage* dse1_fetched_image = CreateTestSymbolImage(icon_size);
  FaviconAttributes* dse1_fetched_attributes =
      [FaviconAttributes attributesWithImage:dse1_fetched_image];

  FaviconLoader::FaviconAttributesCompletionBlock dse1_favicon_callback_block;
  EXPECT_CALL(*mock_favicon_loader_,
              FaviconForIconUrl(GURL("http://test.com/favicon.ico"), icon_size,
                                icon_size, _))
      .WillOnce(SaveArg<3>(&dse1_favicon_callback_block));

  __block UIImage* received_icon_dse1_final = nil;
  __block int callback_dse1_count = 0;

  // 1. Initial fetch for DSE1
  placeholder_service_.FetchDefaultSearchEngineIcon(
      icon_size, base::BindRepeating(^(UIImage* icon) {
        callback_dse1_count++;
        if (!IsPlaceholderImage(icon, icon_size)) {
          received_icon_dse1_final = icon;
        }
      }));

  // Should have received placeholder for DSE1
  EXPECT_EQ(callback_dse1_count, 1);
  ASSERT_TRUE(dse1_favicon_callback_block);

  // 2. Set up DSE2
  TemplateURLData data_dse2;
  data_dse2.SetShortName(u"TestEngine2");
  data_dse2.SetKeyword(u"test2");
  data_dse2.SetURL("http://test2.com/search?q={searchTerms}");
  data_dse2.favicon_url = GURL("http://test2.com/favicon.ico");
  TemplateURL* dse2 =
      template_url_service().Add(std::make_unique<TemplateURL>(data_dse2));

  UIImage* dse2_fetched_image =
      CreateTestSymbolImage(icon_size);  // Can be same image for simplicity
  FaviconAttributes* dse2_fetched_attributes =
      [FaviconAttributes attributesWithImage:dse2_fetched_image];

  // 3. Change DSE to DSE2 *before* DSE1 icon fetch completes
  template_url_service().SetUserSelectedDefaultSearchProvider(dse2);

  // 4. Complete DSE1 icon fetch (which should now be ignored)
  dse1_favicon_callback_block(dse1_fetched_attributes, /*cached*/ true);

  // 5. Future call to fetch for new DSE (DSE2)
  FaviconLoader::FaviconAttributesCompletionBlock dse2_favicon_callback_block;
  EXPECT_CALL(*mock_favicon_loader_,
              FaviconForIconUrl(GURL("http://test2.com/favicon.ico"), icon_size,
                                icon_size, _))
      .WillOnce(SaveArg<3>(&dse2_favicon_callback_block));

  __block UIImage* received_icon_dse2_final = nil;
  __block int callback_dse2_count = 0;
  __block std::unique_ptr<base::RunLoop> run_loop =
      std::make_unique<base::RunLoop>();
  placeholder_service_.FetchDefaultSearchEngineIcon(
      icon_size, base::BindRepeating(^(UIImage* icon) {
        callback_dse2_count++;
        if (callback_dse2_count == 1) {
          EXPECT_TRUE(IsPlaceholderImage(icon, icon_size));
        } else if (callback_dse2_count == 2) {
          received_icon_dse2_final = icon;
          run_loop->Quit();
        }
        run_loop->QuitWhenIdle();
      }));

  // Should have received placeholder for DSE2
  EXPECT_EQ(callback_dse2_count, 1);
  ASSERT_TRUE(dse2_favicon_callback_block);

  // 6. Complete DSE2 icon fetch
  dse2_favicon_callback_block(dse2_fetched_attributes, /*cached*/ true);
  run_loop->Run();

  // Callback for DSE1 should NOT have been called with the fetched icon
  EXPECT_EQ(callback_dse1_count, 1);         // Still 1, only placeholder
  EXPECT_EQ(received_icon_dse1_final, nil);  // Final icon not received for DSE1

  // Callback for DSE2 should have been called with placeholder and then fetched
  // icon
  EXPECT_EQ(received_icon_dse2_final, dse2_fetched_image);
  EXPECT_EQ(callback_dse2_count, 2);
}

}  // namespace
