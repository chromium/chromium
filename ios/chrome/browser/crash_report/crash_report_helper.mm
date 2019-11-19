// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/crash_report_helper.h"

#import <Foundation/Foundation.h>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/upload_list/crash_upload_list.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/crash_report/crash_report_user_application_state.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// WebStateListObserver that allows loaded urls to be sent to the crash server.
@interface CrashReporterURLObserver
    : NSObject <WebStateListObserving, CRWWebStateObserver> {
 @private
  // Map associating the tab id to the breakpad key used to keep track of the
  // loaded URL.
  NSMutableDictionary* breakpadKeyByTabId_;
  // List of keys to use for recording URLs. This list is sorted such that a new
  // tab must use the first key in this list to record its URLs.
  NSMutableArray* breakpadKeys_;
  // The WebStateObserverBridge used to register self as a WebStateObserver
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  // Forwards observer methods for all WebStates in the WebStateList monitored
  // by the CrashReporterURLObserver.
  std::unique_ptr<AllWebStateObservationForwarder>
      _allWebStateObservationForwarder;
  // Bridges C++ WebStateListObserver methods to this CrashReporterURLObserver.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}
+ (CrashReporterURLObserver*)uniqueInstance;
// Removes the URL for the tab with the given id from the URLs sent to the crash
// server.
- (void)removeTabId:(NSString*)tabId;
// Records the given URL associated to the given id to the list of URLs to send
// to the crash server. If |pending| is true, the URL is one that is
// expected to start loading, but hasn't actually been seen yet.
- (void)recordURL:(NSString*)url
         forTabId:(NSString*)tabId
          pending:(BOOL)pending;
// Observes |webState| by this instance of the CrashReporterURLObserver.
- (void)observeWebState:(web::WebState*)webState;
// Stop Observing |webState| by this instance of the CrashReporterURLObserver.
- (void)stopObservingWebState:(web::WebState*)webState;
// Observes |webStateList| by this instance of the CrashReporterURLObserver.
- (void)observeWebStateList:(WebStateList*)webStateList;
// Stop Observing |webStateList| by this instance of the
// CrashReporterURLObserver.
- (void)stopObservingWebStateList:(WebStateList*)webStateList;

@end

// WebStateList Observer that some tabs stats to be sent to the crash server.
@interface CrashReporterTabStateObserver
    : NSObject <CRWWebStateObserver, WebStateListObserving> {
 @private
  // Map associating the tab id to an object describing the current state of the
  // tab.
  NSMutableDictionary* tabCurrentStateByTabId_;
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
// Sets a tab |tabId| specific information with key |key| and value |value| in
// tabCurrentStateByTabId_.
- (void)setTabInfo:(NSString*)key
         withValue:(const NSString*)value
            forTab:(NSString*)tabId;
// Retrieves the |key| information for tab |tabId|.
- (id)getTabInfo:(NSString*)key forTab:(NSString*)tabId;
// Removes the |key| information for tab |tabId|
- (void)removeTabInfo:(NSString*)key forTab:(NSString*)tabId;
// Observes |webState| by this instance of the CrashReporterTabStateObserver.
- (void)observeWebState:(web::WebState*)webState;
// Stop Observing |webState| by this instance of the
// CrashReporterTabStateObserver.
- (void)stopObservingWebState:(web::WebState*)webState;
// Observes |webStateList| by this instance of the
// CrashReporterTabStateObserver.
- (void)observeWebStateList:(WebStateList*)webStateList;
// Stop Observing |webStateList| by this instance of the
// CrashReporterTabStateObserver.
- (void)stopObservingWebStateList:(WebStateList*)webStateList;
@end

namespace {

// Returns the breakpad key to use for a pending URL corresponding to the
// same tab that is using |key|.
NSString* PendingURLKeyForKey(NSString* key) {
  return [key stringByAppendingString:@"-pending"];
}

// Max number of urls to send. This must be kept low for privacy issue as well
// as because breakpad does limit the total number of parameters to 64.
const int kNumberOfURLsToSend = 1;

// Mime type used for PDF documents.
const NSString* kDocumentMimeType = @"application/pdf";
}  // namespace

@implementation CrashReporterURLObserver

+ (CrashReporterURLObserver*)uniqueInstance {
  static CrashReporterURLObserver* instance =
      [[CrashReporterURLObserver alloc] init];
  return instance;
}

- (id)init {
  if ((self = [super init])) {
    breakpadKeyByTabId_ =
        [[NSMutableDictionary alloc] initWithCapacity:kNumberOfURLsToSend];
    breakpadKeys_ =
        [[NSMutableArray alloc] initWithCapacity:kNumberOfURLsToSend];
    for (int i = 0; i < kNumberOfURLsToSend; ++i)
      [breakpadKeys_ addObject:[NSString stringWithFormat:@"url%d", i]];
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)removeTabId:(NSString*)tabId {
  NSString* key = [breakpadKeyByTabId_ objectForKey:tabId];
  if (!key)
    return;
  breakpad_helper::RemoveReportParameter(key);
  breakpad_helper::RemoveReportParameter(PendingURLKeyForKey(key));
  [breakpadKeyByTabId_ removeObjectForKey:tabId];
  [breakpadKeys_ removeObject:key];
  [breakpadKeys_ insertObject:key atIndex:0];
}

- (void)recordURL:(NSString*)url
         forTabId:(NSString*)tabId
          pending:(BOOL)pending {
  NSString* breakpadKey = [breakpadKeyByTabId_ objectForKey:tabId];
  BOOL reusingKey = NO;
  if (!breakpadKey) {
    // Get the first breakpad key and push it back at the end of the keys.
    breakpadKey = [breakpadKeys_ objectAtIndex:0];
    [breakpadKeys_ removeObject:breakpadKey];
    [breakpadKeys_ addObject:breakpadKey];
    // Remove the current mapping to the breakpad key.
    for (NSString* tabId in
         [breakpadKeyByTabId_ allKeysForObject:breakpadKey]) {
      reusingKey = YES;
      [breakpadKeyByTabId_ removeObjectForKey:tabId];
    }
    // Associate the breakpad key to the tab id.
    [breakpadKeyByTabId_ setObject:breakpadKey forKey:tabId];
  }
  NSString* pendingKey = PendingURLKeyForKey(breakpadKey);
  if (pending) {
    if (reusingKey)
      breakpad_helper::RemoveReportParameter(breakpadKey);
    breakpad_helper::AddReportParameter(pendingKey, url, true);
  } else {
    breakpad_helper::AddReportParameter(breakpadKey, url, true);
    breakpad_helper::RemoveReportParameter(pendingKey);
  }
}

- (void)observeWebState:(web::WebState*)webState {
  webState->AddObserver(_webStateObserver.get());
}

- (void)stopObservingWebState:(web::WebState*)webState {
  webState->RemoveObserver(_webStateObserver.get());
}

- (void)observeWebStateList:(WebStateList*)webStateList {
  webStateList->AddObserver(_webStateListObserver.get());
  // CrashReporterURLObserver should only observe one webStateList at a time.
  DCHECK(!_allWebStateObservationForwarder);
  // Observe all webStates of this tabModel, so that URLs are saved in cases
  // of crashing.
  _allWebStateObservationForwarder =
      std::make_unique<AllWebStateObservationForwarder>(
          webStateList, _webStateObserver.get());
}

- (void)stopObservingWebStateList:(WebStateList*)webStateList {
  _allWebStateObservationForwarder.reset(nullptr);
  webStateList->RemoveObserver(_webStateListObserver.get());
}

#pragma mark - WebStateListObserving protocol

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  [self removeTabId:TabIdTabHelper::FromWebState(webState)->tab_id()];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  [self removeTabId:TabIdTabHelper::FromWebState(oldWebState)->tab_id()];
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  if (!newWebState)
    return;
  web::NavigationItem* pendingItem =
      newWebState->GetNavigationManager()->GetPendingItem();
  const GURL& URL =
      pendingItem ? pendingItem->GetURL() : newWebState->GetLastCommittedURL();
  [self recordURL:base::SysUTF8ToNSString(URL.spec())
         forTabId:TabIdTabHelper::FromWebState(newWebState)->tab_id()
          pending:pendingItem ? YES : NO];
}

#pragma mark - CRWWebStateObserver protocol

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  NSString* urlString = base::SysUTF8ToNSString(navigation->GetUrl().spec());
  if (!urlString.length || webState->GetBrowserState()->IsOffTheRecord())
    return;
  [self recordURL:urlString
         forTabId:TabIdTabHelper::FromWebState(webState)->tab_id()
          pending:YES];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  NSString* urlString =
      base::SysUTF8ToNSString(webState->GetLastCommittedURL().spec());
  if (!urlString.length || webState->GetBrowserState()->IsOffTheRecord())
    return;
  [self recordURL:urlString
         forTabId:TabIdTabHelper::FromWebState(webState)->tab_id()
          pending:NO];
}

// Empty method left in place in case jailbreakers are swizzling this.
- (void)detectJailbrokenDevice {
  // This method has been intentionally left blank.
}

@end

@implementation CrashReporterTabStateObserver

+ (CrashReporterTabStateObserver*)uniqueInstance {
  static CrashReporterTabStateObserver* instance =
      [[CrashReporterTabStateObserver alloc] init];
  return instance;
}

- (id)init {
  if ((self = [super init])) {
    tabCurrentStateByTabId_ = [[NSMutableDictionary alloc] init];
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)closingDocumentInTab:(NSString*)tabId {
  NSString* mime = (NSString*)[self getTabInfo:@"mime" forTab:tabId];
  if ([kDocumentMimeType isEqualToString:mime])
    breakpad_helper::SetCurrentTabIsPDF(false);
  [self removeTabInfo:@"mime" forTab:tabId];
}

- (void)setTabInfo:(NSString*)key
         withValue:(const NSString*)value
            forTab:(NSString*)tabId {
  NSMutableDictionary* tabCurrentState =
      [tabCurrentStateByTabId_ objectForKey:tabId];
  if (tabCurrentState == nil) {
    NSMutableDictionary* currentStateOfNewTab =
        [[NSMutableDictionary alloc] init];
    [tabCurrentStateByTabId_ setObject:currentStateOfNewTab forKey:tabId];
    tabCurrentState = [tabCurrentStateByTabId_ objectForKey:tabId];
  }
  [tabCurrentState setObject:value forKey:key];
}

- (id)getTabInfo:(NSString*)key forTab:(NSString*)tabId {
  NSMutableDictionary* tabValues = [tabCurrentStateByTabId_ objectForKey:tabId];
  return [tabValues objectForKey:key];
}

- (void)removeTabInfo:(NSString*)key forTab:(NSString*)tabId {
  [[tabCurrentStateByTabId_ objectForKey:tabId] removeObjectForKey:key];
}

- (void)removeTabId:(NSString*)tabId {
  [self closingDocumentInTab:tabId];
  [tabCurrentStateByTabId_ removeObjectForKey:tabId];
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

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  [self removeTabId:TabIdTabHelper::FromWebState(webState)->tab_id()];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  [self removeTabId:TabIdTabHelper::FromWebState(oldWebState)->tab_id()];
}

#pragma mark - CRWWebStateObserver protocol

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  NSString* tabID = TabIdTabHelper::FromWebState(webState)->tab_id();
  [self closingDocumentInTab:tabID];
}

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  if (!loadSuccess || webState->GetContentsMimeType() != "application/pdf")
    return;
  NSString* tabID = TabIdTabHelper::FromWebState(webState)->tab_id();
  NSString* oldMime = (NSString*)[self getTabInfo:@"mime" forTab:tabID];
  if ([kDocumentMimeType isEqualToString:oldMime])
    return;

  [self setTabInfo:@"mime" withValue:kDocumentMimeType forTab:tabID];
  breakpad_helper::SetCurrentTabIsPDF(true);
}

@end

namespace breakpad {

void MonitorURLsForWebState(web::WebState* web_state) {
  [[CrashReporterURLObserver uniqueInstance] observeWebState:web_state];
}

void StopMonitoringURLsForWebState(web::WebState* web_state) {
  [[CrashReporterURLObserver uniqueInstance] stopObservingWebState:web_state];
}

void MonitorURLsForWebStateList(WebStateList* web_state_list) {
  [[CrashReporterURLObserver uniqueInstance]
      observeWebStateList:web_state_list];
}

void StopMonitoringURLsForWebStateList(WebStateList* web_state_list) {
  [[CrashReporterURLObserver uniqueInstance]
      stopObservingWebStateList:web_state_list];
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
  CrashReporterURLObserver* observer =
      [CrashReporterURLObserver uniqueInstance];

  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    [observer removeTabId:TabIdTabHelper::FromWebState(web_state)->tab_id()];
  }
}

}  // namespace breakpad
