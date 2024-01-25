// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_BROWSER_AGENT_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/follow/model/follow_service.h"
#include "ios/chrome/browser/follow/model/follow_service_observer.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"

@class FeedMetricsRecorder;

@protocol NewTabPageCommands;
@protocol SnackbarCommands;
@protocol FeedCommands;

// Manages the interaction with the FollowService for a Browser.
class FollowBrowserAgent final : public BrowserUserData<FollowBrowserAgent> {
 public:
  // FollowBrowserAgent uses the same observation API as FollowService.
  using Observer = FollowServiceObserver;

  FollowBrowserAgent(const FollowBrowserAgent&) = delete;
  FollowBrowserAgent& operator=(const FollowBrowserAgent&) = delete;

  ~FollowBrowserAgent() final;

  // Returns if a followed website corresponds to `web_page_urls`.
  bool IsWebSiteFollowed(WebPageURLs* web_page_urls);

  // If a recommended website corresponds to `web_page_urls`, returns
  // the URL identifier for the website. Returns nil otherwise
  NSURL* GetRecommendedSiteURL(WebPageURLs* web_page_urls);

  // Returns a list of all followed websites.
  NSArray<FollowedWebSite*>* GetFollowedWebSites();

  // Loads all followed websites.
  void LoadFollowedWebSites();

  // Follows the website associated with `web_page_urls` and presents
  // the UI (snackback, ...) when the operation completes. Nothing
  // is presented if the web channel is already followed.
  void FollowWebSite(WebPageURLs* web_page_urls, FollowSource source);

  // Unfollows the website associated with `web_page_urls` and presents
  // the UI (snackback, ...) when the operation completes. Nothing is
  // presented if the web channel is not followed.
  void UnfollowWebSite(WebPageURLs* web_page_urls, FollowSource source);

  // Sets the UI providers.
  void SetUIProviders(id<NewTabPageCommands> new_tab_page_commands,
                      id<SnackbarCommands> snack_bar_commands,
                      id<FeedCommands> feed_commands);

  // Clears the UI providers.
  void ClearUIProviders();

  // Adds/Removes `observer`.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  using MessageBlock = void (^)(void);
  using CompletionBlock = void (^)(BOOL success);

  friend class BrowserUserData<FollowBrowserAgent>;

  base::WeakPtr<FollowBrowserAgent> AsWeakPtr();

  explicit FollowBrowserAgent(Browser* browser);

  // Show an overlay message at the bottom of the screen with action button.
  // Invoked after a follow/unfollow request completes.
  void ShowOverlayMessage(FollowSource source,
                          NSString* message,
                          NSString* button_text,
                          MessageBlock message_action,
                          CompletionBlock completion_action);

  // Helper method that shows an overlay message at the bottom of the screen on
  // invocation.
  void ShowOverlayMessageHelper(NSString* message,
                                NSString* button_text,
                                MessageBlock message_action,
                                CompletionBlock completion_action);

  // Invoked when a follow request completes.
  void OnFollowResponse(WebPageURLs* web_page_urls,
                        FollowSource source,
                        FollowResult result,
                        FollowedWebSite* web_site);

  // Invoked when an unfollow request completes.
  void OnUnfollowResponse(WebPageURLs* web_page_urls,
                          FollowSource source,
                          FollowResult result,
                          FollowedWebSite* web_site);

  // Helper methods to handle follow/unfollow successful/failed requests.
  void OnFollowSuccess(WebPageURLs* web_page_urls,
                       FollowSource source,
                       FollowedWebSite* web_site);
  void OnFollowFailure(WebPageURLs* web_page_urls,
                       FollowSource source,
                       FollowedWebSite* web_site);
  void OnUnfollowSuccess(WebPageURLs* web_page_urls,
                         FollowSource source,
                         FollowedWebSite* web_site);
  void OnUnfollowFailure(WebPageURLs* web_page_urls,
                         FollowSource source,
                         FollowedWebSite* web_site);

  // Helper method to lazy initiate variables.
  raw_ptr<FollowService> GetFollowService();
  FeedMetricsRecorder* GetMetricsRecorder();

  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<FollowService> service_ = nullptr;

  __weak id<NewTabPageCommands> new_tab_page_commands_;
  __weak id<SnackbarCommands> snack_bar_commands_;
  __weak id<FeedCommands> feed_commands_;
  __weak FeedMetricsRecorder* metrics_recorder_;

  base::WeakPtrFactory<FollowBrowserAgent> weak_ptr_factory_{this};

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_BROWSER_AGENT_H_
