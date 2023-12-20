// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_IMAGE_FETCH_IMAGE_FETCH_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_IMAGE_FETCH_IMAGE_FETCH_JAVA_SCRIPT_FEATURE_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

class GURL;

// A feature which can retrieve image data from a webpage.
class ImageFetchJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // Returns a singleton instance of ImageFetchJavaScriptFeature which uses
  // ImageFetchTabHelper as its handler.
  static ImageFetchJavaScriptFeature* GetInstance();

  class Handler {
   public:
    virtual ~Handler() = default;

    // Called when the webpage successfully sends back image data. `call_id` was
    // the token originally passed to GetImageData(). `decoded_data` is the raw
    // image data. `from` is a string explaining how the image data was
    // retrieved and may be empty.
    virtual void HandleJsSuccess(int call_id,
                                 std::string& decoded_data,
                                 std::string& from) = 0;

    // Called when the webpage fails to retrieve image data. `call_id` was the
    // token originally passed to GetImageData().
    virtual void HandleJsFailure(int call_id) = 0;
  };

  // Gets image data in binary format by trying 2 JavaScript methods in order:
  //   1. Draw <img> to <canvas> and export its data;
  //   2. Download the image by XMLHttpRequest and hopefully get responded from
  //   cache.
  // `url` should be equal to the resolved "src" attribute of <img>, otherwise
  // method 1 will fail. `call_id` is an opaque token that will be passed back
  // along with the response.
  //
  // Upon success or failure, this will invoke the appropriate Handler method.
  void GetImageData(web::WebState* web_state, int call_id, const GURL& url);

 private:
  // Tests are added as friends so that they can call the
  // ImageFetchJavaScriptFeature constructor, rather than relying on the
  // singleton instance.
  friend class ImageFetchTabHelperTest;
  friend class ImageFetchJavaScriptFeatureTest;
  friend class base::NoDestructor<ImageFetchJavaScriptFeature>;

  // Constructs an ImageFetchJavaScriptFeature which uses the given
  // `handler_factory`. Production code will generally install a factory which
  // returns the ImageFetchTabHelper for the given WebState, while test code can
  // install a custom factory to make testing easier. `handler_factory` can
  // return nullptr and will always be passed a non-nullptr WebState.
  ImageFetchJavaScriptFeature(
      base::RepeatingCallback<Handler*(web::WebState*)> handler_factory);
  ~ImageFetchJavaScriptFeature() override;

  ImageFetchJavaScriptFeature(const ImageFetchJavaScriptFeature&) = delete;
  ImageFetchJavaScriptFeature& operator=(const ImageFetchJavaScriptFeature&) =
      delete;

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  base::RepeatingCallback<Handler*(web::WebState*)> handler_factory_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_IMAGE_FETCH_IMAGE_FETCH_JAVA_SCRIPT_FEATURE_H_
