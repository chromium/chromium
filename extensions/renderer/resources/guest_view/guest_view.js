// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements a wrapper for a guestview that manages its
// creation, attaching, and destruction.

var $Document = require('safeMethods').SafeMethods.$Document;
var $HTMLIFrameElement = require('safeMethods').SafeMethods.$HTMLIFrameElement;
var $Node = require('safeMethods').SafeMethods.$Node;
var CreateEvent = require('guestViewEvents').CreateEvent;
var GuestViewInternal = getInternalApi('guestViewInternal');
var GuestViewInternalNatives = requireNative('guest_view_internal');

// Events.
var ResizeEvent = CreateEvent('guestViewInternal.onResize');

// Error messages.
var ERROR_MSG_ALREADY_ATTACHED = 'The guest has already been attached.';
var ERROR_MSG_ALREADY_CREATED = 'The guest has already been created.';
var ERROR_MSG_INVALID_STATE = 'The guest is in an invalid state.';
var ERROR_MSG_NOT_CREATED = 'The guest has not been created.';

// Properties.
var PROPERTY_ON_RESIZE = 'onresize';

var getIframeContentWindow = function(viewInstanceId) {
  var view = GuestViewInternalNatives.GetViewFromID(viewInstanceId);
  if (!view)
    return null;

  var internalIframeElement = view.internalElement;
  if (internalIframeElement)
    return $HTMLIFrameElement.contentWindow.get(internalIframeElement);

  return null;
};

// Returns the window object associated with the given view's element.
var getOwnerWindow = function(viewInstanceId) {
  var view = GuestViewInternalNatives.GetViewFromID(viewInstanceId);
  if (!view) {
    return null;
  }

  var ownerDocument = $Node.ownerDocument.get(view.element);
  if (!ownerDocument) {
    return null;
  }

  return $Document.defaultView.get(ownerDocument);
};

// Contains and hides the internal implementation details of |GuestView|,
// including maintaining its state and enforcing the proper usage of its API
// fucntions.
function GuestViewImpl(guestView, viewType, guestInstanceId) {
  if (guestInstanceId) {
    this.id = guestInstanceId;
    this.state = GuestViewImpl.GuestState.GUEST_STATE_CREATED;
  } else {
    this.id = 0;
    this.state = GuestViewImpl.GuestState.GUEST_STATE_START;
  }
  this.actionQueue = [];
  this.contentWindow = null;
  this.guestView = guestView;
  this.pendingAction = null;
  this.viewType = viewType;
  this.internalInstanceId = 0;

  this.setupOnResize();
}

// Prevent GuestViewImpl inadvertently inheriting code from the global Object,
// allowing a pathway for executing unintended user code execution.
// TODO(wjmaclean): Track down other issues of Object inheritance.
// https://crbug.com/701034
GuestViewImpl.prototype.__proto__ = null;

// Possible states.
GuestViewImpl.GuestState = {
  GUEST_STATE_START: 0,
  GUEST_STATE_CREATED: 1,
  GUEST_STATE_ATTACHED: 2
};

// Sets up the onResize property on the GuestView.
GuestViewImpl.prototype.setupOnResize = function() {
  $Object.defineProperty(this.guestView, PROPERTY_ON_RESIZE, {
    get: $Function.bind(function() {
      return this[PROPERTY_ON_RESIZE];
    }, this),
    set: $Function.bind(function(value) {
      this[PROPERTY_ON_RESIZE] = value;
    }, this),
    enumerable: true
  });

  this.callOnResize = $Function.bind(function(e) {
    if (!this[PROPERTY_ON_RESIZE]) {
      return;
    }
    this[PROPERTY_ON_RESIZE](e);
  }, this);
};

// Callback wrapper that is used to call the callback of the pending action (if
// one exists), and then performs the next action in the queue.
GuestViewImpl.prototype.handleCallback = function(callback) {
  if (callback) {
    callback();
  }
  this.pendingAction = null;
  this.performNextAction();
};

// Perform the next action in the queue, if one exists.
GuestViewImpl.prototype.performNextAction = function() {
  // Make sure that there is not already an action in progress, and that there
  // exists a queued action to perform.
  if (!this.pendingAction && this.actionQueue.length) {
    this.pendingAction = $Array.shift(this.actionQueue);
    this.pendingAction();
  }
};

