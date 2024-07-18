// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_ALL_WEB_STATE_LIST_OBSERVATION_REGISTRAR_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_ALL_WEB_STATE_LIST_OBSERVATION_REGISTRAR_H_

#include <memory>

#import "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class BrowserList;

// AllWebStateListObservationRegistrar tracks when Browsers are created and
// destroyed for a given BrowserList. Whenever the BrowserList changes,
// AllWebStateListObservationRegistrar registers (or unregisters) a provided
// observer as a WebStateListObserver.
class AllWebStateListObservationRegistrar : public BrowserListObserver {
 public:
  // Observation mode optionally used for constructors.
  enum class Mode {
    REGULAR,    // Only register regular web states.
    INCOGNITO,  // Only register incognito web states.
    ALL,        // Register all web states.
  };
  // Constructs an object that register the given `web_state_list_observer` as
  // WebStateListObserver for any regular or OTR Browsers associated with
  // `browser_list` according to `mode`.
  // Keeps observer registration up to date as Browsers are added and removed
  // from `browser_list`.
  AllWebStateListObservationRegistrar(
      BrowserList* browser_list,
      std::unique_ptr<WebStateListObserver> web_state_list_observer,
      Mode mode);

  // Convenience constructor; creates a registrar as described above, with a
  // `mode` of ALL.
  AllWebStateListObservationRegistrar(
      BrowserList* browser_list,
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
  void OnBrowserRemoved(const BrowserList* browser_list,
                        Browser* browser) override;
  void OnBrowserListShutdown(BrowserList* browser_list) override;

 private:
  raw_ptr<BrowserList> browser_list_;
  std::unique_ptr<WebStateListObserver> web_state_list_observer_;
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      scoped_observations_;
  Mode mode_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_ALL_WEB_STATE_LIST_OBSERVATION_REGISTRAR_H_
