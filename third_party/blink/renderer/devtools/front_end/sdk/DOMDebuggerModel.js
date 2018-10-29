// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

SDK.DOMDebuggerModel = class extends SDK.SDKModel {
  /**
   * @param {!SDK.Target} target
   */
  constructor(target) {
    super(target);
    this._agent = target.domdebuggerAgent();
    this._runtimeModel = /** @type {!SDK.RuntimeModel} */ (target.model(SDK.RuntimeModel));
    this._domModel = /** @type {!SDK.DOMModel} */ (target.model(SDK.DOMModel));
    this._domModel.addEventListener(SDK.DOMModel.Events.DocumentUpdated, this._documentUpdated, this);
    this._domModel.addEventListener(SDK.DOMModel.Events.NodeRemoved, this._nodeRemoved, this);

    /** @type {!Array<!SDK.DOMDebuggerModel.DOMBreakpoint>} */
    this._domBreakpoints = [];
    this._domBreakpointsSetting = Common.settings.createLocalSetting('domBreakpoints', []);
    if (this._domModel.existingDocument())
      this._documentUpdated();
  }

  /**
   * @return {!SDK.RuntimeModel}
   */
  runtimeModel() {
    return this._runtimeModel;
  }

  /**
   * @param {!SDK.RemoteObject} remoteObject
   * @return {!Promise<!Array<!SDK.EventListener>>}
   */
  async eventListeners(remoteObject) {
    console.assert(remoteObject.runtimeModel() === this._runtimeModel);
    if (!remoteObject.objectId)
      return [];

    const payloads = await this._agent.getEventListeners(/** @type {string} */ (remoteObject.objectId));
    const eventListeners = [];
    for (const payload of payloads || []) {
      const location = this._runtimeModel.debuggerModel().createRawLocationByScriptId(
          payload.scriptId, payload.lineNumber, payload.columnNumber);
      if (!location)
        continue;
      eventListeners.push(new SDK.EventListener(
          this, remoteObject, payload.type, payload.useCapture, payload.passive, payload.once,
          payload.handler ? this._runtimeModel.createRemoteObject(payload.handler) : null,
          payload.originalHandler ? this._runtimeModel.createRemoteObject(payload.originalHandler) : null, location,
          null));
    }
    return eventListeners;
  }

  retrieveDOMBreakpoints() {
    this._domModel.requestDocument();
  }

  /**
   * @return {!Array<!SDK.DOMDebuggerModel.DOMBreakpoint>}
   */
  domBreakpoints() {
    return this._domBreakpoints.slice();
  }

  /**
   * @param {!SDK.DOMNode} node
   * @param {!SDK.DOMDebuggerModel.DOMBreakpoint.Type} type
   * @return {boolean}
   */
  hasDOMBreakpoint(node, type) {
    return this._domBreakpoints.some(breakpoint => (breakpoint.node === node && breakpoint.type === type));
  }

  /**
   * @param {!SDK.DOMNode} node
   * @param {!SDK.DOMDebuggerModel.DOMBreakpoint.Type} type
   * @return {!SDK.DOMDebuggerModel.DOMBreakpoint}
   */
  setDOMBreakpoint(node, type) {
    for (const breakpoint of this._domBreakpoints) {
      if (breakpoint.node === node && breakpoint.type === type) {
        this.toggleDOMBreakpoint(breakpoint, true);
        return breakpoint;
      }
    }
    const breakpoint = new SDK.DOMDebuggerModel.DOMBreakpoint(this, node, type, true);
    this._domBreakpoints.push(breakpoint);
    this._saveDOMBreakpoints();
    this._enableDOMBreakpoint(breakpoint);
    this.dispatchEventToListeners(SDK.DOMDebuggerModel.Events.DOMBreakpointAdded, breakpoint);
    return breakpoint;
  }

  /**
   * @param {!SDK.DOMNode} node
   * @param {!SDK.DOMDebuggerModel.DOMBreakpoint.Type} type
   */
  removeDOMBreakpoint(node, type) {
    this._removeDOMBreakpoints(breakpoint => breakpoint.node === node && breakpoint.type === type);
  }

  removeAllDOMBreakpoints() {
    this._removeDOMBreakpoints(breakpoint => true);
  }

  /**
   * @param {!SDK.DOMDebuggerModel.DOMBreakpoint} breakpoint
   * @param {boolean} enabled
   */
  toggleDOMBreakpoint(breakpoint, enabled) {
    if (enabled === breakpoint.enabled)
      return;
    breakpoint.enabled = enabled;
    if (enabled)
      this._enableDOMBreakpoint(breakpoint);
    else
      this._disableDOMBreakpoint(breakpoint);
    this.dispatchEventToListeners(SDK.DOMDebuggerModel.Events.DOMBreakpointToggled, breakpoint);
  }

  /**
   * @param {!SDK.DOMDebuggerModel.DOMBreakpoint} breakpoint
   */
  _enableDOMBreakpoint(breakpoint) {
    this._agent.setDOMBreakpoint(breakpoint.node.id, breakpoint.type);
    breakpoint.node.setMarker(SDK.DOMDebuggerModel.DOMBreakpoint.Marker, true);
  }

  /**
   * @param {!SDK.DOMDebuggerModel.DOMBreakpoint} breakpoint
   */
  _disableDOMBreakpoint(breakpoint) {
    this._agent.removeDOMBreakpoint(breakpoint.node.id, breakpoint.type);
    breakpoint.node.setMarker(
        SDK.DOMDebuggerModel.DOMBreakpoint.Marker, this._nodeHasBreakpoints(breakpoint.node) ? true : null);
  }

  /**
   * @param {!SDK.DOMNode} node
   * @return {boolean}
   */
  _nodeHasBreakpoints(node) {
    for (const breakpoint of this._domBreakpoints) {
      if (breakpoint.node === node && breakpoint.enabled)
        return true;
    }
    return false;
  }

  /**
   * @param {!Object} auxData
   * @return {?{type: !SDK.DOMDebuggerModel.DOMBreakpoint.Type, node: !SDK.DOMNode, targetNode: ?SDK.DOMNode, insertion: boolean}}
   */
  resolveDOMBreakpointData(auxData) {
    const type = auxData['type'];
    const node = this._domModel.nodeForId(auxData['nodeId']);
    if (!type || !node)
      return null;
    let targetNode = null;
    let insertion = false;
    if (type === SDK.DOMDebuggerModel.DOMBreakpoint.Type.SubtreeModified) {
      insertion = auxData['insertion'] || false;
      targetNode = this._domModel.nodeForId(auxData['targetNodeId']);
    }
    return {type: type, node: node, targetNode: targetNode, insertion: insertion};
  }

  /**
   * @return {string}
   */
  _currentURL() {
    const domDocument = this._domModel.existingDocument();
    return domDocument ? domDocument.documentURL : '';
  }

  _documentUpdated() {
    const removed = this._domBreakpoints;
    this._domBreakpoints = [];
    this.dispatchEventToListeners(SDK.DOMDebuggerModel.Events.DOMBreakpointsRemoved, removed);

    const currentURL = this._currentURL();
    for (const breakpoint of this._domBreakpointsSetting.get()) {
      if (breakpoint.url === currentURL)
        this._domModel.pushNodeByPathToFrontend(breakpoint.path).then(appendBreakpoint.bind(this, breakpoint));
    }

    /**
     * @param {!{type: !SDK.DOMDebuggerModel.DOMBreakpoint.Type, enabled: boolean}} breakpoint
     * @param {?number} nodeId
     * @this {SDK.DOMDebuggerModel}
     */
    function appendBreakpoint(breakpoint, nodeId) {
      const node = nodeId ? this._domModel.nodeForId(nodeId) : null;
      if (!node)
        return;
      const domBreakpoint = new SDK.DOMDebuggerModel.DOMBreakpoint(this, node, breakpoint.type, breakpoint.enabled);
      this._domBreakpoints.push(domBreakpoint);
      if (breakpoint.enabled)
        this._enableDOMBreakpoint(domBreakpoint);
      this.dispatchEventToListeners(SDK.DOMDebuggerModel.Events.DOMBreakpointAdded, domBreakpoint);
    }
  }

  /**
   * @param {function(!SDK.DOMDebuggerModel.DOMBreakpoint):boolean} filter
   */
  _removeDOMBreakpoints(filter) {
    const removed = [];
    const left = [];
    for (const breakpoint of this._domBreakpoints) {
      if (filter(breakpoint)) {
        removed.push(breakpoint);
        if (breakpoint.enabled) {
          breakpoint.enabled = false;
          this._disableDOMBreakpoint(breakpoint);
        }
      } else {
        left.push(breakpoint);
      }
    }

    if (!removed.length)
      return;
    this._domBreakpoints = left;
    this._saveDOMBreakpoints();
    this.dispatchEventToListeners(SDK.DOMDebuggerModel.Events.DOMBreakpointsRemoved, removed);
  }

  /**
   * @param {!Common.Event} event
   */
  _nodeRemoved(event) {
    const node = /** @type {!SDK.DOMNode} */ (event.data.node);
    const children = node.children() || [];
    this._removeDOMBreakpoints(breakpoint => breakpoint.node === node || children.indexOf(breakpoint.node) !== -1);
  }

  _saveDOMBreakpoints() {
    const currentURL = this._currentURL();
    const breakpoints = this._domBreakpointsSetting.get().filter(breakpoint => breakpoint.url !== currentURL);
    for (const breakpoint of this._domBreakpoints) {
      breakpoints.push(
          {url: currentURL, path: breakpoint.node.path(), type: breakpoint.type, enabled: breakpoint.enabled});
    }
    this._domBreakpointsSetting.set(breakpoints);
  }
};

