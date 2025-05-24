// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_IMAGE_TRANSCODER_JAVA_SCRIPT_IMAGE_TRANSCODER_H_
#define IOS_WEB_PUBLIC_JS_IMAGE_TRANSCODER_JAVA_SCRIPT_IMAGE_TRANSCODER_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"

namespace web {

// Image transcoder which converts an image to a different format, size and
// quality using JavaScript inside of a WKWebView. This can be used to re-encode
// untrustworthy image data locally. As of now, since the transcoder creates one
// WKWebView when in the constructor, the transcoder might stop working if the
// process associated with the WKWebView is terminated. If the process is
// terminated while an image is being transcoded, the completion should still be
// called with an error.
class JavaScriptImageTranscoder {
 public:
  JavaScriptImageTranscoder();
  JavaScriptImageTranscoder(const JavaScriptImageTranscoder&) = delete;
  JavaScriptImageTranscoder& operator=(const JavaScriptImageTranscoder&) =
      delete;
  ~JavaScriptImageTranscoder();

  // Transcodes an image from its base-64 encoded data `src_data_base64` to a
  // different format `type`, `width` and `height`. If the destination type
  // supports lossy compression, then `destinationQuality` is used as the
  // destination quality. If `width` or `height` are nil, then the original
  // width or height will be used instead. If the destination quality is nil,
  // the default quality for the user-agent is used instead.
  // If transcoding is successful, the resulting image data is provided as a
  // base-64 encoded string in `completion_handler`. Otherwise, an error object
  // is provided instead.
  // Completion will not called if the transcoder object is destroyed.
  void TranscodeImageBase64(
      NSString* src_data_base64,
      NSString* type,
      NSNumber* width,
      NSNumber* height,
      NSNumber* quality,
      base::OnceCallback<void(NSString*, NSError*)> completion_handler);

  // Transcodes an image from image data `src_data` to a different format
  // `type`, `width` and `height`. If the destination type supports lossy
  // compression, then `destinationQuality` is used as the destination quality.
  // If `width` or `height` are nil, then the original width or height will be
  // used instead. If the destination quality is nil, the default quality for
  // the user-agent is used instead. If transcoding is successful, the resulting
  // image data is provided in `completion_handler`. Otherwise, an error object
  // is provided instead.
  // Completion will not called if the transcoder object is destroyed.
  void TranscodeImage(
      NSData* src_data,
      NSString* type,
      NSNumber* width,
      NSNumber* height,
      NSNumber* quality,
      base::OnceCallback<void(NSData*, NSError*)> completion_handler);

 private:
  // Called when the page script was loaded. `result` is ignored but the error
  // will be stored in `page_script_loading_error_`.
  void OnPageScriptLoaded(id result, NSError* error);

  // WKWebView used to execute JavaScript.
  WKWebView* web_view_ = nil;
  // Whether the page script is still loading.
  bool page_script_loading_ = true;
  // If page script loading fails, the error is stored here.
  NSError* page_script_loading_error_ = nil;
  // Closure executed when the page script has been loaded into the page.
  base::OnceClosure page_script_loaded_closure_ = base::DoNothing();

  base::WeakPtrFactory<JavaScriptImageTranscoder> weak_ptr_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_IMAGE_TRANSCODER_JAVA_SCRIPT_IMAGE_TRANSCODER_H_
