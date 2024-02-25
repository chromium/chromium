// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_IMAGE_FETCH_IMAGE_FETCH_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_IMAGE_FETCH_IMAGE_FETCH_TAB_HELPER_H_

#include <string>
#include <unordered_map>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/web/model/image_fetch/image_fetch_java_script_feature.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Key of the UMA ContextMenu.iOS.GetImageDataByJsResult histogram.
extern const char kUmaGetImageDataByJsResult[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Enum for the ContextMenu.iOS.GetImageDataByJsResult UMA histogram to report
// the results of GetImageDataByJs.
enum class ContextMenuGetImageDataByJsResult {
  kCanvasSucceed = 0,
  kXMLHttpRequestSucceed = 1,
  kFail = 2,
  kTimeout = 3,
  kMaxValue = kTimeout,
};

// Gets the image data by JavaScript or
// image_fetcher::FetchImageData. Always use this class by
// ImageFetchTabHelper::FromWebState on UI thread. All callbacks will also be
// invoked on UI thread.
class ImageFetchTabHelper : public ImageFetchJavaScriptFeature::Handler,
                            public web::WebStateObserver,
                            public web::WebStateUserData<ImageFetchTabHelper> {
 public:
  ImageFetchTabHelper(const ImageFetchTabHelper&) = delete;
  ImageFetchTabHelper& operator=(const ImageFetchTabHelper&) = delete;

  ~ImageFetchTabHelper() override;

  // Callback for GetImageData. `data` will be in binary format, or nil if
  // GetImageData failed.
  typedef void (^ImageDataCallback)(NSData* data);

  // Gets image data in binary format by following steps:
  //   1. Call injected JavaScript to get the image data from web page;
  //   2. If JavaScript fails or does not send a message back in 300ms, try
  //   downloading the image by image_fetcher::FetchImageData.
  virtual void GetImageData(const GURL& url,
                            const web::Referrer& referrer,
                            ImageDataCallback callback);

  // ImageFetchJavaScriptFeature::Handler.
  void HandleJsSuccess(int call_id,
                       std::string& decoded_data,
                       std::string& from) override;
  void HandleJsFailure(int call_id) override;

 protected:
  friend class web::WebStateUserData<ImageFetchTabHelper>;

  explicit ImageFetchTabHelper(web::WebState* web_state);

  // web::WebStateObserver overrides:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Callback for GetImageDataByJs. `data` will be in binary format, or nullptr
  // if GetImageDataByJs failed.
  typedef base::OnceCallback<void(const std::string* data)> JsCallback;

  // Gets image data in binary format via ImageFetchJavaScriptFeature.
  // `url` should be equal to the resolved "src" attribute of <img>, otherwise
  // the method 1 would fail. If the JavaScript does not respond after
  // `timeout`, the `callback` will be invoked with nullptr.
  void GetImageDataByJs(const GURL& url,
                        base::TimeDelta timeout,
                        JsCallback&& callback);

  // Records ContextMenu.iOS.GetImageDataByJsResult UMA histogram.
  void RecordGetImageDataByJsResult(ContextMenuGetImageDataByJsResult result);

  // Handler for timeout on GetImageDataByJs.
  void OnJsTimeout(int call_id);

  // Handler for calling GetImageDataByJs inside GetImageData.
  void JsCallbackOfGetImageData(const GURL& url,
                                const web::Referrer& referrer,
                                ImageDataCallback callback,
                                const std::string* data);

  // WebState this tab helper is attached to.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Store callbacks for GetImageData, with url as key.
  std::unordered_map<int, JsCallback> js_callbacks_;

  // `GetImageData` uses this counter as ID to match calls with callbacks. Each
  // call on `GetImageData` will increment `call_id_` by 1 and pass it as ID
  // when calling JavaScript. The ID will be regained in the message received in
  // `OnImageDataReceived` and used to invoke the corresponding callback.
  int call_id_ = 0;

  base::WeakPtrFactory<ImageFetchTabHelper> weak_ptr_factory_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_IMAGE_FETCH_IMAGE_FETCH_TAB_HELPER_H_