SDK.SDKModel.register(SDK.DOMDebuggerModel, SDK.Target.Capability.DOM, false);

/** @enum {symbol} */
SDK.DOMDebuggerModel.Events = {
  DOMBreakpointAdded: Symbol('DOMBreakpointAdded'),
  DOMBreakpointToggled: Symbol('DOMBreakpointToggled'),
  DOMBreakpointsRemoved: Symbol('DOMBreakpointsRemoved'),
};

SDK.DOMDebuggerModel.DOMBreakpoint = class {
  /**
   * @param {!SDK.DOMDebuggerModel} domDebuggerModel
   * @param {!SDK.DOMNode} node
   * @param {!SDK.DOMDebuggerModel.DOMBreakpoint.Type} type
   * @param {boolean} enabled
   */
  constructor(domDebuggerModel, node, type, enabled) {
    this.domDebuggerModel = domDebuggerModel;
    this.node = node;
    this.type = type;
    this.enabled = enabled;
  }
};

/** @typedef {Protocol.DOMDebugger.DOMBreakpointType} */
SDK.DOMDebuggerModel.DOMBreakpoint.Type = Protocol.DOMDebugger.DOMBreakpointType;

SDK.DOMDebuggerModel.DOMBreakpoint.Marker = 'breakpoint-marker';

