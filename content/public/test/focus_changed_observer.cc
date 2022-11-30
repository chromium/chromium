// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/focus_changed_observer.h"

namespace content {

FocusChangedObserver::FocusChangedObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  DCHECK(web_contents);
}

FocusChangedObserver::~FocusChangedObserver() = default;

FocusedNodeDetails FocusChangedObserver::Wait() {
  run_loop_.Run();
  DCHECK(observed_details_.has_value());
  return *observed_details_;
}

void FocusChangedObserver::OnFocusChangedInPage(FocusedNodeDetails* details) {
  DCHECK(details);
  observed_details_ = *details;
  run_loop_.Quit();
}

}  // namespace content
