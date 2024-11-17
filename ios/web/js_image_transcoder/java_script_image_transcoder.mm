// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_image_transcoder/java_script_image_transcoder.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "ios/web/js_messaging/page_script_util.h"
#import "ios/web/js_messaging/web_view_js_utils.h"

namespace {

// Filename of the script to transcode images.
NSString* kTranscodeImageScriptFileName = @"transcode_image";

}  // namespace

namespace web {

JavaScriptImageTranscoder::JavaScriptImageTranscoder() {
  web_view_ = [[WKWebView alloc] init];
  NSString* script = web::GetPageScript(kTranscodeImageScriptFileName);
  auto execute_page_script_completion =
      base::BindOnce(&JavaScriptImageTranscoder::OnPageScriptLoaded,
                     weak_ptr_factory_.GetWeakPtr());
  web::ExecuteJavaScript(
      web_view_, script,
      base::CallbackToBlock(std::move(execute_page_script_completion)));
}

JavaScriptImageTranscoder::~JavaScriptImageTranscoder() {
  web_view_ = nil;
}

void JavaScriptImageTranscoder::TranscodeImageBase64(
    NSString* src_data_base64,
    NSString* type,
    NSNumber* width,
    NSNumber* height,
    NSNumber* quality,
    base::OnceCallback<void(NSString*, NSError*)> completion_handler) {
  // If loading already failed, complete with error now.
  if (page_script_loading_error_) {
    std::move(completion_handler).Run(nil, page_script_loading_error_);
    return;
  }
  // If loading is ongoing, add request to the queue.
  if (page_script_loading_) {
    page_script_loaded_closure_ =
        base::BindOnce(&JavaScriptImageTranscoder::TranscodeImageBase64,
                       weak_ptr_factory_.GetWeakPtr(), src_data_base64, type,
                       width, height, quality, std::move(completion_handler))
            .Then(std::move(page_script_loaded_closure_));
    return;
  }

  NSString* functionBody =
      @"return transcode_image.transcodeImage(srcDataBase64, type,"
      @"                                      width, height, quality)";
  NSDictionary<NSString*, id>* arguments = @{
    @"srcDataBase64" : src_data_base64,
    @"type" : type,
    @"width" : width ? width : [NSNull null],
    @"height" : height ? height : [NSNull null],
    @"quality" : quality ? quality : [NSNull null],
  };
  auto call_async_completion_handler = base::BindOnce(
      [](base::OnceCallback<void(NSString*, NSError*)> completion_handler,
         id result, NSError* error) {
        std::move(completion_handler)
            .Run(base::apple::ObjCCast<NSString>(result), error);
      },
      std::move(completion_handler));
  [web_view_ callAsyncJavaScript:functionBody
                       arguments:arguments
                         inFrame:nil
                  inContentWorld:[WKContentWorld pageWorld]
               completionHandler:base::CallbackToBlock(
                                     std::move(call_async_completion_handler))];
}

void JavaScriptImageTranscoder::TranscodeImage(
    NSData* src_data,
    NSString* type,
    NSNumber* width,
    NSNumber* height,
    NSNumber* quality,
    base::OnceCallback<void(NSData*, NSError*)> completion_handler) {
  TranscodeImageBase64(
      [src_data base64EncodedStringWithOptions:0], type, width, height, quality,
      base::BindOnce(
          [](base::OnceCallback<void(NSData*, NSError*)> completion_handler,
             NSString* dst_data_base_64, NSError* error) {
            NSData* dst_data = nil;
            if (dst_data_base_64) {
              dst_data =
                  [[NSData alloc] initWithBase64EncodedString:dst_data_base_64
                                                      options:0];
            }
            std::move(completion_handler).Run(dst_data, error);
          },
          std::move(completion_handler)));
}

#pragma mark - Private

void JavaScriptImageTranscoder::OnPageScriptLoaded(id result, NSError* error) {
  page_script_loading_ = false;
  page_script_loading_error_ = error;
  std::move(page_script_loaded_closure_).Run();
}

}  // namespace web