SDK.EventListener = class {
  /**
   * @param {!SDK.DOMDebuggerModel} domDebuggerModel
   * @param {!SDK.RemoteObject} eventTarget
   * @param {string} type
   * @param {boolean} useCapture
   * @param {boolean} passive
   * @param {boolean} once
   * @param {?SDK.RemoteObject} handler
   * @param {?SDK.RemoteObject} originalHandler
   * @param {!SDK.DebuggerModel.Location} location
   * @param {?SDK.RemoteObject} customRemoveFunction
   * @param {!SDK.EventListener.Origin=} origin
   */
  constructor(
      domDebuggerModel, eventTarget, type, useCapture, passive, once, handler, originalHandler, location,
      customRemoveFunction, origin) {
    this._domDebuggerModel = domDebuggerModel;
    this._eventTarget = eventTarget;
    this._type = type;
    this._useCapture = useCapture;
    this._passive = passive;
    this._once = once;
    this._handler = handler;
    this._originalHandler = originalHandler || handler;
    this._location = location;
    const script = location.script();
    this._sourceURL = script ? script.contentURL() : '';
    this._customRemoveFunction = customRemoveFunction;
    this._origin = origin || SDK.EventListener.Origin.Raw;
  }

  /**
   * @return {!SDK.DOMDebuggerModel}
   */
  domDebuggerModel() {
    return this._domDebuggerModel;
  }

  /**
   * @return {string}
   */
  type() {
    return this._type;
  }

  /**
   * @return {boolean}
   */
  useCapture() {
    return this._useCapture;
  }

  /**
   * @return {boolean}
   */
  passive() {
    return this._passive;
  }

  /**
   * @return {boolean}
   */
  once() {
    return this._once;
  }

  /**
   * @return {?SDK.RemoteObject}
   */
  handler() {
    return this._handler;
  }

  /**
   * @return {!SDK.DebuggerModel.Location}
   */
  location() {
    return this._location;
  }

  /**
   * @return {string}
   */
  sourceURL() {
    return this._sourceURL;
  }

  /**
   * @return {?SDK.RemoteObject}
   */
  originalHandler() {
    return this._originalHandler;
  }

  /**
   * @return {boolean}
   */
  canRemove() {
    return !!this._customRemoveFunction || this._origin !== SDK.EventListener.Origin.FrameworkUser;
  }

  /**
   * @return {!Promise<undefined>}
   */
  remove() {
    if (!this.canRemove())
      return Promise.resolve();

    if (this._origin !== SDK.EventListener.Origin.FrameworkUser) {
      /**
       * @param {string} type
       * @param {function()} listener
       * @param {boolean} useCapture
       * @this {Object}
       * @suppressReceiverCheck
       */
      function removeListener(type, listener, useCapture) {
        this.removeEventListener(type, listener, useCapture);
        if (this['on' + type])
          this['on' + type] = undefined;
      }

      return /** @type {!Promise<undefined>} */ (this._eventTarget.callFunction(removeListener, [
        SDK.RemoteObject.toCallArgument(this._type), SDK.RemoteObject.toCallArgument(this._originalHandler),
        SDK.RemoteObject.toCallArgument(this._useCapture)
      ]));
    }

    return this._customRemoveFunction
        .callFunction(
            callCustomRemove,
            [
              SDK.RemoteObject.toCallArgument(this._type),
              SDK.RemoteObject.toCallArgument(this._originalHandler),
              SDK.RemoteObject.toCallArgument(this._useCapture),
              SDK.RemoteObject.toCallArgument(this._passive),
            ])
        .then(() => undefined);

    /**
     * @param {string} type
     * @param {function()} listener
     * @param {boolean} useCapture
     * @param {boolean} passive
     * @this {Function}
     * @suppressReceiverCheck
     */
    function callCustomRemove(type, listener, useCapture, passive) {
      this.call(null, type, listener, useCapture, passive);
    }
  }

  /**
   * @return {boolean}
   */
  canTogglePassive() {
    return this._origin !== SDK.EventListener.Origin.FrameworkUser;
  }

  /**
   * @return {!Promise<undefined>}
   */
  togglePassive() {
    return /** @type {!Promise<undefined>} */ (this._eventTarget.callFunction(callTogglePassive, [
      SDK.RemoteObject.toCallArgument(this._type),
      SDK.RemoteObject.toCallArgument(this._originalHandler),
      SDK.RemoteObject.toCallArgument(this._useCapture),
      SDK.RemoteObject.toCallArgument(this._passive),
    ]));

    /**
     * @param {string} type
     * @param {function()} listener
     * @param {boolean} useCapture
     * @param {boolean} passive
     * @this {Object}
     * @suppressReceiverCheck
     */
    function callTogglePassive(type, listener, useCapture, passive) {
      this.removeEventListener(type, listener, {capture: useCapture});
      this.addEventListener(type, listener, {capture: useCapture, passive: !passive});
    }
  }

  /**
   * @return {!SDK.EventListener.Origin}
   */
  origin() {
    return this._origin;
  }

  markAsFramework() {
    this._origin = SDK.EventListener.Origin.Framework;
  }

  /**
   * @return {boolean}
   */
  isScrollBlockingType() {
    return this._type === 'touchstart' || this._type === 'touchmove' || this._type === 'mousewheel' ||
        this._type === 'wheel';
  }
};