// Check the current state to see if the proposed action is valid. Returns false
// if invalid.
GuestViewImpl.prototype.checkState = function(action) {
  // Create an error prefix based on the proposed action.
  var errorPrefix = 'Error calling ' + action + ': ';

  // Check that the current state is valid.
  if (!(this.state >= 0 && this.state <= 2)) {
    window.console.error(errorPrefix + ERROR_MSG_INVALID_STATE);
    return false;
  }

  // Map of possible errors for each action. For each action, the errors are
  // listed for states in the order: GUEST_STATE_START, GUEST_STATE_CREATED,
  // GUEST_STATE_ATTACHED.
  var errors = {
    'attach': [ERROR_MSG_NOT_CREATED, null, ERROR_MSG_ALREADY_ATTACHED],
    'create': [null, ERROR_MSG_ALREADY_CREATED, ERROR_MSG_ALREADY_CREATED],
    'destroy': [null, null, null],
    'setSize': [ERROR_MSG_NOT_CREATED, null, null]
  };

  // Check that the proposed action is a real action.
  if (errors[action] == undefined) {
    window.console.error(errorPrefix + ERROR_MSG_INVALID_ACTION);
    return false;
  }

  // Report the error if the proposed action is found to be invalid for the
  // current state.
  var error;
  if (error = errors[action][this.state]) {
    window.console.error(errorPrefix + error);
    return false;
  }

  return true;
};

// Returns a wrapper function for |func| with a weak reference to |this|. This
// implementation of weakWrapper() requires a provided |viewInstanceId| since
// GuestViewImpl does not store this ID.
GuestViewImpl.prototype.weakWrapper = function(func, viewInstanceId) {
  return function() {
    var view = GuestViewInternalNatives.GetViewFromID(viewInstanceId);
    if (view && view.guest) {
      return $Function.apply(
          func, view.guest.internal, $Array.slice(arguments));
    }
  };
};

// Internal implementation of attach().
GuestViewImpl.prototype.attachImpl = function(
    internalInstanceId, viewInstanceId, attachParams, callback) {
  var view = GuestViewInternalNatives.GetViewFromID(viewInstanceId);
  if (!view.elementAttached) {
    // Defer the attachment until the <webview> element is attached.
    view.deferredAttachCallback = $Function.bind(this.attachImpl,
        this, internalInstanceId, viewInstanceId, attachParams, callback);
    return;
  };

  // Check the current state.
  if (!this.checkState('attach')) {
    this.handleCallback(callback);
    return;
  }

  // Callback wrapper function to set the contentWindow following attachment,
  // and advance the queue.
  var callbackWrapper = function(callback) {
    var contentWindow = getIframeContentWindow(viewInstanceId);
    // Check if attaching failed.
    if (!contentWindow) {
      this.state = GuestViewImpl.GuestState.GUEST_STATE_CREATED;
      this.internalInstanceId = 0;
    } else {
      // Only update the contentWindow if attaching is successful.
      this.contentWindow = contentWindow;
    }

    this.handleCallback(callback);
  };

  attachParams['instanceId'] = viewInstanceId;
  var contentWindow = getIframeContentWindow(viewInstanceId);

  // The internal iframe element may have a null contentWindow at this point.
  // For example, we may be trying to attach a guest whose element is in
  // another iframe which has already shutdown.
  if (!contentWindow) {
    this.state = GuestViewImpl.GuestState.GUEST_STATE_CREATED;
    this.internalInstanceId = 0;
    this.handleCallback(callback);
    return;
  }

  // |contentWindow| is used to retrieve the RenderFrame in cpp.
  GuestViewInternalNatives.AttachIframeGuest(
      internalInstanceId, this.id, attachParams, contentWindow,
      $Function.bind(callbackWrapper, this, callback));

  this.internalInstanceId = internalInstanceId;
  this.state = GuestViewImpl.GuestState.GUEST_STATE_ATTACHED;

  // Detach automatically when the container is destroyed.
  GuestViewInternalNatives.RegisterDestructionCallback(
      internalInstanceId, this.weakWrapper(function() {
    if (this.state != GuestViewImpl.GuestState.GUEST_STATE_ATTACHED ||
        this.internalInstanceId != internalInstanceId) {
      return;
    }

    this.internalInstanceId = 0;
    this.state = GuestViewImpl.GuestState.GUEST_STATE_CREATED;
  }, viewInstanceId));
};

