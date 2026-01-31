// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

// Objective-C protocol mirroring ReaderModeTabHelper::Observer.
@protocol ReaderModeTabHelperObserving <NSObject>
@optional
- (void)readerModeWebStateDidLoadContent:(ReaderModeTabHelper*)tabHelper
                                webState:(web::WebState*)webState;
- (void)readerModeWebStateWillBecomeUnavailable:(ReaderModeTabHelper*)tabHelper
                                       webState:(web::WebState*)webState
                                         reason:(ReaderModeDeactivationReason)
                                                    reason;
- (void)readerModeDistillationFailed:(ReaderModeTabHelper*)tabHelper;
- (void)readerModeTabHelperDestroyed:(ReaderModeTabHelper*)tabHelper
                            webState:(web::WebState*)webState;
@end

// Observer bridge to forward C++ ReaderModeTabHelper events to Objective-C
// observer.
class ReaderModeTabHelperObserverBridge : public ReaderModeTabHelper::Observer {
 public:
  explicit ReaderModeTabHelperObserverBridge(
      id<ReaderModeTabHelperObserving> observer);
  ~ReaderModeTabHelperObserverBridge() override;

  // ReaderModeTabHelper::Observer implementation.
  void ReaderModeWebStateDidLoadContent(ReaderModeTabHelper* tab_helper,
                                        web::WebState* web_state) override;
  void ReaderModeWebStateWillBecomeUnavailable(
      ReaderModeTabHelper* tab_helper,
      web::WebState* web_state,
      ReaderModeDeactivationReason reason) override;
  void ReaderModeDistillationFailed(ReaderModeTabHelper* tab_helper) override;
  void ReaderModeTabHelperDestroyed(ReaderModeTabHelper* tab_helper,
                                    web::WebState* web_state) override;

 private:
  __weak id<ReaderModeTabHelperObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_OBSERVER_BRIDGE_H_