/** @enum {string} */
SDK.EventListener.Origin = {
  Raw: 'Raw',
  Framework: 'Framework',
  FrameworkUser: 'FrameworkUser'
};

SDK.DOMDebuggerModel.EventListenerBreakpoint = class {
  /**
   * @param {string} instrumentationName
   * @param {string} eventName
   * @param {!Array<string>} eventTargetNames
   * @param {string} category
   * @param {string} title
   */
  constructor(instrumentationName, eventName, eventTargetNames, category, title) {
    this._instrumentationName = instrumentationName;
    this._eventName = eventName;
    this._eventTargetNames = eventTargetNames;
    this._category = category;
    this._title = title;
    this._enabled = false;
  }

  /**
   * @return {string}
   */
  category() {
    return this._category;
  }

  /**
   * @return {boolean}
   */
  enabled() {
    return this._enabled;
  }

  /**
   * @param {boolean} enabled
   */
  setEnabled(enabled) {
    if (this._enabled === enabled)
      return;
    this._enabled = enabled;
    for (const model of SDK.targetManager.models(SDK.DOMDebuggerModel))
      this._updateOnModel(model);
  }

  /**
   * @param {!SDK.DOMDebuggerModel} model
   */
  _updateOnModel(model) {
    if (this._instrumentationName) {
      if (this._enabled)
        model._agent.setInstrumentationBreakpoint(this._instrumentationName);
      else
        model._agent.removeInstrumentationBreakpoint(this._instrumentationName);
    } else {
      for (const eventTargetName of this._eventTargetNames) {
        if (this._enabled)
          model._agent.setEventListenerBreakpoint(this._eventName, eventTargetName);
        else
          model._agent.removeEventListenerBreakpoint(this._eventName, eventTargetName);
      }
    }
  }

  /**
   * @return {string}
   */
  title() {
    return this._title;
  }
};

