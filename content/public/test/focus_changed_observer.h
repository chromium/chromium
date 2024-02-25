// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FOCUS_CHANGED_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_FOCUS_CHANGED_OBSERVER_H_

#include "base/run_loop.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// Used in tests to wait for focus changes in a WebContents.
class FocusChangedObserver : public WebContentsObserver {
 public:
  explicit FocusChangedObserver(WebContents*);

  FocusChangedObserver(const FocusChangedObserver&) = delete;
  FocusChangedObserver& operator=(const FocusChangedObserver&) = delete;

  ~FocusChangedObserver() override;

  // Waits until focus changes in the page. Returns the observed details.
  FocusedNodeDetails Wait();

 private:
  void OnFocusChangedInPage(FocusedNodeDetails*) override;

  base::RunLoop run_loop_;
  std::optional<FocusedNodeDetails> observed_details_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FOCUS_CHANGED_OBSERVER_H_
