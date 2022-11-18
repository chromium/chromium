// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_DECIDER_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_DECIDER_H_

#include "content/browser/preloading/prerenderer.h"
#include "content/public/browser/anchor_element_preconnect_delegate.h"
#include "content/public/browser/document_user_data.h"
#include "url/gurl.h"

namespace content {

class AnchorElementPreconnectDelegate;

class PreloadingDeciderObserverForTesting {
 public:
  virtual ~PreloadingDeciderObserverForTesting() = default;

  virtual void UpdateSpeculationCandidates(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) = 0;
  virtual void OnPointerDown(const GURL& url) = 0;
  virtual void OnPointerHover(const GURL& url) = 0;
};

// Processes user interaction events and developer provided speculation-rules
// and based on some heuristics decides which preloading actions are safe and
// worth executing.
// TODO(isaboori): implement the preloading link selection heuristics logic
class CONTENT_EXPORT PreloadingDecider
    : public DocumentUserData<PreloadingDecider> {
 public:
  ~PreloadingDecider() override;

  // Receives and processes on pointer down event for 'url' target link.
  void OnPointerDown(const GURL& url);

  // Receives and processes on pointer hover event for 'url' target link.
  void OnPointerHover(const GURL& url);

  void UpdateSpeculationCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

  // Set the preloading decider observer  for testing
  void SetObserverForTesting(
      std::unique_ptr<PreloadingDeciderObserverForTesting> observer);

  // Reset the preloading decider observer  for testing
  void ResetObserverForTesting();

 private:
  explicit PreloadingDecider(RenderFrameHost* rfh);
  friend class DocumentUserData<PreloadingDecider>;
  DOCUMENT_USER_DATA_KEY_DECL();

  std::unique_ptr<AnchorElementPreconnectDelegate> preconnect_delegate_;
  std::unique_ptr<PreloadingDeciderObserverForTesting> observer_for_testing_;
  Prerenderer prerenderer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_DECIDER_H_
