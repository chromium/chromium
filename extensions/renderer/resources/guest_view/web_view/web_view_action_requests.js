// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements helper objects for the dialog, newwindow, and
// permissionrequest <webview> events.

var logging = requireNative('logging');
var MessagingNatives = requireNative('messaging_natives');
var WebViewConstants = require('webViewConstants').WebViewConstants;
var WebViewInternal = getInternalApi('webViewInternal');

var PERMISSION_TYPES = ['media',
                        'geolocation',
                        'pointerLock',
                        'download',
                        'loadplugin',
                        'filesystem',
                        'fullscreen',
                        'hid'];

// The browser will kill us if we send it a bad instance ID.
// TODO(crbug.com/41353094): Remove once the cause of the bad ID is known.
function CrashIfInvalidInstanceId(instanceId, culpritFunction) {
  logging.CHECK(
      instanceId > 0,
      'WebView: Invalid instance ID (' + instanceId + ') from ' +
          culpritFunction);
}

// -----------------------------------------------------------------------------
// WebViewActionRequest object.

// Default partial implementation of a webview action request.
function WebViewActionRequest(webViewImpl, event, webViewEvent, interfaceName) {
  this.webViewImpl = webViewImpl;
  this.event = event;
  this.webViewEvent = webViewEvent;
  this.interfaceName = interfaceName;
  this.guestInstanceId = this.webViewImpl.guest.getId();
  this.requestId = event.requestId;
  this.actionTaken = false;

  // Add on the request information specific to the request type.
  for (var infoName in this.event.requestInfo) {
    this.event[infoName] = this.event.requestInfo[infoName];
    this.webViewEvent[infoName] = this.event.requestInfo[infoName];
  }
}

// Prevent GuestViewEvents inadvertently inheritng code from the global Object,
// allowing a pathway for unintended execution of user code.
// TODO(wjmaclean): Track down other issues of Object inheritance.
// https://crbug.com/701034
WebViewActionRequest.prototype.__proto__ = null;

// Performs the default action for the request.
WebViewActionRequest.prototype.defaultAction = function() {
  // Do nothing if the action has already been taken or the requester is
  // already gone (in which case its guestInstanceId will be stale).
  if (this.actionTaken ||
      this.guestInstanceId != this.webViewImpl.guest.getId()) {
    return;
  }

  this.actionTaken = true;
  CrashIfInvalidInstanceId(
      this.guestInstanceId, 'WebViewActionRequest.defaultAction');
  WebViewInternal.setPermission(this.guestInstanceId, this.requestId, 'default',
                                '', $Function.bind(function(allowed) {
    if (allowed) {
      return;
    }
    this.showWarningMessage();
  }, this));
};

// Called to handle the action request's event.
WebViewActionRequest.prototype.handleActionRequestEvent = function() {
  // Construct the interface object and attach it to |webViewEvent|.
  var request = this.getInterfaceObject();
  this.webViewEvent[this.interfaceName] = request;

  var defaultPrevented = !this.webViewImpl.dispatchEvent(this.webViewEvent);
  // Set |webViewEvent| to null to break the circular reference to |request| so
  // that the garbage collector can eventually collect it.
  this.webViewEvent = null;
  if (this.actionTaken) {
    return;
  }

  if (defaultPrevented) {
    // Track the lifetime of |request| with the garbage collector.
    MessagingNatives.BindToGC(
        request, $Function.bind(this.defaultAction, this));
  } else {
    this.defaultAction();
  }
};

// Displays a warning message when an action request is blocked by default.
WebViewActionRequest.prototype.showWarningMessage = function() {
  window.console.warn(this.WARNING_MSG_REQUEST_BLOCKED);
};

// This function ensures that each action is taken at most once.
WebViewActionRequest.prototype.validateCall = function() {
  if (this.actionTaken) {
    throw new Error(this.ERROR_MSG_ACTION_ALREADY_TAKEN);
  }
  this.actionTaken = true;
};

// The following are implemented by the specific action request.

