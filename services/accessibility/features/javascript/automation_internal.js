// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The ATP implementation of the AutomationInternal API.
// In the extensions implementation, this is achieved by using extensions event
// dispatching system, where here a direct communication between c++ and js
// happens:
// * js creates the event listeners using chrome.event;
// * c++, when receiving an accessibility event, dispatches them directly using
// the helper function `getEventListenersFromName`.
class AtpAutomationInternal {
  constructor() {
    this.onChildTreeID = new ChromeEvent(null);
    this.onTreeChange = new ChromeEvent(null);
    this.onNodesRemoved = new ChromeEvent(null);
    this.onAllAutomationEventListenersRemoved = new ChromeEvent(null);
    this.onAccessibilityEvent = new ChromeEvent(null);
    this.onAccessibilityTreeDestroyed = new ChromeEvent(null);
    this.onAccessibilityTreeSerializationError = new ChromeEvent(null);
    this.onActionResult = new ChromeEvent(null);
    this.onGetTextLocationResult = new ChromeEvent(null);
  }
}

// Shim the AtpAutomationInternal onto the automationInternal variable to
// achieve the same behavior as in extensions.
automationInternal = new AtpAutomationInternal();


// Helper function that is called from c++ to find the correct event listener to
// dispatch some event.
function automationInternalV8Listeners(event_name) {
  if (event_name == 'automationInternal.onChildTreeID') {
    return automationInternal.onChildTreeID;
  }
  if (event_name == 'automationInternal.onTreeChange') {
    return automationInternal.onTreeChange;
  }
  if (event_name == 'automationInternal.onNodesRemoved') {
    return automationInternal.onNodesRemoved;
  }
  if (event_name == 'automationInternal.onAllAutomationEventListenersRemoved') {
    return automationInternal.onAllAutomationEventListenersRemoved;
  }
  if (event_name == 'automationInternal.onAccessibilityEvent') {
    return automationInternal.onAccessibilityEvent;
  }
  if (event_name == 'automationInternal.onAccessibilityTreeDestroyed') {
    return automationInternal.onAccessibilityTreeDestroyed;
  }
  if (event_name ==
      'automationInternal.onAccessibilityTreeSerializationError') {
    return automationInternal.onAccessibilityTreeSerializationError;
  }
  if (event_name == 'automationInternal.onActionResult') {
    return automationInternal.onActionResult;
  }
  if (event_name == 'automationInternal.onGetTextLocationResult') {
    return automationInternal.onGetTextLocationResult;
  }
  return null;
}
