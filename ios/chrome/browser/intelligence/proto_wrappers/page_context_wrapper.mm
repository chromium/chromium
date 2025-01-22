// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/barrier_closure.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/logging.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

#import "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace {

// The JavaScript to be executed on each WebState's WebFrames, which retrieves
// the innerText.
const char16_t* kInnerTextJavaScript = u"document.body.innerText;";

}  // namespace


@implementation PageContextWrapper {
  base::WeakPtr<web::WebState> _webState;

  // The amount of async tasks this specific instance of the PageContext wrapper
  // needs to complete before executing the `completionCallback`.
  NSInteger _asyncTasksToComplete;

  // The accumulation of innerTexts from the all of the WebState's associated
  // WebFrames.
  NSMutableArray<NSString*>* _webFramesInnerTexts;

  // The callback to execute once all async work is complete, whichs
  // relinquishes ownership of the PageContext proto to the callback's handler.
  base::OnceCallback<void(
      std::unique_ptr<optimization_guide::proto::PageContext>)>
      _completion_callback;

  // Unique pointer to the PageContext proto.
  std::unique_ptr<optimization_guide::proto::PageContext> _page_context;
}
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
    _webFramesInnerTexts = [[NSMutableArray alloc] init];

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
  // until the `page_context_barrier` callback is itself run
  // `_asyncTasksToComplete` times.
  __weak PageContextWrapper* weakSelf = self;
  base::RepeatingClosure page_context_barrier =
      base::BarrierClosure(_asyncTasksToComplete, base::BindOnce(^{
                             [weakSelf asyncWorkCompletedForPageContext];
                           }));

  // Asynchronous work. *IMPORTANT NOTES*:
  // When adding async tasks below, an accompanying setter should also be
  // created to follow the disabled-by-default pattern (which
  // increments/decrements `_asyncTasksToComplete` accordingly). Also, if a
  // given task is enabled, every code path for that task should eventually
  // execute the `page_context_barrier` callback, otherwise the `BarrierClosure`
  // will never execute its completion block.

  // Retrieve WebState snapshot, if enabled.
  if (_shouldGetSnapshot) {
    auto callback = ^(UIImage* image) {
      if ([weakSelf shouldUpdateSnapshotWithImage:image]) {
        [weakSelf updateSnapshotWithBarrier:page_context_barrier];
        return;
      }

      [weakSelf encodeImageAndSetTabScreenshot:image];
      page_context_barrier.Run();
    };

    // If the WebState is currently visible, update the snapshot in case the
    // user was scrolling, otherwise retrieve the latest version in cache or on
    // disk.
    if (_webState->IsVisible()) {
      SnapshotTabHelper::FromWebState(_webState.get())
          ->UpdateSnapshotWithCallback(callback);
    } else {
      SnapshotTabHelper::FromWebState(_webState.get())
          ->RetrieveColorSnapshot(callback);
    }
  }

  // Create WebState full page PDF, if enabled.
  if (_shouldGetFullPagePDF) {
    _webState->CreateFullPagePdf(base::BindOnce(^(NSData* PDFData) {
      [weakSelf encodeAndSetFullPagePDF:PDFData];
      page_context_barrier.Run();
    }));
  }

  // Get the WebState's innerText, if enabled.
  if (_shouldGetInnerText) {
    std::set<web::WebFrame*> web_frames =
        _webState->GetPageWorldWebFramesManager()->GetAllWebFrames();
    web::WebFrame* main_frame =
        _webState->GetPageWorldWebFramesManager()->GetMainWebFrame();

    if (web_frames.empty() || !main_frame) {
      page_context_barrier.Run();
    } else {
      // Use a `BarrierClosure` to ensure the JavaScript is done executing in
      // all WebFrames before executing the `page_context_barrier` barrier,
      // which in turn signals to the PageContextWrapper that the innerText is
      // done being processed. The BarrierClosure will wait until the
      // `inner_text_barrier` callback is itself run once per WebFrame.
      base::RepeatingClosure inner_text_barrier =
          base::BarrierClosure(web_frames.size(), base::BindOnce(^{
                                 [weakSelf webFramesInnerTextsFetchCompleted];
                                 page_context_barrier.Run();
                               }));

      auto callback = ^(const base::Value* value, NSError* error) {
        [weakSelf parseAndConcatenateJavaScriptValue:value withError:error];
        inner_text_barrier.Run();
      };

      // Execute the JavaScript on each WebFrame and pass in the callback (which
      // executes the barrier when run).
      for (web::WebFrame* web_frame : web_frames) {
        // Skip WebFrames with different origins from the main WebFrame.
        if (!web_frame || (!web_frame->GetSecurityOrigin().IsSameOriginWith(
                              main_frame->GetSecurityOrigin()))) {
          inner_text_barrier.Run();
          continue;
        }

        web_frame->ExecuteJavaScript(kInnerTextJavaScript,
                                     base::BindOnce(callback));
      }
    }
  }
}

