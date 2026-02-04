// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Data for an accessibility event and/or an atomic change to an accessibility
// tree. See ui/accessibility/ax_tree_update.h for an extended explanation of
// the tree update format.
[nocompile] dictionary AXEventParams {
  // The tree id of the web contents that this update is for.
  required DOMString treeID;

  // ID of the node that the event applies to.
  required long targetID;

  // The type of event that this update represents.
  required DOMString eventType;

  // The source of this event.
  required DOMString eventFrom;

  // The mouse coordinates when this event fired.
  required double mouseX;
  required double mouseY;

  // ID of an action request resulting in this event.
  required long actionRequestID;
};

dictionary AXTextLocationParams {
  required DOMString treeID;
  required long nodeID;
  required boolean result;
  required long left;
  required long top;
  required long width;
  required long height;
  required long requestID;
};

// Arguments required for all actions supplied to performAction.
dictionary PerformActionRequiredParams {
  required DOMString treeID;
  required long automationNodeID;

  // This can be either automation::ActionType or
  // automation_internal::ActionTypePrivate.
  required DOMString actionType;

  long requestID;
};

// Arguments for the customAction action. Those args are passed to
// performAction as opt_args.
dictionary PerformCustomActionParams {
  required long customActionID;
};

// Arguments for the setSelection action supplied to performAction.
dictionary SetSelectionParams {
  // Reuses ActionRequiredParams automationNodeID to mean anchor node id,
  // and treeID to apply to both anchor and focus node ids.
  required long focusNodeID;
  required long anchorOffset;
  required long focusOffset;
};

// Arguments for the replaceSelectedText action supplied to performAction.
dictionary ReplaceSelectedTextParams {
  required DOMString value;
};

// Arguments for the setValue action supplied to performAction.
dictionary SetValueParams {
  required DOMString value;
};

// Arguments for the scrollToPoint action supplied to performAction.
dictionary ScrollToPointParams {
  required long x;
  required long y;
};

// Arguments for the scrollToPositionAtRowColumn action supplied to
// performAction.
dictionary ScrollToPositionAtRowColumnParams {
  required long row;
  required long column;
};

// Arguments for the SetScrollOffset action supplied to performAction.
dictionary SetScrollOffsetParams {
  required long x;
  required long y;
};

// Arguments for the getImageData action.
dictionary GetImageDataParams {
  required long maxWidth;
  required long maxHeight;
};

// Arguments for the hitTest action.
dictionary HitTestParams {
  required long x;
  required long y;
  required DOMString eventToFire;
};

// Arguments for getTextLocation action.
dictionary GetTextLocationDataParams {
  required long startIndex;
  required long endIndex;
};

// Listener callback for the onAccessibilityEvent event.
callback OnAccessibilityEventListener = undefined(AXEventParams update);

interface OnAccessibilityEventEvent : ExtensionEvent {
  static undefined addListener(OnAccessibilityEventListener listener);
  static undefined removeListener(OnAccessibilityEventListener listener);
  static boolean hasListener(OnAccessibilityEventListener listener);
};

// Listener callback for the onAccessibilityTreeDestroyed event.
callback OnAccessibilityTreeDestroyedListener = undefined(DOMString treeID);

interface OnAccessibilityTreeDestroyedEvent : ExtensionEvent {
  static undefined addListener(OnAccessibilityTreeDestroyedListener listener);
  static undefined removeListener(
      OnAccessibilityTreeDestroyedListener listener);
  static boolean hasListener(OnAccessibilityTreeDestroyedListener listener);
};

// Listener callback for the onGetTextLocationResult event.
callback OnGetTextLocationResultListener =
    undefined(AXTextLocationParams params);

interface OnGetTextLocationResultEvent : ExtensionEvent {
  static undefined addListener(OnGetTextLocationResultListener listener);
  static undefined removeListener(OnGetTextLocationResultListener listener);
  static boolean hasListener(OnGetTextLocationResultListener listener);
};

// Listener callback for the onTreeChange event.
callback OnTreeChangeListener = undefined(long observerID,
                                          DOMString treeID,
                                          long nodeID,
                                          DOMString changeType);

