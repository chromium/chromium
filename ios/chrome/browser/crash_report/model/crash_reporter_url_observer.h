// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORTER_URL_OBSERVER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORTER_URL_OBSERVER_H_

#import <Foundation/Foundation.h>

#include <map>
#include <string>

#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"
#include "url/gurl.h"

namespace web {
class NavigationContext;
class WebState;
}  // namespace web
class AllWebStateObservationForwarder;

// Provides method to set and remove parameter values.
@protocol CrashReporterParameterSetter
// Sets a parameter named `key||pending` with value `value`.
- (void)setReportParameterURL:(const GURL&)URL
                       forKey:(NSNumber*)key
                      pending:(BOOL)pending;

// Deletes the parameter `key||pending`.
- (void)removeReportParameter:(NSNumber*)key pending:(BOOL)pending;
@end

// WebStateListObserver that allows loaded urls to be sent to the crash server.
// The different webStates are bundled in groups. Each group reports at most
// 1 URL.
class CrashReporterURLObserver : public WebStateListObserver,
                                 public web::WebStateObserver {
 public:
  CrashReporterURLObserver(id<CrashReporterParameterSetter> params_setter);
  ~CrashReporterURLObserver() override;
  static CrashReporterURLObserver* GetSharedInstance();

  // Removes the URL reported for the group (if any).
  void RemoveWebStateList(WebStateList* web_state_list);

  // Records the given URL associated to the given id to the list of URLs to
  // send to the crash server. If `pending` is true, the URL is one that is
  // expected to start loading, but hasn't actually been seen yet.
  void RecordURL(const GURL& url, const web::WebState* web_state, bool pending);

  // Observes `webState` by this instance of the CrashReporterURLObserver.
  void ObservePreloadWebState(web::WebState* web_state);
  // Stop Observing `webState` by this instance of the CrashReporterURLObserver.
  void StopObservingPreloadWebState(web::WebState* web_state);
  // Observes `webStateList` by this instance of the CrashReporterURLObserver.
  void ObserveWebStateList(WebStateList* web_state_list);
  // Stop Observing `webStateList` by this instance of the
  // CrashReporterURLObserver.
  void StopObservingWebStateList(WebStateList* web_state_list);

  // WebStateObserver
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  // WebStateListObserver
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

 private:
  // A unique string identifying `web_state_list` as a group of WebStates.
  std::string GroupForWebStateList(WebStateList* web_state_list);
  // Convenient method to report the URL displayed by `web_state`.
  void RecordURLForWebState(const web::WebState* web_state);
  // Remove the reporting of URLs of every WebStates in `group`.
  void RemoveGroup(const std::string& group);
  // Map associating each crash key with the group it is currently reporting
  // URLs for.
  NSMutableDictionary<NSString*, NSNumber*>* crash_key_by_group_;
  // Map associating each WebState to its group.
  std::map<const web::WebState*, std::string> web_state_to_group_;
  // Map associating each group to the WebState currently reported in crash
  // reports.
  std::map<std::string, const web::WebState*> current_web_states_;
  // List of keys to use for recording URLs. This list is sorted such that a new
  // tab must use the first key in this list to record its URLs.
  NSMutableArray<NSNumber*>* crash_keys_;
  // Forwards observer methods for all WebStates in the WebStateList monitored
  // by the CrashReporterURLObserver.
  std::map<WebStateList*, std::unique_ptr<AllWebStateObservationForwarder>>
      all_web_state_observation_forwarders_;
  // The object responsible for forwarding the urls to the crash reporter.
  id<CrashReporterParameterSetter> params_setter_;
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORTER_URL_OBSERVER_H_
