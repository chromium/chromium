// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_REQUEST_INSERTER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_REQUEST_INSERTER_H_

#include <map>
#include <memory>

#import "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_factory.h"
#include "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"
#include "ios/web/public/web_state_user_data.h"

class InfoBarIOS;
class InfobarModalCompletionNotifier;
class OverlayRequestQueue;
namespace web {
class WebState;
}

// Struct to indicate what is triggering the Infobar overlay request insertion.
enum class InfobarOverlayInsertionSource {
  kInfoBarManager,  // Request initiated from InfoBarManager::Observer callbacks
                    // (i.e. primary banners for Infobars).
  kBanner,          // Request initiated from a banner button tap.
  kDetailSheet,     // Request initiated from a detail sheet action.
  kBadge,           // Request initiated from a badge tap.
  kInfoBarDelegate,  // Request initiated from the InfoBarDelegate (i.e.
                     // secondary banner for translate).
};

// Struct passed into InfobarOverlayRequestInserter API.
struct InsertParams {
  explicit InsertParams(InfoBarIOS* infobar);
  InsertParams() = delete;

  raw_ptr<InfoBarIOS> infobar;
  InfobarOverlayType overlay_type = InfobarOverlayType::kBanner;
  size_t insertion_index = 0;
  InfobarOverlayInsertionSource source =
      InfobarOverlayInsertionSource::kInfoBarManager;
};

// Helper object that creates OverlayRequests for InfoBars and inserts them into
// a WebState's OverlayRequestQueues.
class InfobarOverlayRequestInserter
    : public web::WebStateUserData<InfobarOverlayRequestInserter> {
 public:
  ~InfobarOverlayRequestInserter() override;

  // Creates an OverlayRequest with `params` configurations.
  void InsertOverlayRequest(const InsertParams& params);

  // Notifies observers of Infobar request insertions
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    // Called to notify observers that an Infobar request has been inserted
    // with `params` configurations.
    // `params.insertion_index` must be less than or equal to the size of the
    // queue.
    virtual void InfobarRequestInserted(InfobarOverlayRequestInserter* inserter,
                                        const InsertParams& params) = 0;
    // Called to notify observers that the `inserter` is about to be destroyed;
    virtual void InserterDestroyed(InfobarOverlayRequestInserter* inserter) = 0;
  };

  // Adds and removes observers of inserted modals.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class web::WebStateUserData<InfobarOverlayRequestInserter>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Constructor for an inserter that uses `factory` to construct
  // OverlayRequests to insert into `web_state`'s OverlayRequestQueues.
  // Both `web_state` and `factory` must be non-null.
  InfobarOverlayRequestInserter(web::WebState* web_state,
                                InfobarOverlayRequestFactory factory);

  // The WebState whose queues are being inserted into.
  raw_ptr<web::WebState> web_state_ = nullptr;
  // The infobar modal completion notifier.
  std::unique_ptr<InfobarModalCompletionNotifier> modal_completion_notifier_;
  // The factory used to create OverlayRequests.
  InfobarOverlayRequestFactory request_factory_;
  // Map of the OverlayRequestQueues to use for each InfobarOverlayType.
  std::map<InfobarOverlayType, OverlayRequestQueue*> queues_;
  // Observers of request insertions.
  base::ObserverList<Observer, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_REQUEST_INSERTER_H_
