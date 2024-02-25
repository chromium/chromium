// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of custom bindings for the contextMenus API.
// This is used to implement the contextMenus API for extensions and for the
// <webview> tag (see chrome_web_view_experimental.js).

var contextMenuNatives = requireNative('context_menus');

// Add the bindings to the contextMenus API.
function createContextMenusHandlers(webViewNamespace) {
  var eventName = '';
  var isWebview = !!webViewNamespace;
  if (isWebview) {
    eventName = webViewNamespace === 'chromeWebViewInternal' ?
        'webViewInternal.contextMenus' :
        webViewNamespace + '.contextMenus';
  } else {
    eventName = 'contextMenus';
  }
  // Some dummy value for chrome.contextMenus instances.
  // Webviews use positive integers, and 0 to denote an invalid webview ID.
  // The following constant is -1 to avoid any conflicts between webview IDs and
  // extensions.
  var INSTANCEID_NON_WEBVIEW = -1;

  // Generates a customCallback for a given method. |handleCallback| will be
  // invoked with the same arguments this function is called with.
  function getCallback(handleCallback) {
    return function() {
      var extensionCallback = arguments[arguments.length - 1];
      if (bindingUtil.hasLastError()) {
        if (extensionCallback)
          extensionCallback();
        return;
      }

      $Function.apply(handleCallback, null, arguments);
      if (extensionCallback)
        extensionCallback();
    };
  }

  // Shift off the instanceId from the arguments for webviews.
  function getInstanceId(args) {
    if (isWebview)
      return $Array.shift(args);  // This modifies the array in-place.
    return INSTANCEID_NON_WEBVIEW;
  }

  var contextMenus = { __proto__: null };
  contextMenus.handlers = { __proto__: null };

  var supportsLazyListeners = !isWebview;
  var supportsFilters = false;
  contextMenus.event = bindingUtil.createCustomEvent(
      eventName, supportsFilters, supportsLazyListeners);

  contextMenus.getIdFromCreateProperties = function(createProperties) {
    if (typeof createProperties.id !== 'undefined')
      return createProperties.id;
    return createProperties.generatedId;
  };

  contextMenus.handlersForId = function(instanceId, id) {
    if (!contextMenus.handlers[instanceId]) {
      contextMenus.handlers[instanceId] = {
        generated: {},
        string: {}
      };
    }
    if (typeof id === 'number')
      return contextMenus.handlers[instanceId].generated;
    return contextMenus.handlers[instanceId].string;
  };

  contextMenus.ensureListenerSetup = function() {
    if (contextMenus.listening) {
      return;
    }
    contextMenus.listening = true;
    contextMenus.event.addListener(function(info) {
      var instanceId = INSTANCEID_NON_WEBVIEW;
      if (isWebview) {
        instanceId = info.webviewInstanceId;
        // Don't expose |webviewInstanceId| via the public API.
        delete info.webviewInstanceId;
      }

      var id = info.menuItemId;
      var onclick = contextMenus.handlersForId(instanceId, id)[id];
      if (onclick) {
        $Function.apply(onclick, null, arguments);
      }
    });
  };

  // To be used with apiFunctions.setHandleRequest
  var requestHandlers = { __proto__: null };

  function createCallback(instanceId, id, onclick) {
    if (onclick) {
      contextMenus.ensureListenerSetup();
      contextMenus.handlersForId(instanceId, id)[id] = onclick;
    }
  }

  requestHandlers.create = function() {
    var args = $Array.from(arguments);
    var instanceId = getInstanceId(args);
    var createProperties = args[0];
    var callback = args[1];
    createProperties.generatedId = contextMenuNatives.GetNextContextMenuId();
    var id = contextMenus.getIdFromCreateProperties(createProperties);
    var onclick = createProperties.onclick;

    var optArgs = {
      __proto__: null,
      customCallback: getCallback($Function.bind(createCallback, null,
                                                 instanceId, id, onclick)),
    };
    if (isWebview) {
      bindingUtil.sendRequest(
          webViewNamespace + '.contextMenusCreate',
          [instanceId, createProperties, callback], optArgs);
    } else {
      bindingUtil.sendRequest(
          'contextMenus.create', [createProperties, callback], optArgs);
    }
    return id;
  };

  function removeCallback(instanceId, id) {
    delete contextMenus.handlersForId(instanceId, id)[id];
  }

  requestHandlers.remove = function() {
    var args = $Array.from(arguments);
    var instanceId = getInstanceId(args);
    var id = args[0];
    var callback = args[1];
    var optArgs = {
      __proto__: null,
      customCallback: getCallback($Function.bind(removeCallback, null,
                                                 instanceId, id)),
    };
    if (isWebview) {
      bindingUtil.sendRequest(
          'chromeWebViewInternal.contextMenusRemove',
          [instanceId, id, callback], optArgs);
    } else {
      bindingUtil.sendRequest('contextMenus.remove', [id, callback], optArgs);
    }
  };

  function updateCallback(instanceId, id, onclick) {
    if (onclick) {
      contextMenus.ensureListenerSetup();
      contextMenus.handlersForId(instanceId, id)[id] = onclick;
    } else if (onclick === null) {
      // When onclick is explicitly set to null, remove the event listener.
      delete contextMenus.handlersForId(instanceId, id)[id];
    }
  }

  requestHandlers.update = function() {
    var args = $Array.from(arguments);
    var instanceId = getInstanceId(args);
    var id = args[0];
    var updateProperties = args[1];
    var callback = args[2];
    var onclick = updateProperties.onclick;
    var optArgs = {
      __proto__: null,
      customCallback: getCallback($Function.bind(updateCallback, null,
                                                 instanceId, id, onclick)),
    };
    if (isWebview) {
      bindingUtil.sendRequest(
          'chromeWebViewInternal.contextMenusUpdate',
          [instanceId, id, updateProperties, callback], optArgs);
    } else {
      bindingUtil.sendRequest(
          'contextMenus.update', [id, updateProperties, callback], optArgs);
    }
  };

  function removeAllCallback(instanceId) {
    delete contextMenus.handlers[instanceId];
  }

  requestHandlers.removeAll = function() {
    var args = $Array.from(arguments);
    var instanceId = getInstanceId(args);
    var callback = args[0];
    var optArgs = {
      __proto__: null,
      customCallback: getCallback($Function.bind(removeAllCallback, null,
                                                 instanceId)),
    };
    if (isWebview) {
      bindingUtil.sendRequest(
          'chromeWebViewInternal.contextMenusRemoveAll', [instanceId, callback],
          optArgs);
    } else {
      bindingUtil.sendRequest('contextMenus.removeAll', [callback], optArgs);
    }
  };

  return {
    requestHandlers: requestHandlers,
  };
}

exports.$set('create', createContextMenusHandlers);
