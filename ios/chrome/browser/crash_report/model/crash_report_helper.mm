// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/crash_report_helper.h"

#import <Foundation/Foundation.h>

#import "base/auto_reset.h"
#import "base/debug/crash_logging.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/upload_list/crash_upload_list.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_report_user_application_state.h"
#import "ios/chrome/browser/crash_report/model/crash_reporter_url_observer.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"

// WebStateList Observer that some tabs stats to be sent to the crash server.
@interface CrashReporterTabStateObserver
    : NSObject <CRWWebStateObserver, WebStateListObserving> {
 @private
  // Map associating the tab id to an object describing the current state of the
  // tab.
  NSMutableDictionary* _tabCurrentStateByTabId;
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
// Removes the stats for the tab tabId
- (void)removeTabId:(NSString*)tabId;
// Removes document related information from tabCurrentStateByTabId_.
- (void)closingDocumentInTab:(NSString*)tabId;
// Sets a tab `tabId` specific information with key `key` and value `value` in
// tabCurrentStateByTabId_.
- (void)setTabInfo:(NSString*)key
         withValue:(const NSString*)value
            forTab:(NSString*)tabId;
// Retrieves the `key` information for tab `tabID`.
- (id)tabInfo:(NSString*)key forTab:(NSString*)tabID;
// Removes the `key` information for tab `tabId`
- (void)removeTabInfo:(NSString*)key forTab:(NSString*)tabId;
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

namespace {
// Mime type used for PDF documents.
const NSString* kDocumentMimeType = @"application/pdf";
}  // namespace

@implementation CrashReporterTabStateObserver

+ (CrashReporterTabStateObserver*)uniqueInstance {
  static CrashReporterTabStateObserver* instance =
      [[CrashReporterTabStateObserver alloc] init];
  return instance;
}

- (id)init {
  if ((self = [super init])) {
    _tabCurrentStateByTabId = [[NSMutableDictionary alloc] init];
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)closingDocumentInTab:(NSString*)tabId {
  NSString* mime = (NSString*)[self tabInfo:@"mime" forTab:tabId];
  if ([kDocumentMimeType isEqualToString:mime]) {
    crash_keys::SetCurrentTabIsPDF(false);
  }
  [self removeTabInfo:@"mime" forTab:tabId];
}

- (void)setTabInfo:(NSString*)key
         withValue:(const NSString*)value
            forTab:(NSString*)tabId {
  NSMutableDictionary* tabCurrentState =
      [_tabCurrentStateByTabId objectForKey:tabId];
  if (tabCurrentState == nil) {
    NSMutableDictionary* currentStateOfNewTab =
        [[NSMutableDictionary alloc] init];
    [_tabCurrentStateByTabId setObject:currentStateOfNewTab forKey:tabId];
    tabCurrentState = [_tabCurrentStateByTabId objectForKey:tabId];
  }
  [tabCurrentState setObject:value forKey:key];
}

- (id)tabInfo:(NSString*)key forTab:(NSString*)tabID {
  NSMutableDictionary* tabValues = [_tabCurrentStateByTabId objectForKey:tabID];
  return [tabValues objectForKey:key];
}

- (void)removeTabInfo:(NSString*)key forTab:(NSString*)tabId {
  [[_tabCurrentStateByTabId objectForKey:tabId] removeObjectForKey:key];
}

- (void)removeTabId:(NSString*)tabId {
  [self closingDocumentInTab:tabId];
  [_tabCurrentStateByTabId removeObjectForKey:tabId];
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
      [self
          removeTabId:detachChange.detached_web_state()->GetStableIdentifier()];
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replaceChange =
          change.As<WebStateListChangeReplace>();
      [self removeTabId:replaceChange.replaced_web_state()
                            ->GetStableIdentifier()];
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
  [self closingDocumentInTab:webState->GetStableIdentifier()];
}

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  if (!loadSuccess || webState->GetContentsMimeType() != "application/pdf") {
    return;
  }
  NSString* tabID = webState->GetStableIdentifier();
  NSString* oldMime = (NSString*)[self tabInfo:@"mime" forTab:tabID];
  if ([kDocumentMimeType isEqualToString:oldMime]) {
    return;
  }

  [self setTabInfo:@"mime" withValue:kDocumentMimeType forTab:tabID];
  crash_keys::SetCurrentTabIsPDF(true);
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
