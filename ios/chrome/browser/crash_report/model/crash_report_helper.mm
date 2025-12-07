// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/crash_report_helper.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/containers/contains.h"
#import "base/debug/crash_logging.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_reporter_url_observer.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
#import "ios/web/public/web_state_observer_bridge.h"

// WebStateList Observer that some tabs stats to be sent to the crash server.
@interface CrashReporterTabStateObserver
    : NSObject <CRWWebStateObserver, WebStateListObserving> {
 @private
  // Set of WebStateID for tabs displaying PDF documents.
  std::set<web::WebStateID> _tabDisplayingPDFSet;
  // The WebStateObserverBridge used to register self as a WebStateObserver
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  // Bridges C++ WebStateListObserver methods to this
  // CrashReporterTabStateObserver.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // Forwards observer methods for all WebStates in each WebStateList monitored
  // by the CrashReporterTabStateObserver.
  std::map<WebStateList*, std::unique_ptr<AllWebStateObservationForwarder>>
      _allWebStateObservationForwarders;
}
+ (CrashReporterTabStateObserver*)uniqueInstance;
// Removes the stats for the tab.
- (void)removeTab:(web::WebState*)webState;
// Removes document related information from tabCurrentStateByTabId_.
- (void)closingDocumentInTab:(web::WebStateID)tabId;
// Observes `webState` by this instance of the CrashReporterTabStateObserver.
- (void)observeWebState:(web::WebState*)webState;
// Stop Observing `webState` by this instance of the
// CrashReporterTabStateObserver.
- (void)stopObservingWebState:(web::WebState*)webState;
// Observes `webStateList` by this instance of the
// CrashReporterTabStateObserver.
- (void)observeWebStateList:(WebStateList*)webStateList;
// Stop Observing `webStateList` by this instance of the
// CrashReporterTabStateObserver.
- (void)stopObservingWebStateList:(WebStateList*)webStateList;
@end

@implementation CrashReporterTabStateObserver

+ (CrashReporterTabStateObserver*)uniqueInstance {
  static CrashReporterTabStateObserver* instance =
      [[CrashReporterTabStateObserver alloc] init];
  return instance;
}

- (id)init {
  if ((self = [super init])) {
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)closingDocumentInTab:(web::WebStateID)tabId {
  if (!base::Contains(_tabDisplayingPDFSet, tabId)) {
    return;
  }

  crash_keys::SetCurrentTabIsPDF(false);
  _tabDisplayingPDFSet.erase(tabId);
}

- (void)removeTab:(web::WebState*)webState {
  [self closingDocumentInTab:webState->GetUniqueIdentifier()];
}

- (void)observeWebState:(web::WebState*)webState {
  webState->AddObserver(_webStateObserver.get());
}

- (void)stopObservingWebState:(web::WebState*)webState {
  webState->RemoveObserver(_webStateObserver.get());
}

- (void)observeWebStateList:(WebStateList*)webStateList {
  webStateList->AddObserver(_webStateListObserver.get());
  DCHECK(!_allWebStateObservationForwarders[webStateList]);
  // Observe all webStates of this webStateList, so that Tab states are saved in
  // cases of crashing.
  _allWebStateObservationForwarders[webStateList] =
      std::make_unique<AllWebStateObservationForwarder>(
          webStateList, _webStateObserver.get());
}

- (void)stopObservingWebStateList:(WebStateList*)webStateList {
  _allWebStateObservationForwarders[webStateList] = nullptr;
  _allWebStateObservationForwarders.erase(webStateList);

  webStateList->RemoveObserver(_webStateListObserver.get());
}

#pragma mark - WebStateListObserving protocol

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detachChange =
          change.As<WebStateListChangeDetach>();
      [self removeTab:detachChange.detached_web_state()];
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replaceChange =
          change.As<WebStateListChangeReplace>();
      [self removeTab:replaceChange.replaced_web_state()];
      break;
    }
    case WebStateListChange::Type::kInsert:
      // Do nothing when a new WebState is inserted.
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

#pragma mark - CRWWebStateObserver protocol

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self closingDocumentInTab:webState->GetUniqueIdentifier()];
}

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  if (!loadSuccess) {
    return;
  }

  const std::string& mimeType = webState->GetContentsMimeType();
  if (mimeType != kAdobePortableDocumentFormatMimeType) {
    return;
  }

  const web::WebStateID tabId = webState->GetUniqueIdentifier();
  if (base::Contains(_tabDisplayingPDFSet, tabId)) {
    return;
  }

  crash_keys::SetCurrentTabIsPDF(true);
  _tabDisplayingPDFSet.insert(tabId);
}

@end

namespace crash_report_helper {

void MonitorURLsForPreloadWebState(web::WebState* web_state) {
  CrashReporterURLObserver::GetSharedInstance()->ObservePreloadWebState(
      web_state);
}

void StopMonitoringURLsForPreloadWebState(web::WebState* web_state) {
  CrashReporterURLObserver::GetSharedInstance()->StopObservingPreloadWebState(
      web_state);
}

void MonitorURLsForWebStateList(WebStateList* web_state_list) {
  CrashReporterURLObserver::GetSharedInstance()->ObserveWebStateList(
      web_state_list);
}

void StopMonitoringURLsForWebStateList(WebStateList* web_state_list) {
  CrashReporterURLObserver::GetSharedInstance()->StopObservingWebStateList(
      web_state_list);
}

void MonitorTabStateForWebStateList(WebStateList* web_state_list) {
  [[CrashReporterTabStateObserver uniqueInstance]
      observeWebStateList:web_state_list];
}

void StopMonitoringTabStateForWebStateList(WebStateList* web_state_list) {
  [[CrashReporterTabStateObserver uniqueInstance]
      stopObservingWebStateList:web_state_list];
}

void ClearStateForWebStateList(WebStateList* web_state_list) {
  CrashReporterURLObserver::GetSharedInstance()->RemoveWebStateList(
      web_state_list);
}

}  // namespace crash_report_helper