// Returns the interface object for this action request.
WebViewActionRequest.prototype.getInterfaceObject = undefined;

// Error/warning messages.
WebViewActionRequest.prototype.ERROR_MSG_ACTION_ALREADY_TAKEN = undefined;
WebViewActionRequest.prototype.WARNING_MSG_REQUEST_BLOCKED = undefined;

// -----------------------------------------------------------------------------
// Dialog object.

// Represents a dialog box request (e.g. alert()).
function Dialog(webViewImpl, event, webViewEvent) {
  $Function.call(
      WebViewActionRequest, this, webViewImpl, event, webViewEvent, 'dialog');

  this.handleActionRequestEvent();
}

Dialog.prototype.__proto__ = WebViewActionRequest.prototype;

Dialog.prototype.getInterfaceObject = function() {
  return {
    ok: $Function.bind(function(user_input) {
      this.validateCall();
      user_input = user_input || '';
      CrashIfInvalidInstanceId(this.guestInstanceId, 'Dialog ok');
      WebViewInternal.setPermission(
          this.guestInstanceId, this.requestId, 'allow', user_input);
    }, this),
    cancel: $Function.bind(function() {
      this.validateCall();
      CrashIfInvalidInstanceId(this.guestInstanceId, 'Dialog cancel');
      WebViewInternal.setPermission(
          this.guestInstanceId, this.requestId, 'deny');
    }, this)
  };
};

Dialog.prototype.showWarningMessage = function() {
  var VOWELS = ['a', 'e', 'i', 'o', 'u'];
  var dialogType = this.event.messageType;
  var article =
      ($Array.indexOf(VOWELS, dialogType.charAt(0)) >= 0) ? 'An' : 'A';
  this.WARNING_MSG_REQUEST_BLOCKED = $String.replace(
      $String.replace(this.WARNING_MSG_REQUEST_BLOCKED, '%1', article), '%2',
      dialogType);
  window.console.warn(this.WARNING_MSG_REQUEST_BLOCKED);
};

Dialog.prototype.ERROR_MSG_ACTION_ALREADY_TAKEN =
    WebViewConstants.ERROR_MSG_DIALOG_ACTION_ALREADY_TAKEN;
Dialog.prototype.WARNING_MSG_REQUEST_BLOCKED =
    WebViewConstants.WARNING_MSG_DIALOG_REQUEST_BLOCKED;

// -----------------------------------------------------------------------------
// NewWindow object.

// Represents a new window request.
function NewWindow(webViewImpl, event, webViewEvent) {
  $Function.call(
      WebViewActionRequest, this, webViewImpl, event, webViewEvent, 'window');

  this.handleActionRequestEvent();
}

NewWindow.prototype.__proto__ = WebViewActionRequest.prototype;

NewWindow.prototype.getInterfaceObject = function() {
  return {
    attach: $Function.bind(function(webview) {
      this.validateCall();
      if (!webview || !webview.tagName ||
          (webview.tagName != 'WEBVIEW' &&
           webview.tagName != 'CONTROLLEDFRAME')) {
        throw new Error('Cannot attach to invalid container element.');
      }

      var webViewImpl = privates(webview).internal;
      // Update the partition.
      if (this.event.partition) {
        webViewImpl.onAttach(this.event.partition);
      }

      webViewImpl.attachWindow(this.event.windowId);

      if (this.guestInstanceId != this.webViewImpl.guest.getId()) {
        // If the opener is already gone, then its guestInstanceId will be
        // stale.
        return;
      }

      // If the object being passed into attach is not a valid <webview>
      // then we will fail and it will be treated as if the new window
      // was rejected. The permission API plumbing is used here to clean
      // up the state created for the new window if attaching fails.
      CrashIfInvalidInstanceId(this.guestInstanceId, 'NewWindow attach');
      WebViewInternal.setPermission(this.guestInstanceId, this.requestId,
                                    'allow');
    }, this),
    discard: $Function.bind(function() {
      this.validateCall();
      if (!this.guestInstanceId) {
        // If the opener is already gone, then we won't have its
        // guestInstanceId.
        return;
      }
      CrashIfInvalidInstanceId(this.guestInstanceId, 'NewWindow discard');
      WebViewInternal.setPermission(
          this.guestInstanceId, this.requestId, 'deny');
    }, this)
  };
};

