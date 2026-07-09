// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TAB_CARD_LABEL_DATA_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TAB_CARD_LABEL_DATA_H_

#import <string>

#import "base/scoped_observation.h"
#import "base/time/time.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

// A WebState UserData class that stores the metadata for a Send Tab to Self
// tab card label (like the sender device name). It automatically cleans itself
// up when the tab is shown (viewed by the user).
// Note that this data is not persisted across restarts.
class SendTabToSelfTabCardLabelData
    : public web::WebStateUserData<SendTabToSelfTabCardLabelData>,
      public web::WebStateObserver {
 public:
  ~SendTabToSelfTabCardLabelData() override;

  // Returns the active SendTabToSelfTabCardLabelData for the WebState, or
  // nullptr if it doesn't exist (because the tab was not sent to device or the
  // label expired or the tab was interacted with by the user).
  static SendTabToSelfTabCardLabelData* FromWebState(web::WebState* web_state);

  // Returns the formatted localized label string for the tab card.
  // This can be used even if the WebState is unrealized.
  static NSString* GetLabelTextForWebState(web::WebState* web_state);

  // Called when the WebState is closed by the user, to log abandonment.
  void WebStateClosedByUser(web::WebState* web_state);

  // Returns the creation timestamp of this label data.
  base::Time creation_time() const { return creation_time_; }

 private:
  friend class web::WebStateUserData<SendTabToSelfTabCardLabelData>;

  // Returns the formatted localized label string for the given device name.
  static NSString* GetLabelText(const std::u16string& device_name);

  SendTabToSelfTabCardLabelData(web::WebState* web_state,
                                const std::string& entry_guid,
                                const std::string& sender_device_name,
                                base::Time creation_time = base::Time::Now());

  // Logs the activation metric with the given entry point.
  void MarkEntryActivated(
      web::WebState* web_state,
      send_tab_to_self::ShareActivatedEntryPoint entry_point);

  // web::WebStateObserver:
  void WasShown(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  std::string entry_guid_;
  std::u16string sender_device_name_;
  base::Time creation_time_;
};

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TAB_CARD_LABEL_DATA_H_
