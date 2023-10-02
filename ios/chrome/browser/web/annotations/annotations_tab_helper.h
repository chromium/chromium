// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/values.h"
#import "ios/web/public/annotations/annotations_text_observer.h"
#import "ios/web/public/annotations/custom_text_checking_result.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

@protocol CRWWebViewHandlerDelegate;
@protocol MiniMapCommands;
@protocol ParcelTrackingOptInCommands;
@class UIViewController;

namespace web {
class WebState;
}  // namespace web

/**
 * Class in charge of annotations in text.
 */
class AnnotationsTabHelper : public web::AnnotationsTextObserver,
                             public web::WebStateUserData<AnnotationsTabHelper>,
                             public web::WebStateObserver {
 public:
  ~AnnotationsTabHelper() override;

  // Sets the BaseViewController from which to present UI.
  void SetBaseViewController(UIViewController* base_view_controller);

  // Sets the MiniMapCommands that can display mini maps.
  void SetMiniMapCommands(id<MiniMapCommands> mini_map_handler);

  // Sets the ParcelTrackingOptInCommands that can display the parcel tracking
  // opt-in prompt.
  void SetParcelTrackingOptInCommands(
      id<ParcelTrackingOptInCommands> parcel_tracking_handler);

  // Returns pointer to latest metadata extracted or `nullptr`. See
  // i/w/p/a/annotations_text_observer.h for metadata key/pair values.
  base::Value::Dict* GetMetadata() { return metadata_.get(); }

  // AnnotationsTextObserver methods:
  void OnTextExtracted(web::WebState* web_state,
                       const std::string& text,
                       int seq_id,
                       const base::Value::Dict& metadata) override;
  void OnDecorated(web::WebState* web_state,
                   int successes,
                   int annotations) override;
  void OnClick(web::WebState* web_state,
               const std::string& text,
               CGRect rect,
               const std::string& data) override;

  // WebStateObserver methods:
  void WebStateDestroyed(web::WebState* web_state) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;

  WEB_STATE_USER_DATA_KEY_DECL();

 private:
  friend class WebStateUserData<AnnotationsTabHelper>;

  explicit AnnotationsTabHelper(web::WebState* web_state);

  // Receiver for text classifier extracted intents. Must run on main thread.
  // `seq_id` comes from `OnTextExtracted` and is meant to be passed on to
  // `AnnotationsTextManager::DecorateAnnotations` to validate decorations
  // against the text extracted.
  void ApplyDeferredProcessing(int seq_id,
                               absl::optional<base::Value> deferred);

  // Triggers the parcel tracking UI display if the given list of annotations
  // contains at least one parcel number and the user is eligible for the
  // prompt. Removes parcels from `annotations_list`.
  void ProcessParcelTrackingNumbers(base::Value::List& annotations_list);

  // Triggers the parcel tracking UI display for the given parcel
  // list `parcels`.
  void MaybeShowParcelTrackingUI(NSArray<CustomTextCheckingResult*>* parcels);

  // Puts annotations data in `match_cache_` and replaces it with a uuid key
  // to be passed to JS and expect back in `OnClick`.
  void BuildCache(base::Value::List& annotations_list);

  UIViewController* base_view_controller_ = nil;

  id<MiniMapCommands> mini_map_handler_ = nil;

  id<ParcelTrackingOptInCommands> parcel_tracking_handler_ = nil;

  web::WebState* web_state_ = nullptr;

  std::unique_ptr<base::Value::Dict> metadata_;

  std::map<std::string, std::string> match_cache_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last member to ensure it is destroyed last.
  base::WeakPtrFactory<AnnotationsTabHelper> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_TAB_HELPER_H_