NewWindow.prototype.ERROR_MSG_ACTION_ALREADY_TAKEN =
    WebViewConstants.ERROR_MSG_NEWWINDOW_ACTION_ALREADY_TAKEN;
NewWindow.prototype.WARNING_MSG_REQUEST_BLOCKED =
    WebViewConstants.WARNING_MSG_NEWWINDOW_REQUEST_BLOCKED;

// -----------------------------------------------------------------------------
// PermissionRequest object.

// Represents a permission request (e.g. to access the filesystem).
function PermissionRequest(webViewImpl, event, webViewEvent) {
  $Function.call(
      WebViewActionRequest, this, webViewImpl, event, webViewEvent, 'request');

  if (!this.validPermissionCheck()) {
    return;
  }

  this.handleActionRequestEvent();
}

PermissionRequest.prototype.__proto__ = WebViewActionRequest.prototype;

PermissionRequest.prototype.allow = function() {
  this.validateCall();
  CrashIfInvalidInstanceId(this.guestInstanceId, 'PermissionRequest.allow');
  WebViewInternal.setPermission(this.guestInstanceId, this.requestId, 'allow');
};

PermissionRequest.prototype.deny = function() {
  this.validateCall();
  CrashIfInvalidInstanceId(this.guestInstanceId, 'PermissionRequest.deny');
  WebViewInternal.setPermission(this.guestInstanceId, this.requestId, 'deny');
};

PermissionRequest.prototype.getInterfaceObject = function() {
  var request = {
    allow: $Function.bind(this.allow, this),
    deny: $Function.bind(this.deny, this)
  };

  // Add on the request information specific to the request type.
  for (var infoName in this.event.requestInfo) {
    request[infoName] = this.event.requestInfo[infoName];
  }

  return $Object.freeze(request);
};

PermissionRequest.prototype.showWarningMessage = function() {
  window.console.warn($String.replace(
      this.WARNING_MSG_REQUEST_BLOCKED, '%1', this.event.permission));
};

// Checks that the requested permission is valid. Returns true if valid.
PermissionRequest.prototype.validPermissionCheck = function() {
  if ($Array.indexOf(PERMISSION_TYPES, this.event.permission) < 0) {
    // The permission type is not allowed. Trigger the default response.
    this.defaultAction();
    return false;
  }
  return true;
};

PermissionRequest.prototype.ERROR_MSG_ACTION_ALREADY_TAKEN =
    WebViewConstants.ERROR_MSG_PERMISSION_ACTION_ALREADY_TAKEN;
PermissionRequest.prototype.WARNING_MSG_REQUEST_BLOCKED =
    WebViewConstants.WARNING_MSG_PERMISSION_REQUEST_BLOCKED;

// -----------------------------------------------------------------------------

// FullscreenPermissionRequest object.

// Represents a fullscreen permission request.
function FullscreenPermissionRequest(webViewImpl, event, webViewEvent) {
  $Function.call(PermissionRequest, this, webViewImpl, event, webViewEvent);
}

FullscreenPermissionRequest.prototype.__proto__ = PermissionRequest.prototype;

FullscreenPermissionRequest.prototype.allow = function() {
  $Function.call(PermissionRequest.prototype.allow, this);
  // Now make the <webview> element go fullscreen.
  this.webViewImpl.makeElementFullscreen();
};

// -----------------------------------------------------------------------------

var WebViewActionRequests = {
  WebViewActionRequest: WebViewActionRequest,
  Dialog: Dialog,
  NewWindow: NewWindow,
  PermissionRequest: PermissionRequest,
  FullscreenPermissionRequest: FullscreenPermissionRequest
};

// Exports.
exports.$set('WebViewActionRequests', WebViewActionRequests);
