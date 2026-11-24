// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/multi_profile_passkey_creation_view_controller.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/apple/foundation_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/credential_provider_extension/favicon_util.h"
#import "ios/chrome/credential_provider_extension/generated_localized_strings.h"
#import "ios/chrome/credential_provider_extension/passkey_request_details+Testing.h"
#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class MultiProfilePasskeyCreationViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(favicon_folder_.CreateUniqueTempDir());
    SetFaviconFolderForTesting(
        base::apple::FilePathToNSURL(favicon_folder_.GetPath()));
  }

  void TearDown() override {
    SetFaviconFolderForTesting(nil);
    PlatformTest::TearDown();
  }

  // Creates a PasskeyWelcomeScreenViewController for the provided purpose.
  MultiProfilePasskeyCreationViewController* CreateController(
      NSString* favicon = nil) {
    PasskeyRequestDetails* details =
        [[PasskeyRequestDetails alloc] initWithURL:@"example.com"
                                          username:@"username"
                               excludedCredentials:nil];
    UIView* navigationView = [[UIView alloc] init];
    return [[MultiProfilePasskeyCreationViewController alloc]
                initWithDetails:details
                           gaia:nil
                      userEmail:@"peter.parker@gmail.com"
                        favicon:favicon
        navigationItemTitleView:navigationView
                       delegate:nil];
  }

  // Writes favicon attributes containing `image` to the favicon folder and
  // returns the file name to use as the credential's favicon key.
  NSString* WriteFaviconWithImage(UIImage* image) {
    NSString* key =
        @"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    NSURL* file_url = [base::apple::FilePathToNSURL(favicon_folder_.GetPath())
        URLByAppendingPathComponent:key
                        isDirectory:NO];
    NSData* data = [NSKeyedArchiver
        archivedDataWithRootObject:[FaviconAttributes attributesWithImage:image]
             requiringSecureCoding:YES
                             error:nil];
    if (![data writeToURL:file_url atomically:YES]) {
      return nil;
    }
    return key;
  }

 private:
  base::ScopedTempDir favicon_folder_;
};

namespace {

// Returns the first `FaviconView` found in `view`'s hierarchy. Not using
// `chrome_test_util::FindViewByClass` as its target depends on browser code
// that is not available in the extension.
FaviconView* FindFaviconView(UIView* view) {
  if ([view isKindOfClass:[FaviconView class]]) {
    return base::apple::ObjCCastStrict<FaviconView>(view);
  }
  for (UIView* subview in view.subviews) {
    if (FaviconView* favicon_view = FindFaviconView(subview)) {
      return favicon_view;
    }
  }
  return nil;
}

// Returns the image currently displayed by `favicon_view`, if any.
UIImage* DisplayedImage(FaviconView* favicon_view) {
  for (UIView* subview in favicon_view.subviews) {
    if ([subview isKindOfClass:[UIImageView class]] && !subview.hidden) {
      return base::apple::ObjCCastStrict<UIImageView>(subview).image;
    }
  }
  return nil;
}

}  // namespace

// Tests that the view's content is as expected.
TEST_F(MultiProfilePasskeyCreationViewControllerTest,
       TestDefaultCreationParameters) {
  MultiProfilePasskeyCreationViewController* controller = CreateController();
  [controller viewDidLoad];

  EXPECT_TRUE(controller.navigationItem.titleView);
  EXPECT_NSEQ(controller.bannerName, @"passkey_generic_banner");
  EXPECT_EQ(controller.bannerSize, BannerImageSizeType::kExtraShort);
  EXPECT_NSEQ(controller.titleText, CredentialProviderPasskeysCreateString());
  EXPECT_FALSE(controller.subtitleText);
  EXPECT_EQ(controller.specificContentView.subviews.count, 2u);
  EXPECT_NSEQ(controller.configuration.primaryActionString,
              CredentialProviderExtensionCreateString());
  EXPECT_NSEQ(controller.configuration.secondaryActionString,
              CredentialProviderExtensionCancelString());
  EXPECT_NSEQ(controller.view.backgroundColor,
              [UIColor colorNamed:kPrimaryBackgroundColor]);
}

// Tests that a favicon fetched after the view is loaded replaces the default
// world icon.
TEST_F(MultiProfilePasskeyCreationViewControllerTest,
       FaviconFetchedAfterViewDidLoadIsDisplayed) {
  const CGSize kFaviconSize = CGSizeMake(3, 5);
  // Favicons are archived as PNG data, which drops the image scale, so render
  // at scale 1 for the decoded size to match.
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = 1;
  UIImage* image = [[[UIGraphicsImageRenderer alloc] initWithSize:kFaviconSize
                                                           format:format]
      imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [UIColor.blackColor setFill];
        [context
            fillRect:CGRectMake(0, 0, kFaviconSize.width, kFaviconSize.height)];
      }];
  NSString* favicon = WriteFaviconWithImage(image);
  ASSERT_TRUE(favicon);

  MultiProfilePasskeyCreationViewController* controller =
      CreateController(favicon);
  // Load the view right away so that the asynchronous fetch completes after
  // `-viewDidLoad`.
  FaviconView* favicon_view = FindFaviconView(controller.view);
  ASSERT_TRUE(favicon_view);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^{
        return CGSizeEqualToSize(DisplayedImage(favicon_view).size,
                                 kFaviconSize);
      }));
}

// Tests that the default world icon is kept when the favicon file can't be
// read.
TEST_F(MultiProfilePasskeyCreationViewControllerTest,
       MissingFaviconKeepsDefaultIcon) {
  // A valid favicon file key (64 hex characters) not backed by a file.
  NSString* const kMissingFaviconKey =
      @"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
  MultiProfilePasskeyCreationViewController* controller =
      CreateController(kMissingFaviconKey);
  FaviconView* favicon_view = FindFaviconView(controller.view);
  ASSERT_TRUE(favicon_view);

  // Favicons are fetched on a serial queue and completions are invoked
  // asynchronously on the main queue, so once this second fetch's completion
  // runs, the controller's fetch has already been handled.
  __block bool done = false;
  FetchFaviconAsync(kMissingFaviconKey, ^(FaviconAttributes*) {
    done = true;
  });
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^{
        return done;
      }));

  EXPECT_TRUE(DisplayedImage(favicon_view));
}