interface OnTreeChangeEvent : ExtensionEvent {
  static undefined addListener(OnTreeChangeListener listener);
  static undefined removeListener(OnTreeChangeListener listener);
  static boolean hasListener(OnTreeChangeListener listener);
};

// Listener callback for the onChildTreeID event.
callback OnChildTreeIDListener = undefined(DOMString treeID);

interface OnChildTreeIDEvent : ExtensionEvent {
  static undefined addListener(OnChildTreeIDListener listener);
  static undefined removeListener(OnChildTreeIDListener listener);
  static boolean hasListener(OnChildTreeIDListener listener);
};

// Listener callback for the onNodesRemoved event.
callback OnNodesRemovedListener =
    undefined(DOMString treeID, sequence<long> nodeIDs);

interface OnNodesRemovedEvent : ExtensionEvent {
  static undefined addListener(OnNodesRemovedListener listener);
  static undefined removeListener(OnNodesRemovedListener listener);
  static boolean hasListener(OnNodesRemovedListener listener);
};

// Listener callback for the onAccessibilityTreeSerializationError event.
callback OnAccessibilityTreeSerializationErrorListener =
    undefined(DOMString treeID);

interface OnAccessibilityTreeSerializationErrorEvent : ExtensionEvent {
  static undefined addListener(
      OnAccessibilityTreeSerializationErrorListener listener);
  static undefined removeListener(
      OnAccessibilityTreeSerializationErrorListener listener);
  static boolean hasListener(
      OnAccessibilityTreeSerializationErrorListener listener);
};

// Listener callback for the onActionResult event.
callback OnActionResultListener =
    undefined(DOMString treeID, long requestID, boolean result);

interface OnActionResultEvent : ExtensionEvent {
  static undefined addListener(OnActionResultListener listener);
  static undefined removeListener(OnActionResultListener listener);
  static boolean hasListener(OnActionResultListener listener);
};

// Listener callback for the onAllAutomationEventListenersRemoved event.
callback OnAllAutomationEventListenersRemovedListener = undefined();

interface OnAllAutomationEventListenersRemovedEvent : ExtensionEvent {
  static undefined addListener(
      OnAllAutomationEventListenersRemovedListener listener);
  static undefined removeListener(
      OnAllAutomationEventListenersRemovedListener listener);
  static boolean hasListener(
      OnAllAutomationEventListenersRemovedListener listener);
};

// This is the implementation layer of the chrome.automation API, and is
// essentially a translation of the internal accessibility tree update system
// into an extension API.
interface AutomationInternal {
  // Enable automation of the tree with the given id.
  static undefined enableTree(DOMString tree_id);

  // Enables desktop automation.
  // |Returns|: Callback called when enableDesktop() returns. Returns the
  // accessibility tree id of the desktop tree.
  // |PromiseValue|: tree_id
  [requiredCallback] static Promise<DOMString> enableDesktop();

  // Disables desktop automation.
  // |Returns|: Callback called when disableDesktop() returns. It is safe to
  // clear accessibility api state at that point.
  [requiredCallback] static Promise<undefined> disableDesktop();

  // Performs an action on an automation node.
  static undefined performAction(PerformActionRequiredParams args,
                                 object opt_args);

  // Fired when an accessibility event occurs
  [nocompile] static attribute OnAccessibilityEventEvent onAccessibilityEvent;

  static attribute OnAccessibilityTreeDestroyedEvent
      onAccessibilityTreeDestroyed;

  static attribute OnGetTextLocationResultEvent onGetTextLocationResult;

  static attribute OnTreeChangeEvent onTreeChange;

  static attribute OnChildTreeIDEvent onChildTreeID;

  static attribute OnNodesRemovedEvent onNodesRemoved;

  static attribute OnAccessibilityTreeSerializationErrorEvent
      onAccessibilityTreeSerializationError;

  static attribute OnActionResultEvent onActionResult;

  static attribute OnAllAutomationEventListenersRemovedEvent
      onAllAutomationEventListenersRemoved;
};

partial interface Browser {
  static attribute AutomationInternal automationInternal;
};
