// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/otr_web_state_observer.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/all_web_state_list_observation_registrar.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

namespace segmentation_platform {
namespace {

// Returns whether the BrowserState corresponding to BrowserList has any
// OTR WebState.
bool BrowserListHasOTRWebStates(BrowserList* browser_list) {
  for (Browser* browser :
       browser_list->BrowsersOfType(BrowserList::BrowserType::kIncognito)) {
    if (!browser->GetWebStateList()->empty()) {
      return true;
    }
  }
  return false;
}

}  // namespace

#pragma mark OTRWebStateObserver::WebStateObserver

class OTRWebStateObserver::WebStateObserver final
    : public WebStateListObserver {
 public:
  WebStateObserver(std::string_view browser_state_name,
                   OTRWebStateObserver* states_observer,
                   BrowserList* browser_list);

  ~WebStateObserver() final;

  // WebStateListObserver
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) final;
  void BatchOperationEnded(WebStateList* web_state_list) final;

 private:
  // Updates whether the BrowserList has any OTR WebStates.
  void UpdateHasOtrWebStates();

  const std::string browser_state_name_;

  const raw_ptr<OTRWebStateObserver> states_observer_;

  // BrowserList should be valid as WebStateList notifications are running.
  const raw_ptr<BrowserList> browser_list_;
};

OTRWebStateObserver::WebStateObserver::WebStateObserver(
    std::string_view browser_state_name,
    OTRWebStateObserver* states_observer,
    BrowserList* browser_list)
    : browser_state_name_(browser_state_name),
      states_observer_(states_observer),
      browser_list_(browser_list) {}

OTRWebStateObserver::WebStateObserver::~WebStateObserver() = default;

void OTRWebStateObserver::WebStateObserver::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (web_state_list->IsBatchInProgress()) {
    // Ignore changes during batch operation.
    return;
  }

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach:
    case WebStateListChange::Type::kInsert:
      UpdateHasOtrWebStates();
      break;
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace:
      // Do nothing when a WebState is replaced.
      break;
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}

void OTRWebStateObserver::WebStateObserver::BatchOperationEnded(
    WebStateList* web_state_list) {
  UpdateHasOtrWebStates();
}

void OTRWebStateObserver::WebStateObserver::UpdateHasOtrWebStates() {
  states_observer_->OnWebStateListChanged(
      browser_state_name_, BrowserListHasOTRWebStates(browser_list_.get()));
}

#pragma mark OTRWebStateObserver::BrowserStateData

// Stores data about a ChromeBrowserState.
class OTRWebStateObserver::BrowserStateData {
 public:
  BrowserStateData(std::string_view browser_state_name,
                   OTRWebStateObserver* states_observer,
                   BrowserList* browser_list);

  BrowserStateData(const BrowserStateData&) = delete;
  BrowserStateData& operator=(BrowserStateData&) = delete;

  ~BrowserStateData();

  void set_has_otr_web_states(bool has_otr_web_states) {
    has_otr_web_states_ = has_otr_web_states;
  }

  bool has_otr_web_states() const { return has_otr_web_states_; }

 private:
  // Whether any OTR WebStates exists for the BrowserState.
  bool has_otr_web_states_ = false;

  // Observer for all WebState(s) in the state.
  AllWebStateListObservationRegistrar all_web_state_observation_;
};

OTRWebStateObserver::BrowserStateData::BrowserStateData(
    std::string_view browser_state_name,
    OTRWebStateObserver* states_observer,
    BrowserList* browser_list)
    : has_otr_web_states_(BrowserListHasOTRWebStates(browser_list)),
      all_web_state_observation_(
          browser_list,
          std::make_unique<OTRWebStateObserver::WebStateObserver>(
              browser_state_name,
              states_observer,
              browser_list),
          AllWebStateListObservationRegistrar::Mode::INCOGNITO) {}

OTRWebStateObserver::BrowserStateData::~BrowserStateData() = default;

#pragma mark OTRWebStateObserver

OTRWebStateObserver::OTRWebStateObserver(
    ChromeBrowserStateManager* browser_state_manager) {
  browser_state_manager_observation_.Observe(browser_state_manager);
  for (ChromeBrowserState* browser_state :
       browser_state_manager->GetLoadedBrowserStates()) {
    OnChromeBrowserStateLoaded(browser_state_manager, browser_state);
  }
}

OTRWebStateObserver::~OTRWebStateObserver() {
  // TearDown() must be called before destruction.
  DCHECK(shutting_down_);
}

void OTRWebStateObserver::OnChromeBrowserStateManagerDestroyed(
    ChromeBrowserStateManager* manager) {
  browser_state_manager_observation_.Reset();
}

void OTRWebStateObserver::OnChromeBrowserStateCreated(
    ChromeBrowserStateManager* manager,
    ChromeBrowserState* browser_state) {
  // Nothing to do, the ChromeBrowserState is not yet fully initialized,
  // and thus KeyedService cannot be accessed yet nor WebState attached.
}

void OTRWebStateObserver::OnChromeBrowserStateLoaded(
    ChromeBrowserStateManager* manager,
    ChromeBrowserState* browser_state) {
  // The OTRWebStateObserver is created lazily by some `KeyedService` as part
  // of the first `ChromeBrowserState` initialisation. This causes this method
  // to be called twice, once from the constructor, and a second time from the
  // `ChromeBrowserStateManager`. So check whether the `ChromeBrowserState` is
  // already observed and return if this is the case.
  const std::string& name = browser_state->GetBrowserStateName();
  auto iterator = browser_state_data_.find(name);
  if (iterator != browser_state_data_.end()) {
    return;
  }

  bool inserted = false;
  std::tie(iterator, inserted) = browser_state_data_.insert(std::make_pair(
      name,
      std::make_unique<BrowserStateData>(
          name, this, BrowserListFactory::GetForBrowserState(browser_state))));

  DCHECK(inserted);
  DCHECK(iterator != browser_state_data_.end());
  DCHECK(iterator->second != nullptr);

  // In the unlikely event that the newly loaded ChromeBrowserState already
  // has OTR WebState, informs the observer. No need to inform if it does
  // not have any since this would not have changed the global state.
  if (iterator->second->has_otr_web_states()) {
    for (ObserverClient& obs : observer_clients_) {
      obs.OnOTRWebStateCountChanged(true);
    }
  }
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
  browser_state_manager_observation_.Reset();
  browser_state_data_.clear();
}

void OTRWebStateObserver::OnWebStateListChanged(
    std::string_view browser_state_name,
    bool has_otr_web_states) {
  auto iterator = browser_state_data_.find(browser_state_name);
  DCHECK(iterator != browser_state_data_.end());
  DCHECK(iterator->second != nullptr);

  iterator->second->set_has_otr_web_states(has_otr_web_states);
  const bool has_otr_state = has_otr_web_states || HasAnyOtrWebState();
  for (ObserverClient& obs : observer_clients_) {
    obs.OnOTRWebStateCountChanged(has_otr_state);
  }
}

bool OTRWebStateObserver::HasAnyOtrWebState() const {
  for (const auto& [key, data] : browser_state_data_) {
    if (data->has_otr_web_states()) {
      return true;
    }
  }
  return false;
}

}  // namespace segmentation_platform
