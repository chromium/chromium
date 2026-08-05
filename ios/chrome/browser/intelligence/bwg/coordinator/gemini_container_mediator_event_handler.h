// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_EVENT_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_EVENT_HANDLER_H_

#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"

// Interface for handling events forwarded by GeminiContainerMediator.
// NOTE: This interface is only to be used by GeminiBrowserAgent until the end
// of the migration and will be removed afterwards.
class GeminiContainerMediatorEventHandler {
 public:
  virtual ~GeminiContainerMediatorEventHandler() = default;

  // Called when the Gemini view state changes.
  virtual void OnViewStateChanged(
      ios::provider::GeminiViewState view_state) = 0;

  // Called when the Gemini processing status updates.
  virtual void OnProcessingStatusChanged(
      ios::provider::GeminiClientMode processing_status,
      ios::provider::GeminiDormantReason dormant_reason) = 0;

  // Collapses floaty if invoked.
  virtual void CollapseFloatyIfInvoked() = 0;

  // Records the most recently presented state of the Gemini view to inform
  // future interactions.
  virtual void SetLastShownViewState(
      ios::provider::GeminiViewState view_state) = 0;

  // Called when the user taps the Live button.
  virtual void OnLiveButtonTapped() = 0;

  // Called when the user presses the Live stop button.
  virtual void OnGeminiLiveUserDidPressStopButton() = 0;

  // Called when the user barges in during Gemini Live session.
  virtual void OnGeminiLiveUserDidBargeIn() = 0;

  // Called when the Gemini view mode changes.
  virtual void OnModeChanged(ios::provider::GeminiViewMode mode) = 0;

  // Called when the Gemini UI did appear.
  virtual void OnGeminiUIDidAppear() = 0;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_EVENT_HANDLER_H_
