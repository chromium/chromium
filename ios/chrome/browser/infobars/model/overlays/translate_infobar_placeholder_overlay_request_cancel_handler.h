// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_TRANSLATE_INFOBAR_PLACEHOLDER_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_TRANSLATE_INFOBAR_PLACEHOLDER_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_cancel_handler.h"

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/translate_overlay_tab_helper.h"

class OverlayRequestQueue;
class TranslateOverlayTabHelper;

namespace translate_infobar_overlays {

// A cancel handler for Translate Infobar placeholder requests that observes the
// TranslateInfobarDelegate and cancels the request when the translation
// finishes.
class PlaceholderRequestCancelHandler
    : public InfobarOverlayRequestCancelHandler {
 public:
  // Constructor for a handler that cancels `request` of `translate_infobar`.
  PlaceholderRequestCancelHandler(OverlayRequest* request,
                                  OverlayRequestQueue* queue,
                                  TranslateOverlayTabHelper* tab_helper,
                                  InfoBarIOS* translate_infobar);
  ~PlaceholderRequestCancelHandler() override;

 private:
  // Observes TranslateOverlayTabHelper to cancel the placeholder when Translate
  // finishes.
  class TranslationFinishedObserver
      : public TranslateOverlayTabHelper::Observer {
   public:
    TranslationFinishedObserver(
        TranslateOverlayTabHelper* tab_helper,
        PlaceholderRequestCancelHandler* cancel_handler);
    ~TranslationFinishedObserver() override;

   private:
    // TranslateOverlayTabHelper::Observer
    void TranslationFinished(TranslateOverlayTabHelper* tab_helper,
                             bool success) override;
    void TranslateOverlayTabHelperDestroyed(
        TranslateOverlayTabHelper* tab_helper) override;

    raw_ptr<PlaceholderRequestCancelHandler> cancel_handler_;

    base::ScopedObservation<TranslateOverlayTabHelper,
                            TranslateOverlayTabHelper::Observer>
        scoped_observation_{this};
  };

  // Indicates to the cancel handler that the translation has finished.
  void TranslationHasFinished();

  TranslationFinishedObserver translation_finished_observer_;
};

}  // namespace translate_infobar_overlays

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_TRANSLATE_INFOBAR_PLACEHOLDER_OVERLAY_REQUEST_CANCEL_HANDLER_H_
