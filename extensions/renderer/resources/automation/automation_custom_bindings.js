// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the automation API.
var AutomationNode = require('automationNode').AutomationNode;
var AutomationRootNode = require('automationNode').AutomationRootNode;
var automationInternal = getInternalApi('automationInternal');
var exceptionHandler = require('uncaught_exception_handler');
var logging = requireNative('logging');
var nativeAutomationInternal = requireNative('automationInternal');
var DestroyAccessibilityTree =
    nativeAutomationInternal.DestroyAccessibilityTree;
var GetIntAttribute = nativeAutomationInternal.GetIntAttribute;
var StartCachingAccessibilityTrees =
    nativeAutomationInternal.StartCachingAccessibilityTrees;
var AddTreeChangeObserver = nativeAutomationInternal.AddTreeChangeObserver;
var RemoveTreeChangeObserver =
    nativeAutomationInternal.RemoveTreeChangeObserver;
var GetFocusNative = nativeAutomationInternal.GetFocus;

/**
 * A namespace to export utility functions to other files in automation.
 */
window.automationUtil = function() {};

// TODO(aboxhall): Look into using WeakMap
var idToCallback = {};

var desktopId = undefined;

automationUtil.storeTreeCallback = function(id, callback) {
  if (!callback)
    return;

  var targetTree = AutomationRootNode.get(id);
  if (!targetTree) {
    // If we haven't cached the tree, hold the callback until the tree is
    // populated by the initial onAccessibilityEvent call.
    if (id in idToCallback)
      idToCallback[id].push(callback);
    else
      idToCallback[id] = [callback];
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
  var apiFunctions = bindingsAPI.apiFunctions;

  // TODO(aboxhall, dtseng): Make this return the speced AutomationRootNode obj.
automationUtil.tabIDToAutomationNode = {};
  apiFunctions.setHandleRequest('getTree', function getTree(tabID, callback) {
    StartCachingAccessibilityTrees();

    // enableTab() ensures the renderer for the active or specified tab has
    // accessibility enabled, and fetches its ax tree id to use as
    // a key in the idToAutomationRootNode map. The callback to
    // enableTab is bound to the callback passed in to getTree(), so that once
    // the tree is available (either due to having been cached earlier, or after
    // an accessibility event occurs which causes the tree to be populated), the
    // callback can be called.
    if (tabID && automationUtil.tabIDToAutomationNode[tabID]) {
      callback(automationUtil.tabIDToAutomationNode[tabID]);
      return;
    }

    var params = { tabID: tabID };
    automationInternal.enableTab(params,
                                 function onEnable(treeID, resultTabID) {
          if (bindingUtil.hasLastError()) {
            callback();
            return;
          }
          automationUtil.storeTreeCallback(treeID, function(root) {
            automationUtil.tabIDToAutomationNode[resultTabID] = root;
            callback(root);
          });
        });
  });

  var desktopTree = null;
  apiFunctions.setHandleRequest('getDesktop', function(callback) {
    StartCachingAccessibilityTrees();
    if (desktopId !== undefined)
      desktopTree = AutomationRootNode.get(desktopId);
    if (!desktopTree) {
      automationInternal.enableDesktop(function(treeId) {
        if (bindingUtil.hasLastError()) {
          AutomationRootNode.destroy(treeId);
          desktopId = undefined;
          callback();
          return;
        }
        desktopId = treeId;
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
    if (desktopId === undefined)
      return;

    var focusedNodeInfo = GetFocusNative(desktopId);
    if (!focusedNodeInfo) {
      callback(null);
      return;
    }
    var tree = AutomationRootNode.getOrCreate(focusedNodeInfo.treeId);
    if (tree) {
      callback(privates(tree).impl.get(focusedNodeInfo.nodeId));
      return;
    }
  });

  function removeTreeChangeObserver(observer) {
    for (var id in automationUtil.treeChangeObserverMap) {
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
    var id = automationUtil.nextTreeChangeObserverId++;
    AddTreeChangeObserver(id, filter);
    automationUtil.treeChangeObserverMap[id] = observer;
  }
  apiFunctions.setHandleRequest('addTreeChangeObserver',
      function(filter, observer) {
    addTreeChangeObserver(filter, observer);
  });

  apiFunctions.setHandleRequest('setDocumentSelection', function(params) {
    var anchorNodeImpl = privates(params.anchorObject).impl;
    var focusNodeImpl = privates(params.focusObject).impl;
    if (anchorNodeImpl.treeID !== focusNodeImpl.treeID)
      throw new Error('Selection anchor and focus must be in the same tree.');
    if (anchorNodeImpl.treeID === desktopId) {
      throw new Error('Use AutomationNode.setSelection to set the selection ' +
          'in the desktop tree.');
    }
    automationInternal.performAction({ treeID: anchorNodeImpl.treeID,
                                       automationNodeID: anchorNodeImpl.id,
                                       actionType: 'setSelection'},
                                     { focusNodeID: focusNodeImpl.id,
                                       anchorOffset: params.anchorOffset,
                                       focusOffset: params.focusOffset });
  });

});

automationInternal.onChildTreeID.addListener(function(childTreeId) {
  var targetTree = AutomationRootNode.get(childTreeId);

  // If the tree is already loded, or if we previously requested it be loaded
  // (i.e. have a callback for it), don't try to do so again.
  if (targetTree || idToCallback[childTreeId])
    return;

  // A WebView in the desktop tree has a different AX tree as its child.
  // When we encounter a WebView with a child AX tree id that we don't
  // currently have cached, explicitly request that AX tree from the
  // browser process and set up a callback when it loads to attach that
  // tree as a child of this node and fire appropriate events.
  automationUtil.storeTreeCallback(childTreeId, function(root) {
    privates(root).impl.dispatchEvent('loadComplete', 'page');
  }, true);

  automationInternal.enableFrame(childTreeId);
});

automationInternal.onTreeChange.addListener(function(observerID,
                                                     treeID,
                                                     nodeID,
                                                     changeType) {
  var tree = AutomationRootNode.getOrCreate(treeID);
  if (!tree)
    return;

  var node = privates(tree).impl.get(nodeID);
  if (!node)
    return;

  var observer = automationUtil.treeChangeObserverMap[observerID];
  if (!observer)
    return;

  try {
    observer({target: node, type: changeType});
  } catch (e) {
    exceptionHandler.handle('Error in tree change observer for ' +
        changeType, e);
  }
});

automationInternal.onNodesRemoved.addListener(function(treeID, nodeIDs) {
  var tree = AutomationRootNode.getOrCreate(treeID);
  if (!tree)
    return;

  for (var i = 0; i < nodeIDs.length; i++) {
    privates(tree).impl.remove(nodeIDs[i]);
  }
});

/**
 * Dispatch accessibility events fired on individual nodes to its
 * corresponding AutomationNode.
 */
automationInternal.onAccessibilityEvent.addListener(function(eventParams) {
  var id = eventParams.treeID;
  var targetTree = AutomationRootNode.getOrCreate(id);
  if (eventParams.eventType == 'mediaStartedPlaying' ||
      eventParams.eventType == 'mediaStoppedPlaying') {
    // These events are global to the tree.
    eventParams.targetID = privates(targetTree).impl.id;
  }

  if (!privates(targetTree).impl.onAccessibilityEvent(eventParams))
    return;

  // If we're not waiting on a callback to getTree(), we can early out here.
  if (!(id in idToCallback))
    return;

  // We usually get a 'placeholder' tree first, which doesn't have any url
  // attribute or child nodes. If we've got that, wait for the full tree before
  // calling the callback.
  // TODO(dmazzoni): Don't send down placeholder (crbug.com/397553)
  if (id != desktopId && !targetTree.url && targetTree.children.length == 0)
    return;

  // If the tree wasn't available when getTree() was called, the callback will
  // have been cached in idToCallback, so call and delete it now that we
  // have the complete tree.
  for (var i = 0; i < idToCallback[id].length; i++) {
    var callback = idToCallback[id][i];
    callback(targetTree);
  }
  delete idToCallback[id];
});

automationInternal.onAccessibilityTreeDestroyed.addListener(function(id) {
  // Destroy the AutomationRootNode.
  var targetTree = AutomationRootNode.get(id);
  if (targetTree) {
    privates(targetTree).impl.destroy();
    AutomationRootNode.destroy(id);
    for (var tabID in automationUtil.tabIDToAutomationNode) {
      if (automationUtil.tabIDToAutomationNode[tabID] == targetTree) {
        delete automationUtil.tabIDToAutomationNode[tabID];
      }
    }
  } else {
    logging.WARNING('no targetTree to destroy');
  }

  // Destroy the native cache of the accessibility tree.
  DestroyAccessibilityTree(id);
});

automationInternal.onAccessibilityTreeSerializationError.addListener(
    function(id) {
  automationInternal.enableFrame(id);
});

automationInternal.onActionResult.addListener(function(
    treeID, requestID, result) {
  var targetTree = AutomationRootNode.get(treeID);
  if (!targetTree)
    return;

  privates(targetTree).impl.onActionResult(requestID, result);
});

automationInternal.onGetTextLocationResult.addListener(function(
    textLocationParams) {
  var targetTree = AutomationRootNode.get(textLocationParams.treeID);
  if (!targetTree)
    return;
  privates(targetTree).impl.onGetTextLocationResult(textLocationParams);
});
