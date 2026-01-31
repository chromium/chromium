// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/test/page_context_app_interface.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "base/types/expected.h"
#import "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#import "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#import "components/optimization_guide/proto/features/bling_prototyping.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/ai_prototyping/utils/page_context_util.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/web_state.h"

namespace {
NSString* kErrorDomain = @"PageContextAppInterfaceError";
NSInteger kPageContextWrapperErrorCode = 0;
NSInteger kPageContextLocalStorageErrorCode = 1;
NSInteger kMQLSUploadErrorCode = 2;
NSUInteger kMaxUrlChars = 20;
base::TimeDelta kPageLoadTimeout = base::Seconds(30);

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
// The url of page context.
@property(nonatomic, strong) NSString* url;
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

+ (void)triggerPageContextCaptureWithConfig:(PageContextExtractionConfig*)config
                                        url:(NSString*)url {
  [[PageContextAppInterface sharedInstance]
      captureAnnotatedPageContextWithConfig:config
                                        url:url];
}

+ (BOOL)isPageContextCaptureComplete {
  return [[PageContextAppInterface sharedInstance] pageContextCaptureComplete];
}

+ (PageContextExtractionResult*)pageContextResult {
  return [[PageContextAppInterface sharedInstance] pageContextResult];
}

#pragma mark - Helper

- (void)captureAnnotatedPageContextWithConfig:
            (PageContextExtractionConfig*)config
                                          url:(NSString*)url {
  [self reset];
  [self setConfig:config];
  [self setUrl:url];
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
                                   kPageLoadTimeout);
  });
}

- (void)reset {
  _pageContextWrapper = nil;
  self.pageContextCaptureComplete = NO;
  self.pageContextResult = nil;
  self.config = nil;
  self.url = nil;
}

- (void)pageContextResultCompleted {
  self->_pageContextWrapper = nil;
  self.pageContextCaptureComplete = YES;
}

// Uploads the page context to MQLS. Returns an error if any occurs.
- (NSError*)uploadPageContextToMQLS:
    (const optimization_guide::proto::PageContext&)pageContext {
  web::WebState* web_state = chrome_test_util::GetCurrentBrowser()
                                 ->GetWebStateList()
                                 ->GetActiveWebState();
  if (!web_state) {
    return
        [NSError errorWithDomain:kErrorDomain
                            code:kMQLSUploadErrorCode
                        userInfo:@{
                          NSLocalizedDescriptionKey : @"WebState not available."
                        }];
  }

  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state->GetBrowserState()));
  if (!optimization_guide_service) {
    return [NSError errorWithDomain:kErrorDomain
                               code:kMQLSUploadErrorCode
                           userInfo:@{
                             NSLocalizedDescriptionKey :
                                 @"OptimizationGuideService not available."
                           }];
  }

  auto* mqls_service =
      optimization_guide_service->GetModelQualityLogsUploaderService();
  if (!mqls_service) {
    return [NSError errorWithDomain:kErrorDomain
                               code:kMQLSUploadErrorCode
                           userInfo:@{
                             NSLocalizedDescriptionKey :
                                 @"ModelQualityLogsUploaderService not "
                                 @"available."
                           }];
  }

  const optimization_guide::MqlsFeatureMetadata* metadata =
      optimization_guide::MqlsFeatureRegistry::GetInstance().GetFeature(
          optimization_guide::proto::LogAiDataRequest::FeatureCase::
              kBlingPrototyping);

  if (!metadata || !mqls_service->CanUploadLogs(metadata)) {
    return [NSError
        errorWithDomain:kErrorDomain
                   code:kMQLSUploadErrorCode
               userInfo:@{
                 NSLocalizedDescriptionKey : @"ModelQualityLogs upload is not "
                                             @"enabled for BlingPrototyping."
               }];
  }

  std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          mqls_service->GetWeakPtr());

  optimization_guide::proto::BlingPrototypingLoggingData proto_logging_data;
  *proto_logging_data.mutable_request()->mutable_page_context() = pageContext;

  if ([self.config.mqlsLoggingTag length] > 0) {
    proto_logging_data.mutable_metadata()->set_logging_tag(
        base::SysNSStringToUTF8(self.config.mqlsLoggingTag));
  }

  *log_entry->log_ai_data_request()->mutable_bling_prototyping() =
      proto_logging_data;
  optimization_guide::ModelQualityLogEntry::Upload(std::move(log_entry));
  return nil;
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
                                                    wrapperError:error
                                                      storeError:nil
                                                       mqlsError:nil
                                                        filePath:nil];
    [self pageContextResultCompleted];
    return;
  }

  NSString* filePath = nil;
  NSError* pageContextStoreError = nil;
  // Optionally store page context to disk.
  if (self.config.shouldStorePageContextLocally) {
    SavePageContextResult result;
    if (!self.config.outputDir) {
      result = SaveSerializedPageContextToDisk(*response.value());
    } else {
      NSString* sanitizedUrl = SanitizeUrl(self.url);
      // Prevent error from the file name being too long.
      if ([sanitizedUrl length] > kMaxUrlChars) {
        sanitizedUrl = [sanitizedUrl substringToIndex:kMaxUrlChars];
      }
      NSString* fileName = [sanitizedUrl stringByAppendingString:@".txtpb"];
      result = SaveSerializedPageContextToDisk(
          *response.value(), base::SysNSStringToUTF8(self.config.outputDir),
          base::SysNSStringToUTF8(fileName));
    }
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

  // Optionally upload page context to MQLS.
  NSError* mqls_upload_error = nil;
  if (self.config.shouldUploadToMQLS) {
    mqls_upload_error = [self uploadPageContextToMQLS:*response.value()];
  }

  const std::string& serialized_string = response.value()->SerializeAsString();

  NSString* pageContext =
      [NSString stringWithCString:serialized_string.c_str()
                         encoding:[NSString defaultCStringEncoding]];

  self.pageContextResult = [[PageContextExtractionResult alloc]
      initWithPageContext:pageContext
             wrapperError:nil
               storeError:pageContextStoreError
                mqlsError:mqls_upload_error
                 filePath:filePath];
  [self pageContextResultCompleted];
}

@end
