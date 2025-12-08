// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/test/page_context_app_interface.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "base/test/ios/wait_util.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/ai_prototyping/utils/page_context_util.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace {
// Helper function to convert PageContextWrapperError enum to a readable string.
NSString* StringFromPageContextWrapperError(PageContextWrapperError error) {
  switch (error) {
    case PageContextWrapperError::kGenericError:
      return @"Generic Error";
    case PageContextWrapperError::kAPCError:
      return @"Annotated Page Content (APC) Error";
    case PageContextWrapperError::kScreenshotError:
      return @"Screenshot Error";
    case PageContextWrapperError::kPDFDataError:
      return @"PDF Data Error";
    case PageContextWrapperError::kForceDetachError:
      return @"Force Detach Error (Webpage Protected)";
    case PageContextWrapperError::kTimeout:
      return @"Timeout Error";
    case PageContextWrapperError::kInnerTextError:
      return @"InnerText Error";
  }
}
}  // namespace

@interface PageContextAppInterface ()
// Whether the page context capture has completed.
@property(nonatomic, assign) BOOL pageContextCaptureComplete;
// The page context capture result.
@property(nonatomic, copy) NSString* pageContextResult;
@end

@implementation PageContextAppInterface {
  // Holds the PageContextWrapper instance.
  PageContextWrapper* _pageContextWrapper;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    [self reset];
  }
  return self;
}

+ (instancetype)sharedInstance {
  static PageContextAppInterface* instance = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[self alloc] init];
  });
  return instance;
}

+ (void)triggerPageContextCapture {
  [[PageContextAppInterface sharedInstance] captureAnnotatedPageContext];
}

+ (BOOL)isPageContextCaptureComplete {
  return [[PageContextAppInterface sharedInstance] pageContextCaptureComplete];
}

+ (NSString*)pageContextResult {
  return [[PageContextAppInterface sharedInstance] pageContextResult];
}

#pragma mark - Helper

- (void)captureAnnotatedPageContext {
  [self reset];

  dispatch_async(dispatch_get_main_queue(), ^{
    base::OnceCallback<void(PageContextWrapperCallbackResponse)>
        page_context_completion_callback =
            base::BindOnce(^(PageContextWrapperCallbackResponse response) {
              if (response.has_value()) {
                const std::string& serialized_string =
                    response.value()->SerializeAsString();
                // TODO(crbug.com/465016086): Store page context locally or
                // upload it based on the test args.
                self.pageContextResult = [NSString
                    stringWithCString:serialized_string.c_str()
                             encoding:[NSString defaultCStringEncoding]];
              } else {
                self.pageContextResult = [NSString
                    stringWithFormat:@"Error: %@",
                                     StringFromPageContextWrapperError(
                                         response.error())];
              }
              self->_pageContextWrapper = nil;
              self.pageContextCaptureComplete = YES;
            });

    self->_pageContextWrapper =
        CreatePageContextWrapper(chrome_test_util::GetCurrentBrowser()
                                     ->GetWebStateList()
                                     ->GetActiveWebState(),
                                 std::move(page_context_completion_callback));
    PopulatePageContextWithTimeout(self->_pageContextWrapper,
                                   chrome_test_util::GetCurrentBrowser()
                                       ->GetWebStateList()
                                       ->GetActiveWebState(),
                                   base::Seconds(30));
  });
}

- (void)reset {
  _pageContextWrapper = nil;
  self.pageContextCaptureComplete = NO;
  self.pageContextResult = nil;
}

@end