SDK.DOMDebuggerModel.EventListenerBreakpoint._listener = 'listener:';
SDK.DOMDebuggerModel.EventListenerBreakpoint._instrumentation = 'instrumentation:';

/**
 * @implements {SDK.SDKModelObserver<!SDK.DOMDebuggerModel>}
 */
SDK.DOMDebuggerManager = class {
  constructor() {
    this._xhrBreakpointsSetting = Common.settings.createLocalSetting('xhrBreakpoints', []);
    /** @type {!Map<string, boolean>} */
    this._xhrBreakpoints = new Map();
    for (const breakpoint of this._xhrBreakpointsSetting.get())
      this._xhrBreakpoints.set(breakpoint.url, breakpoint.enabled);

    /** @type {!Array<!SDK.DOMDebuggerModel.EventListenerBreakpoint>} */
    this._eventListenerBreakpoints = [];
    this._createInstrumentationBreakpoints(
        Common.UIString('Animation'),
        ['requestAnimationFrame', 'cancelAnimationFrame', 'requestAnimationFrame.callback']);
    this._createInstrumentationBreakpoints(
        Common.UIString('Canvas'), ['canvasContextCreated', 'webglErrorFired', 'webglWarningFired']);
    this._createInstrumentationBreakpoints(
        Common.UIString('Geolocation'), ['Geolocation.getCurrentPosition', 'Geolocation.watchPosition']);
    this._createInstrumentationBreakpoints(Common.UIString('Notification'), ['Notification.requestPermission']);
    this._createInstrumentationBreakpoints(Common.UIString('Parse'), ['Element.setInnerHTML', 'Document.write']);
    this._createInstrumentationBreakpoints(Common.UIString('Script'), ['scriptFirstStatement', 'scriptBlockedByCSP']);
    this._createInstrumentationBreakpoints(
        Common.UIString('Timer'),
        ['setTimeout', 'clearTimeout', 'setInterval', 'clearInterval', 'setTimeout.callback', 'setInterval.callback']);
    this._createInstrumentationBreakpoints(Common.UIString('Window'), ['DOMWindow.close']);
    this._createInstrumentationBreakpoints(
        Common.UIString('WebAudio'),
        ['audioContextCreated', 'audioContextClosed', 'audioContextResumed', 'audioContextSuspended']);

    this._createEventListenerBreakpoints(
        Common.UIString('Media'),
        [
          'play',      'pause',          'playing',    'canplay',    'canplaythrough', 'seeking',
          'seeked',    'timeupdate',     'ended',      'ratechange', 'durationchange', 'volumechange',
          'loadstart', 'progress',       'suspend',    'abort',      'error',          'emptied',
          'stalled',   'loadedmetadata', 'loadeddata', 'waiting'
        ],
        ['audio', 'video']);
    this._createEventListenerBreakpoints(
        Common.UIString('Picture-in-Picture'), ['enterpictureinpicture', 'leavepictureinpicture'], ['video']);
    this._createEventListenerBreakpoints(Common.UIString('Picture-in-Picture'), ['resize'], ['PictureInPictureWindow']);
    this._createEventListenerBreakpoints(
        Common.UIString('Clipboard'), ['copy', 'cut', 'paste', 'beforecopy', 'beforecut', 'beforepaste'], ['*']);
    this._createEventListenerBreakpoints(
        Common.UIString('Control'),
        ['resize', 'scroll', 'zoom', 'focus', 'blur', 'select', 'change', 'submit', 'reset'], ['*']);
    this._createEventListenerBreakpoints(Common.UIString('Device'), ['deviceorientation', 'devicemotion'], ['*']);
    this._createEventListenerBreakpoints(
        Common.UIString('DOM Mutation'),
        [
          'DOMActivate', 'DOMFocusIn', 'DOMFocusOut', 'DOMAttrModified', 'DOMCharacterDataModified', 'DOMNodeInserted',
          'DOMNodeInsertedIntoDocument', 'DOMNodeRemoved', 'DOMNodeRemovedFromDocument', 'DOMSubtreeModified',
          'DOMContentLoaded'
        ],
        ['*']);
    this._createEventListenerBreakpoints(
        Common.UIString('Drag / drop'), ['drag', 'dragstart', 'dragend', 'dragenter', 'dragover', 'dragleave', 'drop'],
        ['*']);

    this._createEventListenerBreakpoints(Common.UIString('Keyboard'), ['keydown', 'keyup', 'keypress', 'input'], ['*']);
    this._createEventListenerBreakpoints(
        Common.UIString('Load'), ['load', 'beforeunload', 'unload', 'abort', 'error', 'hashchange', 'popstate'], ['*']);
    this._createEventListenerBreakpoints(
        Common.UIString('Mouse'),
        [
          'auxclick', 'click', 'dblclick', 'mousedown', 'mouseup', 'mouseover', 'mousemove', 'mouseout', 'mouseenter',
          'mouseleave', 'mousewheel', 'wheel', 'contextmenu'
        ],
        ['*']);
    this._createEventListenerBreakpoints(
        Common.UIString('Pointer'),
        [
          'pointerover', 'pointerout', 'pointerenter', 'pointerleave', 'pointerdown', 'pointerup', 'pointermove',
          'pointercancel', 'gotpointercapture', 'lostpointercapture'
        ],
        ['*']);
    this._createEventListenerBreakpoints(
        Common.UIString('Touch'), ['touchstart', 'touchmove', 'touchend', 'touchcancel'], ['*']);
    this._createEventListenerBreakpoints(Common.UIString('Worker'), ['message', 'messageerror'], ['*']);
    this._createEventListenerBreakpoints(
        Common.UIString('XHR'),
        ['readystatechange', 'load', 'loadstart', 'loadend', 'abort', 'error', 'progress', 'timeout'],
        ['xmlhttprequest', 'xmlhttprequestupload']);

    this._resolveEventListenerBreakpoint('instrumentation:setTimeout.callback')._title =
        Common.UIString('setTimeout fired');
    this._resolveEventListenerBreakpoint('instrumentation:setInterval.callback')._title =
        Common.UIString('setInterval fired');
    this._resolveEventListenerBreakpoint('instrumentation:scriptFirstStatement')._title =
        Common.UIString('Script First Statement');
    this._resolveEventListenerBreakpoint('instrumentation:scriptBlockedByCSP')._title =
        Common.UIString('Script Blocked by Content Security Policy');
    this._resolveEventListenerBreakpoint('instrumentation:requestAnimationFrame')._title =
        Common.UIString('Request Animation Frame');
    this._resolveEventListenerBreakpoint('instrumentation:cancelAnimationFrame')._title =
        Common.UIString('Cancel Animation Frame');
    this._resolveEventListenerBreakpoint('instrumentation:requestAnimationFrame.callback')._title =
        Common.UIString('Animation Frame Fired');
    this._resolveEventListenerBreakpoint('instrumentation:webglErrorFired')._title =
        Common.UIString('WebGL Error Fired');
    this._resolveEventListenerBreakpoint('instrumentation:webglWarningFired')._title =
        Common.UIString('WebGL Warning Fired');
    this._resolveEventListenerBreakpoint('instrumentation:Element.setInnerHTML')._title =
        Common.UIString('Set innerHTML');
    this._resolveEventListenerBreakpoint('instrumentation:canvasContextCreated')._title =
        Common.UIString('Create canvas context');
    this._resolveEventListenerBreakpoint('instrumentation:Geolocation.getCurrentPosition')._title =
        'getCurrentPosition';
    this._resolveEventListenerBreakpoint('instrumentation:Geolocation.watchPosition')._title = 'watchPosition';
    this._resolveEventListenerBreakpoint('instrumentation:Notification.requestPermission')._title = 'requestPermission';
    this._resolveEventListenerBreakpoint('instrumentation:DOMWindow.close')._title = 'window.close';
    this._resolveEventListenerBreakpoint('instrumentation:Document.write')._title = 'document.write';
    this._resolveEventListenerBreakpoint('instrumentation:audioContextCreated')._title =
        Common.UIString('Create AudioContext');
    this._resolveEventListenerBreakpoint('instrumentation:audioContextClosed')._title =
        Common.UIString('Close AudioContext');
    this._resolveEventListenerBreakpoint('instrumentation:audioContextResumed')._title =
        Common.UIString('Resume AudioContext');
    this._resolveEventListenerBreakpoint('instrumentation:audioContextSuspended')._title =
        Common.UIString('Suspend AudioContext');

    SDK.targetManager.observeModels(SDK.DOMDebuggerModel, this);
  }

  /**
   * @param {string} category
   * @param {!Array<string>} instrumentationNames
   */
  _createInstrumentationBreakpoints(category, instrumentationNames) {
    for (const instrumentationName of instrumentationNames) {
      this._eventListenerBreakpoints.push(
          new SDK.DOMDebuggerModel.EventListenerBreakpoint(instrumentationName, '', [], category, instrumentationName));
    }
  }

  /**
   * @param {string} category
   * @param {!Array<string>} eventNames
   * @param {!Array<string>} eventTargetNames
   */
  _createEventListenerBreakpoints(category, eventNames, eventTargetNames) {
    for (const eventName of eventNames) {
      this._eventListenerBreakpoints.push(
          new SDK.DOMDebuggerModel.EventListenerBreakpoint('', eventName, eventTargetNames, category, eventName));
    }
  }

  /**
   * @param {string} eventName
   * @param {string=} eventTargetName
   * @return {?SDK.DOMDebuggerModel.EventListenerBreakpoint}
   */
  _resolveEventListenerBreakpoint(eventName, eventTargetName) {
    const instrumentationPrefix = 'instrumentation:';
    const listenerPrefix = 'listener:';
    let instrumentationName = '';
    if (eventName.startsWith(instrumentationPrefix)) {
      instrumentationName = eventName.substring(instrumentationPrefix.length);
      eventName = '';
    } else if (eventName.startsWith(listenerPrefix)) {
      eventName = eventName.substring(listenerPrefix.length);
    } else {
      return null;
    }
    eventTargetName = (eventTargetName || '*').toLowerCase();
    let result = null;
    for (const breakpoint of this._eventListenerBreakpoints) {
      if (instrumentationName && breakpoint._instrumentationName === instrumentationName)
        result = breakpoint;
      if (eventName && breakpoint._eventName === eventName &&
          breakpoint._eventTargetNames.indexOf(eventTargetName) !== -1)
        result = breakpoint;
      if (!result && eventName && breakpoint._eventName === eventName &&
          breakpoint._eventTargetNames.indexOf('*') !== -1)
        result = breakpoint;
    }
    return result;
  }

  /**
   * @return {!Array<!SDK.DOMDebuggerModel.EventListenerBreakpoint>}
   */
  eventListenerBreakpoints() {
    return this._eventListenerBreakpoints.slice();
  }

  /**
   * @param {!Object} auxData
   * @return {string}
   */
  resolveEventListenerBreakpointTitle(auxData) {
    const id = auxData['eventName'];
    if (id === 'instrumentation:webglErrorFired' && auxData['webglErrorName']) {
      let errorName = auxData['webglErrorName'];
      // If there is a hex code of the error, display only this.
      errorName = errorName.replace(/^.*(0x[0-9a-f]+).*$/i, '$1');
      return Common.UIString('WebGL Error Fired (%s)', errorName);
    }
    if (id === 'instrumentation:scriptBlockedByCSP' && auxData['directiveText'])
      return Common.UIString('Script blocked due to Content Security Policy directive: %s', auxData['directiveText']);
    const breakpoint = this._resolveEventListenerBreakpoint(id, auxData['targetName']);
    if (!breakpoint)
      return '';
    if (auxData['targetName'])
      return auxData['targetName'] + '.' + breakpoint._title;
    return breakpoint._title;
  }

  /**
   * @param {!Object} auxData
   * @return {?SDK.DOMDebuggerModel.EventListenerBreakpoint}
   */
  resolveEventListenerBreakpoint(auxData) {
    return this._resolveEventListenerBreakpoint(auxData['eventName'], auxData['targetName']);
  }

  /**
   * @return {!Map<string, boolean>}
   */
  xhrBreakpoints() {
    return this._xhrBreakpoints;
  }

  _saveXHRBreakpoints() {
    const breakpoints = [];
    for (const url of this._xhrBreakpoints.keys())
      breakpoints.push({url: url, enabled: this._xhrBreakpoints.get(url)});
    this._xhrBreakpointsSetting.set(breakpoints);
  }

  /**
   * @param {string} url
   * @param {boolean} enabled
   */
  addXHRBreakpoint(url, enabled) {
    this._xhrBreakpoints.set(url, enabled);
    if (enabled) {
      for (const model of SDK.targetManager.models(SDK.DOMDebuggerModel))
        model._agent.setXHRBreakpoint(url);
    }
    this._saveXHRBreakpoints();
  }

  /**
   * @param {string} url
   */
  removeXHRBreakpoint(url) {
    const enabled = this._xhrBreakpoints.get(url);
    this._xhrBreakpoints.delete(url);
    if (enabled) {
      for (const model of SDK.targetManager.models(SDK.DOMDebuggerModel))
        model._agent.removeXHRBreakpoint(url);
    }
    this._saveXHRBreakpoints();
  }

  /**
   * @param {string} url
   * @param {boolean} enabled
   */
  toggleXHRBreakpoint(url, enabled) {
    this._xhrBreakpoints.set(url, enabled);
    for (const model of SDK.targetManager.models(SDK.DOMDebuggerModel)) {
      if (enabled)
        model._agent.setXHRBreakpoint(url);
      else
        model._agent.removeXHRBreakpoint(url);
    }
    this._saveXHRBreakpoints();
  }

  /**
   * @override
   * @param {!SDK.DOMDebuggerModel} domDebuggerModel
   */
  modelAdded(domDebuggerModel) {
    for (const url of this._xhrBreakpoints.keys()) {
      if (this._xhrBreakpoints.get(url))
        domDebuggerModel._agent.setXHRBreakpoint(url);
    }
    for (const breakpoint of this._eventListenerBreakpoints) {
      if (breakpoint._enabled)
        breakpoint._updateOnModel(domDebuggerModel);
    }
  }

  /**
   * @override
   * @param {!SDK.DOMDebuggerModel} domDebuggerModel
   */
  modelRemoved(domDebuggerModel) {
  }
};

/** @type {!SDK.DOMDebuggerManager} */
SDK.domDebuggerManager;
