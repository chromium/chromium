// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_request.h"

#import <Foundation/Foundation.h>

#import "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FindInPageRequest::FindInPageRequest() {}

FindInPageRequest::~FindInPageRequest() {}

void FindInPageRequest::Reset(NSString* new_query_,
                              int new_pending_frame_call_count) {
  unique_id_++;
  selected_frame_id_ = frame_order_.end();
  selected_match_index_in_selected_frame_ = -1;
  query_ = [new_query_ copy];
  pending_frame_call_count_ = new_pending_frame_call_count;
  for (auto& pair : frame_match_count_) {
    pair.second = 0;
  }
}

int FindInPageRequest::GetTotalMatchCount() const {
  int matches = 0;
  for (auto pair : frame_match_count_) {
    matches += pair.second;
  }
  return matches;
}

int FindInPageRequest::GetRequestId() const {
  return unique_id_;
}
NSString* FindInPageRequest::GetRequestQuery() const {
  return query_;
}

bool FindInPageRequest::GoToFirstMatch() {
  for (auto frame_id = frame_order_.begin(); frame_id != frame_order_.end();
       ++frame_id) {
    if (frame_match_count_[*frame_id] > 0) {
      selected_frame_id_ = frame_id;
      selected_match_index_in_selected_frame_ = 0;
      return true;
    }
  }
  return false;
}

bool FindInPageRequest::GoToNextMatch() {
  if (GetTotalMatchCount() == 0) {
    return false;
  }
  // No currently selected match, but there are matches. Move iterator to
  // beginning. This can happen if a frame containing the currently selected
  // match is removed from the page.
  if (selected_frame_id_ == frame_order_.end()) {
    selected_frame_id_ = frame_order_.begin();
  }

  bool next_match_is_in_selected_frame =
      selected_match_index_in_selected_frame_ + 1 <
      frame_match_count_[*selected_frame_id_];
  if (next_match_is_in_selected_frame) {
    selected_match_index_in_selected_frame_++;
  } else {
    // Since the function returns early if there are no matches, an infinite
    // loop should not be a risk.
    do {
      if (selected_frame_id_ == --frame_order_.end()) {
        selected_frame_id_ = frame_order_.begin();
      } else {
        selected_frame_id_++;
      }
    } while (frame_match_count_[*selected_frame_id_] == 0);
    // Should have found new frame.
    selected_match_index_in_selected_frame_ = 0;
  }
  return true;
}

bool FindInPageRequest::GoToPreviousMatch() {
  if (GetTotalMatchCount() == 0) {
    return false;
  }
  // No currently selected match, but there are matches. Move iterator to
  // beginning. This can happen if a frame containing the currently selected
  // matchs is removed from the page.
  if (selected_frame_id_ == frame_order_.end()) {
    selected_frame_id_ = frame_order_.begin();
  }

  bool previous_match_is_in_selected_frame =
      selected_match_index_in_selected_frame_ - 1 >= 0;
  if (previous_match_is_in_selected_frame) {
    selected_match_index_in_selected_frame_--;
  } else {
    // Since the function returns early if there are no matches, an infinite
    // loop should not be a risk.
    do {
      if (selected_frame_id_ == frame_order_.begin()) {
        selected_frame_id_ = --frame_order_.end();
      } else {
        selected_frame_id_--;
      }
    } while (frame_match_count_[*selected_frame_id_] == 0);
    // Should have found new frame.
    selected_match_index_in_selected_frame_ =
        frame_match_count_[*selected_frame_id_] - 1;
  }
  return true;
}

int FindInPageRequest::GetMatchCountForFrame(const std::string& frame_id) {
  if (frame_match_count_.find(frame_id) == frame_match_count_.end()) {
    return -1;
  }
  return frame_match_count_[frame_id];
}

void FindInPageRequest::SetMatchCountForFrame(int match_count,
                                              const std::string& frame_id) {
  frame_match_count_[frame_id] = match_count;
}

int FindInPageRequest::GetMatchCountForSelectedFrame() {
  if (selected_frame_id_ == frame_order_.end()) {
    return -1;
  }
  return frame_match_count_[*selected_frame_id_];
}

void FindInPageRequest::SetMatchCountForSelectedFrame(int match_count) {
  if (selected_frame_id_ == frame_order_.end()) {
    return;
  }
  frame_match_count_[*selected_frame_id_] = match_count;
}

int FindInPageRequest::GetCurrentSelectedMatchPageIndex() {
  if (selected_match_index_in_selected_frame_ == -1) {
    return -1;
  }
  // Count all matches in frames that come before frame with id
  // |selected_frame_id|.
  int total_match_index = selected_match_index_in_selected_frame_;
  for (auto it = frame_order_.begin(); it != selected_frame_id_; ++it) {
    total_match_index += frame_match_count_[*it];
  }
  return total_match_index;
}

std::string FindInPageRequest::GetSelectedFrameId() {
  if (selected_frame_id_ == frame_order_.end()) {
    return std::string();
  }
  return *selected_frame_id_;
}

int FindInPageRequest::GetCurrentSelectedMatchFrameIndex() const {
  return selected_match_index_in_selected_frame_;
}

void FindInPageRequest::SetCurrentSelectedMatchFrameIndex(int index) {
  selected_match_index_in_selected_frame_ = index;
}

void FindInPageRequest::RemoveFrame(const std::string& frame_id) {
  if (IsSelectedFrame(frame_id)) {
    // If currently selecting match in frame that will become unavailable,
    // there will no longer be a selected match. Reset to unselected match
    // state.
    selected_frame_id_ = frame_order_.end();
    selected_match_index_in_selected_frame_ = -1;
  }
  frame_order_.remove(frame_id);
  frame_match_count_.erase(frame_id);
}

void FindInPageRequest::AddFrame(WebFrame* web_frame) {
  frame_match_count_[web_frame->GetFrameId()] = 0;
  if (web_frame->IsMainFrame()) {
    // Main frame matches should show up first.
    frame_order_.push_front(web_frame->GetFrameId());
  } else {
    // The order of iframes is not important.
    frame_order_.push_back(web_frame->GetFrameId());
  }
}

void FindInPageRequest::DidReceiveFindResponseFromOneFrame() {
  pending_frame_call_count_--;
}

bool FindInPageRequest::AreAllFindResponsesReturned() {
  return pending_frame_call_count_ == 0;
}

bool FindInPageRequest::IsSelectedFrame(const std::string& frame_id) {
  if (selected_frame_id_ == frame_order_.end()) {
    return false;
  }
  return *selected_frame_id_ == frame_id;
}

}  // namespace web
