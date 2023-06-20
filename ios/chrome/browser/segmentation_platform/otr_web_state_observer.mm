// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/otr_web_state_observer.h"

#import "ios/chrome/browser/shared/model/browser/all_web_state_list_observation_registrar.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace segmentation_platform {

class OTRWebStateObserver::WebStateObserver : public WebStateListObserver {
 public:
  WebStateObserver(const base::FilePath& browser_state_path,
                   OTRWebStateObserver* states_observer,
                   BrowserList* browser_list)
      : browser_state_path_(browser_state_path),
        states_observer_(states_observer),
        browser_list_(browser_list) {
    // Ensure the count is updated at creation time.
    UpdateOtrWebStateCount();
  }

  // WebStateListObserver
  void WebStateListChanged(WebStateList* web_state_list,
                           const WebStateListChange& change,
                           const WebStateSelection& selection) override;
  void BatchOperationEnded(WebStateList* web_state_list) override;

 private:
  void UpdateOtrWebStateCount();

  const base::FilePath browser_state_path_;

  const raw_ptr<OTRWebStateObserver> states_observer_;

  // BrowserList should be valid as WebStateList notifications are running.
  const raw_ptr<BrowserList> browser_list_;
};

#pragma mark - WebStateListObserver

void OTRWebStateObserver::WebStateObserver::WebStateListChanged(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateSelection& selection) {
  switch (change.type()) {
    case WebStateListChange::Type::kSelectionOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach:
    case WebStateListChange::Type::kInsert:
      UpdateOtrWebStateCount();
      break;
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace:
      // Do nothing when a WebState is replaced.
      break;
  }
}

void OTRWebStateObserver::WebStateObserver::BatchOperationEnded(
    WebStateList* web_state_list) {
  UpdateOtrWebStateCount();
}

void OTRWebStateObserver::WebStateObserver::UpdateOtrWebStateCount() {
  const std::set<Browser*>& browsers = browser_list_->AllIncognitoBrowsers();
  int otr_state_count = 0;
  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    if (web_state_list) {
      otr_state_count += web_state_list->count();
    }
  }
  states_observer_->OnWebStateListChanged(browser_state_path_, otr_state_count);
}

OTRWebStateObserver::BrowserStateData::BrowserStateData() = default;
OTRWebStateObserver::BrowserStateData::~BrowserStateData() = default;

OTRWebStateObserver::OTRWebStateObserver(
    ios::ChromeBrowserStateManager* browser_state_manager)
    : browser_state_manager_(browser_state_manager) {
  browser_state_manager_->GetBrowserStateInfoCache()->AddObserver(this);
  for (ChromeBrowserState* state :
       browser_state_manager_->GetLoadedBrowserStates()) {
    OnBrowserStateAdded(state->GetStatePath());
  }
}

OTRWebStateObserver::~OTRWebStateObserver() {
  // TearDown() must be called before destruction.
  DCHECK(shutting_down_);
}

void OTRWebStateObserver::OnBrowserStateAdded(const base::FilePath& path) {
  // This method can be called by the constructor and then the browser state
  // cache if this class is created at browser state init time. So, if the state
  // is already tracked, do nothing.
  if (browser_state_data_.count(path)) {
    return;
  }
  BrowserList* browser_list = BrowserListFactory::GetForBrowserState(
      browser_state_manager_->GetBrowserState(path));
  DCHECK(browser_list);

  auto it = browser_state_data_.emplace(
      std::make_pair(path, std::make_unique<BrowserStateData>()));
  DCHECK(it.second);
  BrowserStateData& data = *it.first->second;
  data.all_web_state_observation =
      std::make_unique<AllWebStateListObservationRegistrar>(
          browser_list,
          std::make_unique<WebStateObserver>(path, this, browser_list),
          AllWebStateListObservationRegistrar::INCOGNITO);
}

void OTRWebStateObserver::OnBrowserStateWasRemoved(const base::FilePath& path) {
  browser_state_data_.erase(path);
}

void OTRWebStateObserver::AddObserver(ObserverClient* client) {
  observer_clients_.AddObserver(client);
  // Notify if OTR state was created before registration.
  client->OnOTRWebStateCountChanged(HasAnyOtrWebState());
}

void OTRWebStateObserver::RemoveObserver(ObserverClient* client) {
  observer_clients_.RemoveObserver(client);
}

void OTRWebStateObserver::TearDown() {
  shutting_down_ = true;
  browser_state_manager_->GetBrowserStateInfoCache()->RemoveObserver(this);
  browser_state_data_.clear();
  browser_state_manager_ = nullptr;
}

void OTRWebStateObserver::OnWebStateListChanged(
    const base::FilePath& browser_state_path,
    int otr_web_state_count) {
  auto& data = browser_state_data_[browser_state_path];
  data->otr_web_state_count = otr_web_state_count;

  const bool has_otr_state = HasAnyOtrWebState();
  for (ObserverClient& obs : observer_clients_) {
    obs.OnOTRWebStateCountChanged(has_otr_state);
  }
}

bool OTRWebStateObserver::HasAnyOtrWebState() const {
  bool has_otr_state = false;
  for (const auto& data : browser_state_data_) {
    has_otr_state = has_otr_state || (data.second->otr_web_state_count > 0);
  }
  return has_otr_state;
}

}  // namespace segmentation_platform
