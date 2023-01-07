// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_ALL_WEB_STATE_LIST_OBSERVATION_REGISTRAR_H_
#define IOS_CHROME_BROWSER_MAIN_ALL_WEB_STATE_LIST_OBSERVATION_REGISTRAR_H_

#include <memory>

#include "base/scoped_multi_source_observation.h"
#include "ios/chrome/browser/main/browser_list_observer.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class BrowserList;
class ChromeBrowserState;

// AllWebStateListObservationRegistrar tracks when Browsers are created and
// destroyed for a given ChromeBrowserState. Whenever the BrowserList changes,
// AllWebStateListObservationRegistrar registers (or unregisters) a provided
// observer as a WebStateListObserver.
class AllWebStateListObservationRegistrar : public BrowserListObserver {
 public:
  // Observation mode optionally used for constructors.
  enum Mode {
    REGULAR = 1 << 0,          // Only register regular web states.
    INCOGNITO = 1 << 1,        // Only register incognito web states.
    ALL = REGULAR | INCOGNITO  // Register all web states.
  };
  // Constructs an object that registers the given `web_state_list_observer` as
  // a WebStateListObserver for any Browsers associated with `browser_state` or
  // `browser_state`'s OTR browser state, according to the value of `mode`.
  // Keeps observer registration up to date as Browsers are added and
  // removed from `browser_state`'s BrowserList.
  AllWebStateListObservationRegistrar(
      ChromeBrowserState* browser_state,
      std::unique_ptr<WebStateListObserver> web_state_list_observer,
      Mode mode);

  // Convenience constructor; creates a registrar as described above, with a
  // `mode` of ALL.
  AllWebStateListObservationRegistrar(
      ChromeBrowserState* browser_state,
      std::unique_ptr<WebStateListObserver> web_state_list_observer);

  // Not copyable or moveable
  AllWebStateListObservationRegistrar(
      const AllWebStateListObservationRegistrar&) = delete;
  AllWebStateListObservationRegistrar& operator=(
      const AllWebStateListObservationRegistrar&) = delete;

  ~AllWebStateListObservationRegistrar() override;

  // BrowserListObserver
  void OnBrowserAdded(const BrowserList* browser_list,
                      Browser* browser) override;
  void OnIncognitoBrowserAdded(const BrowserList* browser_list,
                               Browser* browser) override;
  void OnBrowserRemoved(const BrowserList* browser_list,
                        Browser* browser) override;
  void OnIncognitoBrowserRemoved(const BrowserList* browser_list,
                                 Browser* browser) override;
  void OnBrowserListShutdown(BrowserList* browser_list) override;

 private:
  BrowserList* browser_list_;
  std::unique_ptr<WebStateListObserver> web_state_list_observer_;
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      scoped_observations_;
  Mode mode_;
};

#endif  // IOS_CHROME_BROWSER_MAIN_ALL_WEB_STATE_LIST_OBSERVATION_REGISTRAR_H_
