// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"

#import <memory>

#import "base/barrier_closure.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/web_state.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

#import "components/optimization_guide/proto/features/common_quality_data.pb.h"

#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

@implementation PageContextWrapper {
  base::WeakPtr<web::WebState> _webState;

  // The amount of async tasks this specific instance of the PageContext wrapper
  // needs to complete before executing the `completionCallback`.
  NSInteger _asyncTasksToComplete;

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

  // The callback to execute once all async work is complete, whichs
  // relinquishes ownership of the PageContext proto to the callback's handler.
  base::OnceCallback<void(
      std::unique_ptr<optimization_guide::proto::PageContext>)>
      _completion_callback;

  // Unique pointer to the PageContext proto.
  std::unique_ptr<optimization_guide::proto::PageContext> _page_context;

#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
}

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

- (instancetype)
      initWithWebState:(web::WebState*)webState
    completionCallback:
        (base::OnceCallback<
            void(std::unique_ptr<optimization_guide::proto::PageContext>)>)
            completionCallback {
  self = [super init];
  if (self) {
    _asyncTasksToComplete = 0;
    _webState = webState->GetWeakPtr();
    _completion_callback = std::move(completionCallback);

    // Create the PageContext proto/object.
    _page_context = std::make_unique<optimization_guide::proto::PageContext>();
    _page_context->set_url(_webState->GetVisibleURL().spec());
    _page_context->set_title(base::UTF16ToUTF8(_webState->GetTitle()));
  }
  return self;
}

- (void)populatePageContextFieldsAsync {
  CHECK_GE(_asyncTasksToComplete, 0);

  if (_asyncTasksToComplete == 0) {
    [self asyncWorkCompletedForPageContext];
    return;
  }

  // Use a `BarrierClosure` to ensure all async tasks are completed before
  // executing the overall completion callback. The BarrierClosure will wait
  // until the `barrier` callback is itself run `_asyncTasksToComplete` times.
  __weak PageContextWrapper* weakSelf = self;
  base::RepeatingClosure barrier =
      base::BarrierClosure(_asyncTasksToComplete, base::BindOnce(^{
                             [weakSelf asyncWorkCompletedForPageContext];
                           }));

  // Asynchronous work. *IMPORTANT NOTES*:
  // When adding async tasks below, an accompanying setter should also be
  // created to follow the disabled-by-default pattern (which
  // increments/decrements `_asyncTasksToComplete` accordingly). Also, if a
  // given task is enabled, every code path for that task should eventually
  // execute the `barrier` callback, otherwise the `BarrierClosure` will never
  // execute its completion block.

  // Take WebState snapshot, if enabled.
  if (_shouldGetSnapshot) {
    if (_webState->CanTakeSnapshot()) {
      CGRect rect = [_webState->GetView() bounds];
      _webState->TakeSnapshot(rect, base::BindRepeating(^(UIImage* image) {
                                [weakSelf encodeImageAndSetTabScreenshot:image];
                                barrier.Run();
                              }));
    } else {
      barrier.Run();
    }
  }

  // Create WebState full page PDF, if enabled.
  if (_shouldGetFullPagePDF) {
    _webState->CreateFullPagePdf(base::BindOnce(^(NSData* PDFData) {
      [weakSelf encodeAndSetFullPagePDF:PDFData];
      barrier.Run();
    }));
  }
}

#pragma mark - Setters

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetSnapshot:(BOOL)enabled {
  if (_shouldGetSnapshot == enabled) {
    return;
  }

  _asyncTasksToComplete += enabled ? 1 : -1;
  _shouldGetSnapshot = enabled;
}

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetFullPagePDF:(BOOL)enabled {
  if (_shouldGetFullPagePDF == enabled) {
    return;
  }

  _asyncTasksToComplete += enabled ? 1 : -1;
  _shouldGetFullPagePDF = enabled;
}

#pragma mark - Private

// All async tasks are complete, execute the overall completion callback.
// Relinquish ownership to the callback handler.
- (void)asyncWorkCompletedForPageContext {
  std::move(_completion_callback).Run(std::move(_page_context));
}

// Convert UIImage snapshot to PNG, and then to base64 encoded string. Set the
// tab screenshot on the current PageContext.
- (void)encodeImageAndSetTabScreenshot:(UIImage*)image {
  NSData* imageData = UIImagePNGRepresentation(image);
  NSString* base64String = [imageData base64EncodedStringWithOptions:0];
  _page_context->set_tab_screenshot(base::SysNSStringToUTF8(base64String));
}

// If it exists, convert the PDF data to base64 encoded string and set it in the
// PageContext proto.
- (void)encodeAndSetFullPagePDF:(NSData*)PDFData {
  if (!PDFData) {
    return;
  }

  NSString* base64String = [PDFData base64EncodedStringWithOptions:0];
  _page_context->set_pdf_data(base::SysNSStringToUTF8(base64String));
}

#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

@end
