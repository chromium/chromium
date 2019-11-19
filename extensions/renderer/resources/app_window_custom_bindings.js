// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the app_window API.

var appWindowNatives = requireNative('app_window_natives');
var runtimeNatives = requireNative('runtime');
var forEach = require('utils').forEach;
var renderFrameObserverNatives = requireNative('renderFrameObserverNatives');

var appWindowData = null;
var currentAppWindow = null;
var currentWindowInternal = null;

var kSetBoundsFunction = 'setBounds';
var kSetSizeConstraintsFunction = 'setSizeConstraints';

// Bounds class definition.
var Bounds = function(boundsKey) {
  privates(this).boundsKey_ = boundsKey;
};
Object.defineProperty(Bounds.prototype, 'left', {
  get: function() {
    return appWindowData[privates(this).boundsKey_].left;
  },
  set: function(left) {
    this.setPosition(left, null);
  },
  enumerable: true
});
Object.defineProperty(Bounds.prototype, 'top', {
  get: function() {
    return appWindowData[privates(this).boundsKey_].top;
  },
  set: function(top) {
    this.setPosition(null, top);
  },
  enumerable: true
});
Object.defineProperty(Bounds.prototype, 'width', {
  get: function() {
    return appWindowData[privates(this).boundsKey_].width;
  },
  set: function(width) {
    this.setSize(width, null);
  },
  enumerable: true
});
Object.defineProperty(Bounds.prototype, 'height', {
  get: function() {
    return appWindowData[privates(this).boundsKey_].height;
  },
  set: function(height) {
    this.setSize(null, height);
  },
  enumerable: true
});
Object.defineProperty(Bounds.prototype, 'minWidth', {
  get: function() {
    return appWindowData[privates(this).boundsKey_].minWidth;
  },
  set: function(minWidth) {
    updateSizeConstraints(privates(this).boundsKey_, { minWidth: minWidth });
  },
  enumerable: true
});
Object.defineProperty(Bounds.prototype, 'maxWidth', {
  get: function() {
    return appWindowData[privates(this).boundsKey_].maxWidth;
  },
  set: function(maxWidth) {
    updateSizeConstraints(privates(this).boundsKey_, { maxWidth: maxWidth });
  },
  enumerable: true
});
Object.defineProperty(Bounds.prototype, 'minHeight', {
  get: function() {
    return appWindowData[privates(this).boundsKey_].minHeight;
  },
  set: function(minHeight) {
    updateSizeConstraints(privates(this).boundsKey_, { minHeight: minHeight });
  },
  enumerable: true
});
Object.defineProperty(Bounds.prototype, 'maxHeight', {
  get: function() {
    return appWindowData[privates(this).boundsKey_].maxHeight;
  },
  set: function(maxHeight) {
    updateSizeConstraints(privates(this).boundsKey_, { maxHeight: maxHeight });
  },
  enumerable: true
});
Bounds.prototype.setPosition = function(left, top) {
  updateBounds(privates(this).boundsKey_, { left: left, top: top });
};
Bounds.prototype.setSize = function(width, height) {
  updateBounds(privates(this).boundsKey_, { width: width, height: height });
};
Bounds.prototype.setMinimumSize = function(minWidth, minHeight) {
  updateSizeConstraints(privates(this).boundsKey_,
                        { minWidth: minWidth, minHeight: minHeight });
};
Bounds.prototype.setMaximumSize = function(maxWidth, maxHeight) {
  updateSizeConstraints(privates(this).boundsKey_,
                        { maxWidth: maxWidth, maxHeight: maxHeight });
};

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('create',
      function(name, request, callback, windowParams) {
    // |callback| is optional.
    let maybeCallback = callback || function() {};

    // When window creation fails, windowParams is undefined. Return undefined
    // to the caller.
    if (!windowParams || !windowParams.frameId) {
      maybeCallback(undefined);
      return;
    }

    let view = appWindowNatives.GetFrame(windowParams.frameId,
                                         true /* notifyBrowser */);

    if (windowParams.existingWindow) {
      // Not creating a new window, but activating an existing one, so trigger
      // callback with existing window and don't do anything else.
      let windowResult = view ? view.chrome.app.window.current() : undefined;
      maybeCallback(windowResult);
      return;
    }

    // Handle the sandboxed page case.
    if (!view || !view.chrome.app) {
      var sandbox_window_message = 'Creating sandboxed window, it doesn\'t ' +
          'have access to the chrome.app API.';
      if (callback) {
        sandbox_window_message = sandbox_window_message +
            ' The chrome.app.window.create callback will be called, but ' +
            'there will be no object provided for the sandboxed window.';
      }
      console.warn(sandbox_window_message);
      maybeCallback(undefined);
      return;
    }

    // Handle error pages.
    // TODO(arthursonzogni): Figure out why view.chrome.app is defined for error
    // pages and stop doing it.
    if (!view.chrome.app.window) {
      maybeCallback(undefined);
      return;
    }

    // Initialize appWindowData in the newly created JS context
    view.chrome.app.window.initializeAppWindow(windowParams);

    var willCallback = renderFrameObserverNatives.OnDocumentElementCreated(
        windowParams.frameId, function(success) {
          let windowResult = success ? view.chrome.app.window.current()
                                     : undefined;
          maybeCallback(windowResult);
        });
    appWindowNatives.ResumeParser(windowParams.frameId);
    if (!willCallback)
      maybeCallback(undefined);
  });

  apiFunctions.setHandleRequest('current', function() {
    if (!currentAppWindow) {
      console.error('The JavaScript context calling ' +
                    'chrome.app.window.current() has no associated AppWindow.');
      return null;
    }
    return currentAppWindow;
  });

  apiFunctions.setHandleRequest('getAll', function() {
    var views = runtimeNatives.GetExtensionViews(-1, -1, 'APP_WINDOW');
    return $Array.map(views, function(win) {
      return win.chrome.app.window.current();
    });
  });

  apiFunctions.setHandleRequest('get', function(id) {
    var windows = $Array.filter(chrome.app.window.getAll(), function(win) {
      return win.id == id;
    });
    return windows.length > 0 ? windows[0] : null;
  });

  apiFunctions.setHandleRequest('canSetVisibleOnAllWorkspaces', function() {
    return /Mac/.test(navigator.platform) || /Linux/.test(navigator.userAgent);
  });

  // This is an internal function, but needs to be bound into a closure
  // so the correct JS context is used for global variables such as
  // currentWindowInternal, appWindowData, etc.
  apiFunctions.setHandleRequest('initializeAppWindow', function(params) {
    currentWindowInternal = getInternalApi('app.currentWindowInternal');
    var AppWindow = function() {
      this.innerBounds = new Bounds('innerBounds');
      this.outerBounds = new Bounds('outerBounds');
    };
    forEach(currentWindowInternal, function(key, value) {
      // Do not add internal functions that should not appear in the AppWindow
      // interface. They are called by Bounds mutators.
      if (key !== kSetBoundsFunction && key !== kSetSizeConstraintsFunction)
        AppWindow.prototype[key] = value;
    });
    AppWindow.prototype.moveTo = $Function.bind(window.moveTo, window);
    AppWindow.prototype.resizeTo = $Function.bind(window.resizeTo, window);
    AppWindow.prototype.contentWindow = window;
    var supportsFilters = false;
    var supportsLazyListeners = false;
    AppWindow.prototype.onClosed =
        bindingUtil.createCustomEvent(undefined /* name */,
                                      supportsFilters,
                                      supportsLazyListeners);
    AppWindow.prototype.close = function() {
      this.contentWindow.close();
    };
    AppWindow.prototype.getBounds = function() {
      // This is to maintain backcompatibility with a bug on Windows and
      // ChromeOS, which returns the position of the window but the size of
      // the content.
      var innerBounds = appWindowData.innerBounds;
      var outerBounds = appWindowData.outerBounds;
      return { left: outerBounds.left, top: outerBounds.top,
               width: innerBounds.width, height: innerBounds.height };
    };
    AppWindow.prototype.setBounds = function(bounds) {
      updateBounds('bounds', bounds);
    };
    AppWindow.prototype.isFullscreen = function() {
      return appWindowData.fullscreen;
    };
    AppWindow.prototype.isMinimized = function() {
      return appWindowData.minimized;
    };
    AppWindow.prototype.isMaximized = function() {
      return appWindowData.maximized;
    };
    AppWindow.prototype.isAlwaysOnTop = function() {
      return appWindowData.alwaysOnTop;
    };
    AppWindow.prototype.alphaEnabled = function() {
      return appWindowData.alphaEnabled;
    };

    Object.defineProperty(AppWindow.prototype, 'id', {get: function() {
      return appWindowData.id;
    }});

    // These properties are for testing.
    Object.defineProperty(
        AppWindow.prototype, 'hasFrameColor', {get: function() {
      return appWindowData.hasFrameColor;
    }});

    Object.defineProperty(AppWindow.prototype, 'activeFrameColor',
                          {get: function() {
      return appWindowData.activeFrameColor;
    }});

    Object.defineProperty(AppWindow.prototype, 'inactiveFrameColor',
                          {get: function() {
      return appWindowData.inactiveFrameColor;
    }});

    appWindowData = {
      id: params.id || '',
      innerBounds: {
        left: params.innerBounds.left,
        top: params.innerBounds.top,
        width: params.innerBounds.width,
        height: params.innerBounds.height,

        minWidth: params.innerBounds.minWidth,
        minHeight: params.innerBounds.minHeight,
        maxWidth: params.innerBounds.maxWidth,
        maxHeight: params.innerBounds.maxHeight
      },
      outerBounds: {
        left: params.outerBounds.left,
        top: params.outerBounds.top,
        width: params.outerBounds.width,
        height: params.outerBounds.height,

        minWidth: params.outerBounds.minWidth,
        minHeight: params.outerBounds.minHeight,
        maxWidth: params.outerBounds.maxWidth,
        maxHeight: params.outerBounds.maxHeight
      },
      fullscreen: params.fullscreen,
      minimized: params.minimized,
      maximized: params.maximized,
      alwaysOnTop: params.alwaysOnTop,
      hasFrameColor: params.hasFrameColor,
      activeFrameColor: params.activeFrameColor,
      inactiveFrameColor: params.inactiveFrameColor,
      alphaEnabled: params.alphaEnabled
    };
    currentAppWindow = new AppWindow;
  });
});