#pragma mark - Setters

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetSnapshot:(BOOL)shouldGetSnapshot {
  if (_shouldGetSnapshot == shouldGetSnapshot) {
    return;
  }

  _asyncTasksToComplete += shouldGetSnapshot ? 1 : -1;
  _shouldGetSnapshot = shouldGetSnapshot;
}

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetFullPagePDF:(BOOL)shouldGetFullPagePDF {
  if (_shouldGetFullPagePDF == shouldGetFullPagePDF) {
    return;
  }

  _asyncTasksToComplete += shouldGetFullPagePDF ? 1 : -1;
  _shouldGetFullPagePDF = shouldGetFullPagePDF;
}

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetInnerText:(BOOL)shouldGetInnerText {
  if (_shouldGetInnerText == shouldGetInnerText) {
    return;
  }

  _asyncTasksToComplete += shouldGetInnerText ? 1 : -1;
  _shouldGetInnerText = shouldGetInnerText;
}

#pragma mark - Private

// All async tasks are complete, execute the overall completion callback.
// Relinquish ownership to the callback handler.
- (void)asyncWorkCompletedForPageContext {
  std::move(_completion_callback).Run(std::move(_page_context));
}

// Returns YES if the image is nil and forcing the update of missing snapshots
// is enabled.
- (BOOL)shouldUpdateSnapshotWithImage:(UIImage*)image {
  return !image && _shouldForceUpdateMissingSnapshots;
}

// Updates the snapshot for the given WebState, and executes the `barrier`
// callback when finished.
- (void)updateSnapshotWithBarrier:(base::RepeatingClosure)barrier {
  SnapshotTabHelper::FromWebState(_webState.get())
      ->UpdateSnapshotWithCallback(^(UIImage* image) {
        [self encodeImageAndSetTabScreenshot:image];
        barrier.Run();
      });
}

// Convert UIImage snapshot to PNG, and then to base64 encoded string. Set the
// tab screenshot on the current PageContext.
- (void)encodeImageAndSetTabScreenshot:(UIImage*)image {
  if (!image) {
    DLOG(WARNING) << "Failed to fetch webpage screenshot.";
    return;
  }

  NSData* imageData = UIImagePNGRepresentation(image);
  NSString* base64String = [imageData base64EncodedStringWithOptions:0];
  _page_context->set_tab_screenshot(base::SysNSStringToUTF8(base64String));
}

// If it exists, convert the PDF data to base64 encoded string and set it in the
// PageContext proto.
- (void)encodeAndSetFullPagePDF:(NSData*)PDFData {
  if (!PDFData) {
    DLOG(WARNING) << "Failed to fetch webpage PDF data.";
    return;
  }

  NSString* base64String = [PDFData base64EncodedStringWithOptions:0];
  _page_context->set_pdf_data(base::SysNSStringToUTF8(base64String));
}

// If it exists, parse and trim the returned base::Value from the JavaScript
// execution, and append it to the `_webFramesInnerTexts` array. `error` is
// defined if the JavaScript execution failed.
- (void)parseAndConcatenateJavaScriptValue:(const base::Value*)value
                                 withError:(NSError*)error {
  if (error || !value || !value->is_string()) {
    DLOG(WARNING) << "Failed to fetch webpage innerText.";
    if (error) {
      DLOG(WARNING) << base::SysNSStringToUTF8([error localizedDescription]);
    }
    return;
  }

  NSString* resultString = [base::SysUTF8ToNSString(value->GetString())
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];

  if (!resultString.length) {
    return;
  }

  [_webFramesInnerTexts addObject:resultString];
}

// Concatenate the innerText strings, and set the result in the PageContext
// proto.
- (void)webFramesInnerTextsFetchCompleted {
  NSString* concatenatedInnerTexts =
      [_webFramesInnerTexts componentsJoinedByString:@"\n"];
  _page_context->set_inner_text(
      base::SysNSStringToUTF8(concatenatedInnerTexts));
}

@end
