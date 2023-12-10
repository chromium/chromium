// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the automation API.
const AutomationRootNode = require('automationNode').AutomationRootNode;
const automationInternal = getInternalApi('automationInternal');
const exceptionHandler = require('uncaught_exception_handler');
const logging = requireNative('logging');
const nativeAutomationInternal = requireNative('automationInternal');
const DestroyAccessibilityTree =
    nativeAutomationInternal.DestroyAccessibilityTree;
const StartCachingAccessibilityTrees =
    nativeAutomationInternal.StartCachingAccessibilityTrees;
const StopCachingAccessibilityTrees =
    nativeAutomationInternal.StopCachingAccessibilityTrees;
const AddTreeChangeObserver = nativeAutomationInternal.AddTreeChangeObserver;
const RemoveTreeChangeObserver =
    nativeAutomationInternal.RemoveTreeChangeObserver;
const GetFocusNative = nativeAutomationInternal.GetFocus;
const GetAccessibilityFocusNative =
    nativeAutomationInternal.GetAccessibilityFocus;
const SetDesktopID = nativeAutomationInternal.SetDesktopID;

// A namespace to export utility functions to other files in automation.
const automationUtil = function() {};

// TODO(aboxhall): Look into using WeakMap
let idToCallback = {};

let desktopId;
let desktopTree;

automationUtil.storeTreeCallback = function(id, callback) {
  if (!callback) {
    return;
  }

  const targetTree = AutomationRootNode.get(id);
  if (!targetTree) {
    // If we haven't cached the tree, hold the callback until the tree is
    // populated by the initial onAccessibilityEvent call.
    if (id in idToCallback) {
      idToCallback[id].push(callback);
    } else {
      idToCallback[id] = [callback];
    }
  } else {
    callback(targetTree);
  }
};

/**
 * Global list of tree change observers.
 * @type {Object<number, TreeChangeObserver>}
 */
automationUtil.treeChangeObserverMap = {};

/**
 * The id of the next tree change observer.
 * @type {number}
 */
automationUtil.nextTreeChangeObserverId = 1;

apiBridge.registerCustomHook(function(bindingsAPI) {
  const apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('getDesktop', function(callback) {
    StartCachingAccessibilityTrees();
    if (desktopId !== undefined) {
      desktopTree = AutomationRootNode.get(desktopId);
    }
    if (!desktopTree) {
      automationInternal.enableDesktop(function(treeId) {
        if (bindingUtil.hasLastError()) {
          AutomationRootNode.destroy(treeId);
          desktopId = undefined;
          SetDesktopID('');
          callback();
          return;
        }
        desktopId = treeId;
        SetDesktopID(desktopId);
        desktopTree = AutomationRootNode.getOrCreate(desktopId);
        callback(desktopTree);

        // TODO(dtseng): Disable desktop tree once desktop object goes out of
        // scope.
      });
    } else {
      callback(desktopTree);
    }
  });

  apiFunctions.setHandleRequest('getFocus', function(callback) {
    const focusedNodeInfo = GetFocusNative();
    if (!focusedNodeInfo) {
      callback(null);
      return;
    }
    const tree = AutomationRootNode.getOrCreate(focusedNodeInfo.treeId);
    if (tree) {
      callback(privates(tree).impl.get(focusedNodeInfo.nodeId));
      return;
    }
  });

  apiFunctions.setHandleRequest('getAccessibilityFocus', function(callback) {
    const focusedNodeInfo = GetAccessibilityFocusNative();
    if (!focusedNodeInfo) {
      callback(null);
      return;
    }
    const tree = AutomationRootNode.getOrCreate(focusedNodeInfo.treeId);
    if (tree) {
      callback(privates(tree).impl.get(focusedNodeInfo.nodeId));
    }
  });

  function removeTreeChangeObserver(observer) {
    for (const id in automationUtil.treeChangeObserverMap) {
      if (automationUtil.treeChangeObserverMap[id] == observer) {
        RemoveTreeChangeObserver(id);
        delete automationUtil.treeChangeObserverMap[id];
        return;
      }
    }
  }
  apiFunctions.setHandleRequest('removeTreeChangeObserver', function(observer) {
    removeTreeChangeObserver(observer);
  });

  function addTreeChangeObserver(filter, observer) {
    removeTreeChangeObserver(observer);
    const id = automationUtil.nextTreeChangeObserverId++;
    AddTreeChangeObserver(id, filter);
    automationUtil.treeChangeObserverMap[id] = observer;
  }
  apiFunctions.setHandleRequest('addTreeChangeObserver',
      function(filter, observer) {
    addTreeChangeObserver(filter, observer);
  });

  apiFunctions.setHandleRequest('setDocumentSelection', function(params) {
    const anchorNodeImpl = privates(params.anchorObject).impl;
    const focusNodeImpl = privates(params.focusObject).impl;
    if (anchorNodeImpl.treeID !== focusNodeImpl.treeID) {
      throw new Error('Selection anchor and focus must be in the same tree.');
    }
    if (anchorNodeImpl.treeID === desktopId) {
      throw new Error('Use AutomationNode.setSelection to set the selection ' +
          'in the desktop tree.');
    }
    automationInternal.performAction(
        {
          treeID: anchorNodeImpl.treeID,
          automationNodeID: anchorNodeImpl.id,
          actionType: 'setSelection',
        },
        {
          focusNodeID: focusNodeImpl.id,
          anchorOffset: params.anchorOffset,
          focusOffset: params.focusOffset,
        });
  });
});