function boundsEqual(bounds1, bounds2) {
  if (!bounds1 || !bounds2)
    return false;
  return (bounds1.left == bounds2.left && bounds1.top == bounds2.top &&
          bounds1.width == bounds2.width && bounds1.height == bounds2.height);
}

function dispatchEventIfExists(target, name) {
  // Sometimes apps like to put their own properties on the window which
  // break our assumptions.
  var event = target[name];
  if (event && (typeof event.dispatch == 'function'))
    event.dispatch();
  else
    console.warn('Could not dispatch ' + name + ', event has been clobbered');
}

function updateAppWindowProperties(update) {
  if (!appWindowData)
    return;

  var oldData = appWindowData;
  update.id = oldData.id;
  appWindowData = update;

  var currentWindow = currentAppWindow;

  if (!boundsEqual(oldData.innerBounds, update.innerBounds))
    dispatchEventIfExists(currentWindow, "onBoundsChanged");

  if (!oldData.fullscreen && update.fullscreen)
    dispatchEventIfExists(currentWindow, "onFullscreened");
  if (!oldData.minimized && update.minimized)
    dispatchEventIfExists(currentWindow, "onMinimized");
  if (!oldData.maximized && update.maximized)
    dispatchEventIfExists(currentWindow, "onMaximized");

  if ((oldData.fullscreen && !update.fullscreen) ||
      (oldData.minimized && !update.minimized) ||
      (oldData.maximized && !update.maximized))
    dispatchEventIfExists(currentWindow, "onRestored");

  if (oldData.alphaEnabled !== update.alphaEnabled)
    dispatchEventIfExists(currentWindow, "onAlphaEnabledChanged");
};

function onAppWindowClosed() {
  if (!currentAppWindow)
    return;
  dispatchEventIfExists(currentAppWindow, "onClosed");
}

function updateBounds(boundsType, bounds) {
  if (!currentWindowInternal)
    return;

  currentWindowInternal.setBounds(boundsType, bounds);
}

function updateSizeConstraints(boundsType, constraints) {
  if (!currentWindowInternal)
    return;

  forEach(constraints, function(key, value) {
    // From the perspective of the API, null is used to reset constraints.
    // We need to convert this to 0 because a value of null is interpreted
    // the same as undefined in the browser and leaves the constraint unchanged.
    if (value === null)
      constraints[key] = 0;
  });

  currentWindowInternal.setSizeConstraints(boundsType, constraints);
}

exports.$set('onAppWindowClosed', onAppWindowClosed);
exports.$set('updateAppWindowProperties', updateAppWindowProperties);
