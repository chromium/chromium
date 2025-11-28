// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/task/single_thread_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"

namespace {
// Placeholder will not be displayed longer than this time.
constexpr base::TimeDelta kPlaceholderMaxTimeout = base::Seconds(1.5);

// Placeholder removal will include a fade-out animation of this length.
constexpr base::TimeDelta kFadeOutAnimationDuration = base::Seconds(0.5);
}  // namespace

PagePlaceholderTabHelper::PagePlaceholderTabHelper(web::WebState* web_state)
    : web_state_(web_state), weak_factory_(this) {
  web_state_->AddObserver(this);
}

PagePlaceholderTabHelper::~PagePlaceholderTabHelper() {
  RemovePlaceholder();
}

void PagePlaceholderTabHelper::AddPlaceholderForNextNavigation() {
  if (!add_placeholder_for_next_navigation_) {
    ++placeholder_request_id_;
    cached_placeholder_image_ = nil;
    add_placeholder_for_next_navigation_ = true;
    if (web_state_->IsRealized()) {
      FetchPlaceholderIfNecessary();
    }
  }
}

void PagePlaceholderTabHelper::CancelPlaceholderForNextNavigation() {
  cached_placeholder_image_ = nil;
  placeholder_fetch_in_progress_ = false;
  add_placeholder_for_next_navigation_ = false;
  if (displaying_placeholder_) {
    RemovePlaceholder();
  }
}

void PagePlaceholderTabHelper::WasShown(web::WebState* web_state) {
  if (add_placeholder_for_next_navigation_) {
    AddPlaceholder();
  }
}

void PagePlaceholderTabHelper::WasHidden(web::WebState* web_state) {
  RemovePlaceholder();
}

void PagePlaceholderTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  if (add_placeholder_for_next_navigation_ && web_state->IsVisible()) {
    AddPlaceholder();
  }
}

void PagePlaceholderTabHelper::PageLoaded(web::WebState* web_state,
                                          web::PageLoadCompletionStatus) {
  DCHECK_EQ(web_state_, web_state);
  RemovePlaceholder();
}

void PagePlaceholderTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  RemovePlaceholder();
}

void PagePlaceholderTabHelper::OnImageRetrieved(int request_id,
                                                UIImage* image) {
  if (request_id == placeholder_request_id_) {
    placeholder_fetch_in_progress_ = false;
    if (displaying_placeholder_) {
      DisplaySnapshotImage(image);
    } else if (add_placeholder_for_next_navigation_) {
      cached_placeholder_image_ = image;
    }
  }
}

void PagePlaceholderTabHelper::AddPlaceholder() {
  // WebState::WasShown() and WebState::IsVisible() are bookkeeping mechanisms
  // that do not guarantee the WebState's view is in the view hierarchy.
  // TODO(crbug.com/40630853): Do not DCHECK([webState->GetView() window]) here
  // since this is a known issue.
  if (displaying_placeholder_ || ![web_state_->GetView() window]) {
    return;
  }

  add_placeholder_for_next_navigation_ = false;
  displaying_placeholder_ = true;

  // Update placeholder view's image and display it on top of WebState's view.
  FetchPlaceholderIfNecessary();

  // Remove placeholder if it takes too long to load the page.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PagePlaceholderTabHelper::RemovePlaceholder,
                     weak_factory_.GetWeakPtr()),
      kPlaceholderMaxTimeout);
}

void PagePlaceholderTabHelper::DisplaySnapshotImage(UIImage* snapshot) {
  UIView* web_state_view = web_state_->GetView();
  NamedGuide* guide = [NamedGuide guideWithName:kContentAreaGuide
                                           view:web_state_view];

  // TODO(crbug.com/40630853): It is a known issue that the guide may be nil,
  // causing a crash when attempting to display the placeholder. Choose to not
  // display the placeholder rather than crashing, since the placeholder is not
  // critical to the user experience.
  if (!guide) {
    displaying_placeholder_ = false;
    return;
  }

  // Lazily create the placeholder view.
  if (!placeholder_view_) {
    placeholder_view_ = [[TopAlignedImageView alloc] init];
    placeholder_view_.backgroundColor = [UIColor whiteColor];
    placeholder_view_.translatesAutoresizingMaskIntoConstraints = NO;
  }

  placeholder_view_.image = snapshot;
  [web_state_view addSubview:placeholder_view_];
  AddSameConstraints(guide, placeholder_view_);
}

void PagePlaceholderTabHelper::RemovePlaceholder() {
  if (!displaying_placeholder_) {
    return;
  }

  displaying_placeholder_ = false;

  // Remove placeholder view with a fade-out animation.
  __weak UIView* weak_placeholder_view = placeholder_view_;
  [UIView animateWithDuration:kFadeOutAnimationDuration.InSecondsF()
      animations:^{
        weak_placeholder_view.alpha = 0.0f;
      }
      completion:^(BOOL finished) {
        [weak_placeholder_view removeFromSuperview];
        weak_placeholder_view.alpha = 1.0f;
      }];
}

void PagePlaceholderTabHelper::FetchPlaceholderIfNecessary() {
  if (placeholder_fetch_in_progress_) {
    return;
  }

  if (cached_placeholder_image_) {
    OnImageRetrieved(placeholder_request_id_, cached_placeholder_image_);
    cached_placeholder_image_ = nil;
    return;
  }

  auto* snapshot_tab_helper = SnapshotTabHelper::FromWebState(web_state_);
  if (!snapshot_tab_helper) {
    return;
  }

  placeholder_fetch_in_progress_ = true;
  if (!web_state_->IsRealized() || web_state_->IsLoading()) {
    if (!base::FeatureList::IsEnabled(kRemoveGreySnapshot)) {
      snapshot_tab_helper->RetrieveGreySnapshot(base::CallbackToBlock(
          base::BindOnce(&PagePlaceholderTabHelper::OnImageRetrieved,
                         weak_factory_.GetWeakPtr(), placeholder_request_id_)));
    }
  } else {
    snapshot_tab_helper->RetrieveColorSnapshot(base::CallbackToBlock(
        base::BindOnce(&PagePlaceholderTabHelper::OnImageRetrieved,
                       weak_factory_.GetWeakPtr(), placeholder_request_id_)));
  }
}
