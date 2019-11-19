// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Event management for WebView.

var $Document = require('safeMethods').SafeMethods.$Document;
var CreateEvent = require('guestViewEvents').CreateEvent;
var DCHECK = requireNative('logging').DCHECK;
var DeclarativeWebRequestSchema =
    requireNative('schema_registry').GetSchema('declarativeWebRequest');
var GuestViewEvents = require('guestViewEvents').GuestViewEvents;
var GuestViewInternalNatives = requireNative('guest_view_internal');
var IdGenerator = requireNative('id_generator');
var WebRequestEvent = require('webRequestEvent').WebRequestEvent;
var WebRequestSchema =
    requireNative('schema_registry').GetSchema('webRequest');
var WebViewActionRequests =
    require('webViewActionRequests').WebViewActionRequests;

var WebRequestMessageEvent = CreateEvent('webViewInternal.onMessage');

function WebViewEvents(webViewImpl) {
  $Function.call(GuestViewEvents, this, webViewImpl);

  this.setupWebRequestEvents();
}

function createOnMessageEvent(name, schema, options, webviewId) {
  var subEventName = name + '/' + IdGenerator.GetNextId();
  var newEvent = bindingUtil.createCustomEvent(
      subEventName, false /* supports filters */,
      false /* supports lazy listeners */);

  var view = GuestViewInternalNatives.GetViewFromID(webviewId || 0);
  if (view) {
    view.events.addScopedListener(
        WebRequestMessageEvent,
        $Function.bind(function() {
          // Re-dispatch to subEvent's listeners.
          $Function.apply(newEvent.dispatch, newEvent, $Array.slice(arguments));
        }, newEvent),
        {instanceId: webviewId || 0});
  }

  return newEvent;
}

WebViewEvents.prototype.__proto__ = GuestViewEvents.prototype;

// A dictionary of <webview> extension events to be listened for. This
// dictionary augments |GuestViewEvents.EVENTS| in guest_view_events.js. See the
// documentation there for details.
WebViewEvents.EVENTS = {
  'audiostatechanged': {
    evt: CreateEvent('webViewInternal.onAudioStateChanged'),
    fields: ['audible']
  },
  'close': {
    evt: CreateEvent('webViewInternal.onClose')
  },
  'consolemessage': {
    evt: CreateEvent('webViewInternal.onConsoleMessage'),
    fields: ['level', 'message', 'line', 'sourceId']
  },
  'contentload': {
    evt: CreateEvent('webViewInternal.onContentLoad')
  },
  'dialog': {
    cancelable: true,
    evt: CreateEvent('webViewInternal.onDialog'),
    fields: ['defaultPromptText', 'messageText', 'messageType', 'url'],
    handler: 'handleDialogEvent'
  },
  'droplink': {
    evt: CreateEvent('webViewInternal.onDropLink'),
    fields: ['url']
  },
  'exit': {
    evt: CreateEvent('webViewInternal.onExit'),
    fields: ['processId', 'reason']
  },
  'exitfullscreen': {
    evt: CreateEvent('webViewInternal.onExitFullscreen'),
    fields: ['url'],
    handler: 'handleFullscreenExitEvent',
    internal: true
  },
  'findupdate': {
    evt: CreateEvent('webViewInternal.onFindReply'),
    fields: [
      'searchText',
      'numberOfMatches',
      'activeMatchOrdinal',
      'selectionRect',
      'canceled',
      'finalUpdate'
    ]
  },
  'framenamechanged': {
    evt: CreateEvent('webViewInternal.onFrameNameChanged'),
    handler: 'handleFrameNameChangedEvent',
    internal: true
  },
  'loadabort': {
    cancelable: true,
    evt: CreateEvent('webViewInternal.onLoadAbort'),
    fields: ['url', 'isTopLevel', 'code', 'reason'],
    handler: 'handleLoadAbortEvent'
  },
  'loadcommit': {
    evt: CreateEvent('webViewInternal.onLoadCommit'),
    fields: ['url', 'isTopLevel'],
    handler: 'handleLoadCommitEvent'
  },
  'loadprogress': {
    evt: CreateEvent('webViewInternal.onLoadProgress'),
    fields: ['url', 'progress']
  },
  'loadredirect': {
    evt: CreateEvent('webViewInternal.onLoadRedirect'),
    fields: ['isTopLevel', 'oldUrl', 'newUrl']
  },
  'loadstart': {
    evt: CreateEvent('webViewInternal.onLoadStart'),
    fields: ['url', 'isTopLevel']
  },
  'loadstop': {
    evt: CreateEvent('webViewInternal.onLoadStop')
  },
  'newwindow': {
    cancelable: true,
    evt: CreateEvent('webViewInternal.onNewWindow'),
    fields: [
      'initialHeight',
      'initialWidth',
      'targetUrl',
      'windowOpenDisposition',
      'name'
    ],
    handler: 'handleNewWindowEvent'
  },
  'permissionrequest': {
    cancelable: true,
    evt: CreateEvent('webViewInternal.onPermissionRequest'),
    fields: [
      'identifier',
      'lastUnlockedBySelf',
      'name',
      'permission',
      'requestMethod',
      'url',
      'userGesture'
    ],
    handler: 'handlePermissionEvent'
  },
  'responsive': {
    evt: CreateEvent('webViewInternal.onResponsive'),
    fields: ['processId']
  },
  'sizechanged': {
    evt: CreateEvent('webViewInternal.onSizeChanged'),
    fields: ['oldHeight', 'oldWidth', 'newHeight', 'newWidth'],
    handler: 'handleSizeChangedEvent'
  },
  'unresponsive': {
    evt: CreateEvent('webViewInternal.onUnresponsive'),
    fields: ['processId']
  },
  'zoomchange': {
    evt: CreateEvent('webViewInternal.onZoomChange'),
    fields: ['oldZoomFactor', 'newZoomFactor']
  }
};

