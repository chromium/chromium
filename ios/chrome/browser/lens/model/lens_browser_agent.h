// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_MODEL_LENS_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_LENS_MODEL_LENS_BROWSER_AGENT_H_

#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

enum class LensEntrypoint;
class Browser;

// A browser agent to help with Lens navigation.
class LensBrowserAgent : public BrowserObserver,
                         public BrowserUserData<LensBrowserAgent> {
 public:
  LensBrowserAgent(const LensBrowserAgent&) = delete;
  LensBrowserAgent& operator=(const LensBrowserAgent&) = delete;

  ~LensBrowserAgent() override;

  // Returns true if the active browser WebState's visible URL is
  // a supported Lens camera results URL. In this case, back navigation
  // should open the Lens camera experience if the user is at the
  // bottom of the navigation stack.
  bool CanGoBackToLensViewFinder() const;

  // Returns the user to the Lens camera experience if the active
  // browser WebState's visible URL is a supported Lens camera results URL.
  void GoBackToLensViewFinder() const;

 private:
  friend class BrowserUserData<LensBrowserAgent>;

  explicit LensBrowserAgent(Browser* browser);

  // BrowserObserver methods.
  void BrowserDestroyed(Browser* browser) override;

  // Returns the Lens entrypoint for the current WebState URL if it is a
  // Lens Web results URL from an enabled camera entrypoint.
  std::optional<LensEntrypoint> CurrentResultsEntrypoint() const;

  // The Browser that this agent is attached to.
  raw_ptr<Browser> browser_ = nullptr;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_LENS_MODEL_LENS_BROWSER_AGENT_H_
