// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of custom bindings for the contextMenus API.
// This is used to implement the contextMenus API for extensions and for the
// <webview> tag (see chrome_web_view_experimental.js).

var contextMenuNatives = requireNative('context_menus');

// Add the bindings to the contextMenus API.
function createContextMenusHandlers(isWebview) {
  var eventName = isWebview ? 'webViewInternal.contextMenus' : 'contextMenus';
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
    var createProperties = isWebview ? arguments[1] : arguments[0];
    createProperties.generatedId = contextMenuNatives.GetNextContextMenuId();
    var id = contextMenus.getIdFromCreateProperties(createProperties);
    var instanceId = isWebview ? arguments[0] : INSTANCEID_NON_WEBVIEW;
    var onclick = createProperties.onclick;

    var optArgs = {
      __proto__: null,
      customCallback: getCallback($Function.bind(createCallback, null,
                                                 instanceId, id, onclick)),
    };
    var name = isWebview ?
        'chromeWebViewInternal.contextMenusCreate' : 'contextMenus.create';
    bindingUtil.sendRequest(name, $Array.from(arguments), optArgs);
    return id;
  };

  function removeCallback(instanceId, id) {
    delete contextMenus.handlersForId(instanceId, id)[id];
  }

  requestHandlers.remove = function() {
    var instanceId = isWebview ? arguments[0] : INSTANCEID_NON_WEBVIEW;
    var id = isWebview ? arguments[1] : arguments[0];
    var optArgs = {
      __proto__: null,
      customCallback: getCallback($Function.bind(removeCallback, null,
                                                 instanceId, id)),
    };
    var name = isWebview ?
        'chromeWebViewInternal.contextMenusRemove' : 'contextMenus.remove';
    bindingUtil.sendRequest(name, $Array.from(arguments), optArgs);
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
    var instanceId = isWebview ? arguments[0] : INSTANCEID_NON_WEBVIEW;
    var id = isWebview ? arguments[1] : arguments[0];
    var updateProperties = isWebview ? arguments[2] : arguments[1];
    var onclick = updateProperties.onclick;
    var optArgs = {
      __proto__: null,
      customCallback: getCallback($Function.bind(updateCallback, null,
                                                 instanceId, id, onclick)),
    };

    var name = isWebview ?
        'chromeWebViewInternal.contextMenusUpdate' :
        'contextMenus.update';
    bindingUtil.sendRequest(name, $Array.from(arguments), optArgs);
  };

  function removeAllCallback(instanceId) {
    delete contextMenus.handlers[instanceId];
  }

  requestHandlers.removeAll = function() {
    var instanceId = isWebview ? arguments[0] : INSTANCEID_NON_WEBVIEW;
    var optArgs = {
      __proto__: null,
      customCallback: getCallback($Function.bind(removeAllCallback, null,
                                                 instanceId)),
    };

    var name = isWebview ?
        'chromeWebViewInternal.contextMenusRemoveAll' :
        'contextMenus.removeAll';
    bindingUtil.sendRequest(name, $Array.from(arguments), optArgs);
  };

  return {
    requestHandlers: requestHandlers,
  };
}

exports.$set('create', createContextMenusHandlers);