// Internal implementation of create().
GuestViewImpl.prototype.createImpl = function(
    viewInstanceId, createParams, callback) {
  // Check the current state.
  if (!this.checkState('create')) {
    this.handleCallback(callback);
    return;
  }

  // Callback wrapper function to store the guestInstanceId from the
  // createGuest() callback, handle potential creation failure, and advance the
  // queue.
  var callbackWrapper = function(callback, instanceId) {
    this.id = instanceId;

    // Check if creation failed.
    if (this.id === 0) {
      this.state = GuestViewImpl.GuestState.GUEST_STATE_START;
      this.contentWindow = null;
    }

    ResizeEvent.addListener(this.callOnResize, {instanceId: this.id});
    this.handleCallback(callback);
  };

  // Determine the window which owns the guest view element, so we can inform
  // the browser of the prospective owner of the guest.
  var ownerWindow = getOwnerWindow(viewInstanceId);
  var ownerFrameToken = GuestViewInternalNatives.GetFrameToken(ownerWindow);

  GuestViewInternal.createGuest(
      this.viewType, ownerFrameToken, createParams,
      $Function.bind(callbackWrapper, this, callback));

  this.state = GuestViewImpl.GuestState.GUEST_STATE_CREATED;
};

// Internal implementation of destroy().
GuestViewImpl.prototype.destroyImpl = function(callback) {
  // Check the current state.
  if (!this.checkState('destroy')) {
    this.handleCallback(callback);
    return;
  }

  if (this.state == GuestViewImpl.GuestState.GUEST_STATE_START) {
    // destroy() does nothing in this case.
    this.handleCallback(callback);
    return;
  }

  if (this.state == GuestViewImpl.GuestState.GUEST_STATE_CREATED) {
    // If we destroy a guest before attaching it, inform the browser so it can
    // clear its associated state. This is only needed for unattached guests,
    // since after attachment, the browser knows when to clear the state.
    GuestViewInternal.destroyUnattachedGuest(this.id);
  }

  // Reset the state of the destroyed guest;
  this.contentWindow = null;
  this.id = 0;
  this.internalInstanceId = 0;
  this.state = GuestViewImpl.GuestState.GUEST_STATE_START;
  if (ResizeEvent.hasListener(this.callOnResize)) {
    ResizeEvent.removeListener(this.callOnResize);
  }

  // Handle callback at end to avoid handling items in the action queue out of
  // order, since the callback is run synchronously here.
  this.handleCallback(callback);
};

// Internal implementation of setSize().
GuestViewImpl.prototype.setSizeImpl = function(sizeParams, callback) {
  // Check the current state.
  if (!this.checkState('setSize')) {
    this.handleCallback(callback);
    return;
  }

  GuestViewInternal.setSize(
      this.id, sizeParams,
      $Function.bind(this.handleCallback, this, callback));
};

// The exposed interface to a guestview. Exposes in its API the functions
// attach(), create(), destroy(), and getId(). All other implementation details
// are hidden.
function GuestView(viewType, guestInstanceId) {
  this.internal = new GuestViewImpl(this, viewType, guestInstanceId);
}

GuestView.prototype.__proto__ = null;

// Attaches the guestview to the container with ID |internalInstanceId|.
GuestView.prototype.attach = function(
    internalInstanceId, viewInstanceId, attachParams, callback) {
  var internal = this.internal;
  $Array.push(internal.actionQueue, $Function.bind(internal.attachImpl,
      internal, internalInstanceId, viewInstanceId, attachParams, callback));
  internal.performNextAction();
};

// Creates the guestview.
GuestView.prototype.create = function(viewInstanceId, createParams, callback) {
  var internal = this.internal;
  $Array.push(
      internal.actionQueue,
      $Function.bind(
          internal.createImpl, internal, viewInstanceId, createParams,
          callback));
  internal.performNextAction();
};

// Destroys the guestview. Nothing can be done with the guestview after it has
// been destroyed.
GuestView.prototype.destroy = function(callback) {
  var internal = this.internal;
  $Array.push(
      internal.actionQueue,
      $Function.bind(internal.destroyImpl, internal, callback));
  internal.performNextAction();
};

// Adjusts the guestview's sizing parameters.
GuestView.prototype.setSize = function(sizeParams, callback) {
  var internal = this.internal;
  $Array.push(internal.actionQueue,
      $Function.bind(internal.setSizeImpl, internal, sizeParams, callback));
  internal.performNextAction();
};

// Returns the contentWindow for this guestview.
GuestView.prototype.getContentWindow = function() {
  var internal = this.internal;
  return internal.contentWindow;
};

// Returns the ID for this guestview.
GuestView.prototype.getId = function() {
  var internal = this.internal;
  return internal.id;
};

// Exports
exports.$set('GuestView', GuestView);
exports.$set('ResizeEvent', ResizeEvent);
