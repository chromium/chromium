// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_cancel_handler.h"

class InfoBarIOS;

// OverlayRequestCancelHandler that cancels its OverlayRequest when its InfoBar
// is removed from its InfoBarManager.
class InfobarOverlayRequestCancelHandler : public OverlayRequestCancelHandler {
 public:
  InfobarOverlayRequestCancelHandler(OverlayRequest* request,
                                     OverlayRequestQueue* queue,
                                     InfoBarIOS* infobar);
  ~InfobarOverlayRequestCancelHandler() override;

 protected:
  // Returns the InfoBar that the corresponding request was configured with.
  InfoBarIOS* infobar() const { return infobar_; }

  // Called when the infobar triggering `request` was replaced in its manager.
  // Default implementation does nothing.
  virtual void HandleReplacement(InfoBarIOS* replacement);

 private:
  // Cancels the request when an InfoBar is removed from its InfoBarManager.
  void CancelForInfobarRemoval();

  // Helper object that triggers cancellation when its InfoBar is removed from
  // its InfoBarManager.
  class RemovalObserver : public infobars::InfoBarManager::Observer {
   public:
    RemovalObserver(InfobarOverlayRequestCancelHandler* cancel_handler);
    ~RemovalObserver() override;

   private:
    // infobars::InfoBarManager::Observer:
    void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
    void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                           infobars::InfoBar* new_infobar) override;
    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

   private:
    raw_ptr<InfobarOverlayRequestCancelHandler> cancel_handler_ = nullptr;
    base::ScopedObservation<infobars::InfoBarManager,
                            infobars::InfoBarManager::Observer>
        scoped_observation_{this};
  };

  raw_ptr<InfoBarIOS> infobar_ = nullptr;
  RemovalObserver removal_observer_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_REQUEST_CANCEL_HANDLER_H_
