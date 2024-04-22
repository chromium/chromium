// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_SERVICE_H_
#define IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_SERVICE_H_

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/web_resource/resource_request_allowed_notifier.h"

namespace ios_web_view {

// Singleton managing the resources required for Translate.
class WebViewTranslateService {
 public:
  static WebViewTranslateService* GetInstance();

  WebViewTranslateService(const WebViewTranslateService&) = delete;
  WebViewTranslateService& operator=(const WebViewTranslateService&) = delete;

  // Must be called before the Translate feature can be used.
  void Initialize();

  // Must be called to shut down the Translate feature.
  void Shutdown();

 private:
  // Manages enabling translate requests only when resource requests are
  // allowed.
  // TODO(crbug.com/41322782): Merge TranslateRequestsAllowedListener and
  // WebViewTranslateService. They currently must be separate classes because
  // the destructor of web_resource::ResourceRequestAllowedNotifier::Observer is
  // not virtual.
  class TranslateRequestsAllowedListener
      : public web_resource::ResourceRequestAllowedNotifier::Observer {
   public:
    TranslateRequestsAllowedListener();

    TranslateRequestsAllowedListener(const TranslateRequestsAllowedListener&) =
        delete;
    TranslateRequestsAllowedListener& operator=(
        const TranslateRequestsAllowedListener&) = delete;

    ~TranslateRequestsAllowedListener() override;

    // ResourceRequestAllowedNotifier::Observer methods.
    void OnResourceRequestsAllowed() override;

   private:
    // Notifier class to know if it's allowed to make network resource requests.
    web_resource::ResourceRequestAllowedNotifier
        resource_request_allowed_notifier_;
  };

  WebViewTranslateService();
  ~WebViewTranslateService();

  friend class base::NoDestructor<WebViewTranslateService>;

  // Listener which manages when translate requests can occur.
  std::unique_ptr<TranslateRequestsAllowedListener>
      translate_requests_allowed_listener_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_SERVICE_H_