automationInternal.onChildTreeID.addListener(function(childTreeId) {
  const targetTree = AutomationRootNode.get(childTreeId);

  // If the tree is already loded, or if we previously requested it be loaded
  // (i.e. have a callback for it), don't try to do so again.
  if (targetTree || idToCallback[childTreeId]) {
    return;
  }

  // A WebView in the desktop tree has a different AX tree as its child.
  // When we encounter a WebView with a child AX tree id that we don't
  // currently have cached, explicitly request that AX tree from the
  // browser process and set up a callback when it loads to attach that
  // tree as a child of this node and fire appropriate events.
  automationUtil.storeTreeCallback(childTreeId, function(root) {
    const rootImpl = privates(root).impl;
    rootImpl.dispatchEvent('loadComplete', 'page');
    if (rootImpl.parent) {
      privates(rootImpl.parent).impl.dispatchEvent('childrenChanged');
    }
  }, true);

  automationInternal.enableTree(childTreeId);
});

automationInternal.onTreeChange.addListener(function(
    observerID, treeID, nodeID, changeType) {
  const tree = AutomationRootNode.getOrCreate(treeID);
  if (!tree) {
    return;
  }

  const node = privates(tree).impl.get(nodeID);
  if (!node) {
    return;
  }

  const observer = automationUtil.treeChangeObserverMap[observerID];
  if (!observer) {
    return;
  }

  try {
    observer({target: node, type: changeType});
  } catch (e) {
    exceptionHandler.handle('Error in tree change observer for ' +
        changeType, e);
  }
});

automationInternal.onNodesRemoved.addListener(function(treeID, nodeIDs) {
  const tree = AutomationRootNode.getOrCreate(treeID);
  if (!tree) {
    return;
  }

  for (let i = 0; i < nodeIDs.length; i++) {
    privates(tree).impl.remove(nodeIDs[i]);
  }
});

automationInternal.onAllAutomationEventListenersRemoved.addListener(() => {
  if (!desktopId) {
    return;
  }
  automationInternal.disableDesktop(() => {
    desktopId = undefined;
    desktopTree = undefined;
    idToCallback = {};
    AutomationRootNode.destroyAll();
    StopCachingAccessibilityTrees();
  });
});

/**
 * Dispatch accessibility events fired on individual nodes to its
 * corresponding AutomationNode.
 */
automationInternal.onAccessibilityEvent.addListener(function(eventParams) {
  const id = eventParams.treeID;
  const targetTree = AutomationRootNode.getOrCreate(id);
  if (eventParams.eventType == 'mediaStartedPlaying' ||
      eventParams.eventType == 'mediaStoppedPlaying') {
    // These events are global to the tree.
    eventParams.targetID = privates(targetTree).impl.id;
  }

  privates(targetTree).impl.onAccessibilityEvent(eventParams);

  // If we're not waiting on a callback, we can early out here.
  if (!(id in idToCallback)) {
    return;
  }

  // We usually get a 'placeholder' tree first, which doesn't have any url
  // attribute or child nodes. If we've got that, wait for the full tree before
  // calling the callback.
  // TODO(dmazzoni): Don't send down placeholder (crbug.com/397553)
  if (id != desktopId && !targetTree.url && targetTree.children.length == 0) {
    return;
  }

  // If the tree wasn't available, the callback will have been cached in
  // idToCallback, so call and delete it now that we have the complete tree.
  for (let i = 0; i < idToCallback[id].length; i++) {
    const callback = idToCallback[id][i];
    callback(targetTree);
  }
  delete idToCallback[id];
});

automationInternal.onAccessibilityTreeDestroyed.addListener(function(id) {
  // Destroy the AutomationRootNode.
  const targetTree = AutomationRootNode.get(id);
  if (targetTree) {
    privates(targetTree).impl.destroy();
    AutomationRootNode.destroy(id);
  } else {
    logging.WARNING('no targetTree to destroy');
  }

  // Destroy the native cache of the accessibility tree.
  DestroyAccessibilityTree(id);
});

automationInternal.onAccessibilityTreeSerializationError.addListener(
    function(id) {
  automationInternal.enableTree(id);
});

automationInternal.onActionResult.addListener(function(
    treeID, requestID, result) {
  const targetTree = AutomationRootNode.get(treeID);
  if (!targetTree) {
    return;
  }

  privates(targetTree).impl.onActionResult(requestID, result);
});

automationInternal.onGetTextLocationResult.addListener(function(
    textLocationParams) {
  const targetTree = AutomationRootNode.get(textLocationParams.treeID);
  if (!targetTree) {
    return;
  }
  privates(targetTree).impl.onGetTextLocationResult(textLocationParams);
});