WebViewEvents.EVENTS.__proto__ = null;
for (var eventName in WebViewEvents.EVENTS) {
  WebViewEvents.EVENTS[eventName].__proto__ = null;
}

WebViewEvents.prototype.setupWebRequestEvents = function() {
  var request = {};
  var createWebRequestEvent = $Function.bind(function(webRequestEvent) {
    return this.weakWrapper(function() {
      if (!this[webRequestEvent.name]) {
        this[webRequestEvent.name] =
            new WebRequestEvent(
                'webViewInternal.' + webRequestEvent.name,
                webRequestEvent.parameters,
                webRequestEvent.extraParameters, webRequestEvent.options,
                this.view.viewInstanceId);
      }
      return this[webRequestEvent.name];
    });
  }, this);

  var createDeclarativeWebRequestEvent =
      $Function.bind(function(webRequestEvent) {
    return this.weakWrapper(function() {
      if (!this[webRequestEvent.name]) {
        var newEvent;
        var eventName =
            'webViewInternal.declarativeWebRequest.' + webRequestEvent.name;
        if (webRequestEvent.name === 'onMessage') {
          // The onMessage event gets a special event type because we want the
          // listener to fire only for messages targeted for this particular
          // <webview>.
          newEvent = createOnMessageEvent(eventName,
                                          webRequestEvent.parameters,
                                          webRequestEvent.options,
                                          this.view.viewInstanceId);
        } else {
          newEvent = bindingUtil.createCustomDeclarativeEvent(
              eventName, webRequestEvent.options.actions,
              webRequestEvent.options.conditions,
              this.view.viewInstanceId || 0);
        }
        this[webRequestEvent.name] = newEvent;
      }
      return this[webRequestEvent.name];
    });
  }, this);

  for (var i = 0; i < DeclarativeWebRequestSchema.events.length; ++i) {
    var eventSchema = DeclarativeWebRequestSchema.events[i];
    var webRequestEvent = createDeclarativeWebRequestEvent(eventSchema);
    $Object.defineProperty(
        request, eventSchema.name, {get: webRequestEvent, enumerable: true});
  }

  // Populate the WebRequest events from the API definition.
  for (var i = 0; i < WebRequestSchema.events.length; ++i) {
    var eventSchema = WebRequestSchema.events[i];

    // Skip "onActionIgnored" which is not relevant for webviews.
    if (eventSchema.name === 'onActionIgnored')
      continue;

    var webRequestEvent = createWebRequestEvent(eventSchema);
    $Object.defineProperty(
        request, eventSchema.name, {get: webRequestEvent, enumerable: true});
  }

  this.view.setRequestPropertyOnWebViewElement(request);
};

WebViewEvents.prototype.getEvents = function() {
  return WebViewEvents.EVENTS;
};

WebViewEvents.prototype.handleDialogEvent = function(event, eventName) {
  var webViewEvent = this.makeDomEvent(event, eventName);
  new WebViewActionRequests.Dialog(this.view, event, webViewEvent);
};

WebViewEvents.prototype.handleFrameNameChangedEvent = function(event) {
  this.view.onFrameNameChanged(event.name);
};

WebViewEvents.prototype.handleFullscreenExitEvent = function(event, eventName) {
  $Document.webkitCancelFullScreen(document);
};

WebViewEvents.prototype.handleLoadAbortEvent = function(event, eventName) {
  var showWarningMessage = function(code, reason) {
    var WARNING_MSG_LOAD_ABORTED = '<webview>: ' +
        'The load has aborted with error %1: %2.';
    window.console.warn($String.replace(
        $String.replace(WARNING_MSG_LOAD_ABORTED, '%1', code), '%2', reason));
  };
  var webViewEvent = this.makeDomEvent(event, eventName);
  if (this.view.dispatchEvent(webViewEvent)) {
    showWarningMessage(event.code, event.reason);
  }
};

WebViewEvents.prototype.handleLoadCommitEvent = function(event, eventName) {
  this.view.onLoadCommit(event.baseUrlForDataUrl,
                         event.currentEntryIndex,
                         event.entryCount,
                         event.processId,
                         event.url,
                         event.isTopLevel);
  var webViewEvent = this.makeDomEvent(event, eventName);
  this.view.dispatchEvent(webViewEvent);
};

WebViewEvents.prototype.handleNewWindowEvent = function(event, eventName) {
  var webViewEvent = this.makeDomEvent(event, eventName);
  new WebViewActionRequests.NewWindow(this.view, event, webViewEvent);
};

WebViewEvents.prototype.handlePermissionEvent = function(event, eventName) {
  var webViewEvent = this.makeDomEvent(event, eventName);
  if (event.permission === 'fullscreen') {
    new WebViewActionRequests.FullscreenPermissionRequest(
        this.view, event, webViewEvent);
  } else {
    new WebViewActionRequests.PermissionRequest(this.view, event, webViewEvent);
  }
};

WebViewEvents.prototype.handleSizeChangedEvent = function(event, eventName) {
  var webViewEvent = this.makeDomEvent(event, eventName);
  this.view.onSizeChanged(webViewEvent);
};

// Exports.
exports.$set('WebViewEvents', WebViewEvents);
