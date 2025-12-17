// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/test/page_context_app_interface.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/ai_prototyping/utils/page_context_util.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace {
NSString* kErrorDomain = @"PageContextAppInterfaceError";
NSInteger kPageContextWrapperErrorCode = 0;
NSInteger kPageContextLocalStorageErrorCode = 1;

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
// Configuration for the current capture.
@property(nonatomic, strong) PageContextExtractionConfig* config;
// Whether the page context capture has completed.
@property(nonatomic, assign) BOOL pageContextCaptureComplete;
// The page context capture result.
@property(nonatomic, strong) PageContextExtractionResult* pageContextResult;
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

+ (void)triggerPageContextCaptureWithConfig:
    (PageContextExtractionConfig*)config {
  [[PageContextAppInterface sharedInstance]
      captureAnnotatedPageContextWithConfig:config];
}

+ (BOOL)isPageContextCaptureComplete {
  return [[PageContextAppInterface sharedInstance] pageContextCaptureComplete];
}

+ (PageContextExtractionResult*)pageContextResult {
  return [[PageContextAppInterface sharedInstance] pageContextResult];
}

#pragma mark - Helper

- (void)captureAnnotatedPageContextWithConfig:
    (PageContextExtractionConfig*)config {
  [self reset];
  [self setConfig:config];
  // Configure the callback to be executed once the page context is ready.
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    base::OnceCallback<void(PageContextWrapperCallbackResponse)>
        page_context_completion_callback =
            base::BindOnce(^(PageContextWrapperCallbackResponse response) {
              [weakSelf OnPageContextWrapperCallback:std::move(response)];
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
  self.config = nil;
}

- (void)pageContextResultCompleted {
  self->_pageContextWrapper = nil;
  self.pageContextCaptureComplete = YES;
}

- (void)OnPageContextWrapperCallback:
    (PageContextWrapperCallbackResponse)response {
  // Handle PageContextWrapper errors.
  if (!response.has_value()) {
    NSDictionary* userInfo = @{
      NSLocalizedDescriptionKey :
          StringFromPageContextWrapperError(response.error())
    };
    NSError* error = [NSError errorWithDomain:kErrorDomain
                                         code:kPageContextWrapperErrorCode
                                     userInfo:userInfo];
    self.pageContextResult =
        [[PageContextExtractionResult alloc] initWithPageContext:nil
                                                           error:error
                                                        filePath:nil];
    [self pageContextResultCompleted];
    return;
  }

  NSString* filePath = nil;
  NSError* pageContextStoreError = nil;
  // Optionally store page context to disk.
  if (self.config.shouldStorePageContextLocally) {
    SavePageContextResult result =
        SaveSerializedPageContextToDisk(*response.value());

    if (result.success) {
      filePath = base::SysUTF8ToNSString(result.file_path.value());
    } else {
      NSString* storeErrorString = [NSString
          stringWithFormat:@"Failed to save serialized page "
                           @"context to "
                           @"disk: %@",
                           base::SysUTF8ToNSString(result.error_message)];

      NSLog(@"[PageContextAppInterface] %@", storeErrorString);

      pageContextStoreError = [NSError
          errorWithDomain:kErrorDomain
                     code:kPageContextLocalStorageErrorCode
                 userInfo:@{NSLocalizedDescriptionKey : storeErrorString}];
    }
    CHECK(filePath != nil || pageContextStoreError != nil);
  }

  const std::string& serialized_string = response.value()->SerializeAsString();

  NSString* pageContext =
      [NSString stringWithCString:serialized_string.c_str()
                         encoding:[NSString defaultCStringEncoding]];

  self.pageContextResult = [[PageContextExtractionResult alloc]
      initWithPageContext:pageContext
                    error:pageContextStoreError
                 filePath:filePath];
  [self pageContextResultCompleted];
}

@end
