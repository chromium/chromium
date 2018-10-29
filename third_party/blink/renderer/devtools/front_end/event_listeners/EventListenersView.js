// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @typedef {Array<{object: !SDK.RemoteObject, eventListeners: ?Array<!SDK.EventListener>, frameworkEventListeners: ?{eventListeners: ?Array<!SDK.EventListener>, internalHandlers: ?SDK.RemoteArray}, isInternal: ?Array<boolean>}>}
 */
EventListeners.EventListenersResult;

/**
 * @unrestricted
 */
EventListeners.EventListenersView = class extends UI.VBox {
  /**
   * @param {function()} changeCallback
   */
  constructor(changeCallback) {
    super();
    this._changeCallback = changeCallback;
    this._treeOutline = new UI.TreeOutlineInShadow();
    this._treeOutline.hideOverflow();
    this._treeOutline.registerRequiredCSS('object_ui/objectValue.css');
    this._treeOutline.registerRequiredCSS('event_listeners/eventListenersView.css');
    this._treeOutline.setComparator(EventListeners.EventListenersTreeElement.comparator);
    this._treeOutline.element.classList.add('monospace');
    this._treeOutline.setShowSelectionOnKeyboardFocus(true);
    this._treeOutline.setFocusable(true);
    this.element.appendChild(this._treeOutline.element);
    this._emptyHolder = createElementWithClass('div', 'gray-info-message');
    this._emptyHolder.textContent = Common.UIString('No event listeners');
    this._linkifier = new Components.Linkifier();
    /** @type {!Map<string, !EventListeners.EventListenersTreeElement>} */
    this._treeItemMap = new Map();
  }

  /**
   * @param {!Array<?SDK.RemoteObject>} objects
   * @return {!Promise<undefined>}
   */
  async addObjects(objects) {
    this.reset();
    await Promise.all(objects.map(obj => obj ? this._addObject(obj) : Promise.resolve()));
    this.addEmptyHolderIfNeeded();
    this._eventListenersArrivedForTest();
  }

  /**
   * @param {!SDK.RemoteObject} object
   * @return {!Promise<undefined>}
   */
  _addObject(object) {
    /** @type {!Array<!SDK.EventListener>} */
    let eventListeners;
    /** @type {?EventListeners.FrameworkEventListenersObject}*/
    let frameworkEventListenersObject = null;

    const promises = [];
    const domDebuggerModel = object.runtimeModel().target().model(SDK.DOMDebuggerModel);
    // TODO(kozyatinskiy): figure out how this should work for |window| when there is no DOMDebugger.
    if (domDebuggerModel)
      promises.push(domDebuggerModel.eventListeners(object).then(storeEventListeners));
    promises.push(EventListeners.frameworkEventListeners(object).then(storeFrameworkEventListenersObject));
    return Promise.all(promises).then(markInternalEventListeners).then(addEventListeners.bind(this));

    /**
     * @param {!Array<!SDK.EventListener>} result
     */
    function storeEventListeners(result) {
      eventListeners = result;
    }

    /**
     * @param {?EventListeners.FrameworkEventListenersObject} result
     */
    function storeFrameworkEventListenersObject(result) {
      frameworkEventListenersObject = result;
    }

    /**
     * @return {!Promise<undefined>}
     */
    function markInternalEventListeners() {
      if (!frameworkEventListenersObject.internalHandlers)
        return Promise.resolve(undefined);
      return frameworkEventListenersObject.internalHandlers.object()
          .callFunctionJSON(isInternalEventListener, eventListeners.map(handlerArgument))
          .then(setIsInternal);

      /**
       * @param {!SDK.EventListener} listener
       * @return {!Protocol.Runtime.CallArgument}
       */
      function handlerArgument(listener) {
        return SDK.RemoteObject.toCallArgument(listener.handler());
      }

      /**
       * @suppressReceiverCheck
       * @return {!Array<boolean>}
       * @this {Array<*>}
       */
      function isInternalEventListener() {
        const isInternal = [];
        const internalHandlersSet = new Set(this);
        for (const handler of arguments)
          isInternal.push(internalHandlersSet.has(handler));
        return isInternal;
      }

      /**
       * @param {!Array<boolean>} isInternal
       */
      function setIsInternal(isInternal) {
        for (let i = 0; i < eventListeners.length; ++i) {
          if (isInternal[i])
            eventListeners[i].markAsFramework();
        }
      }
    }

    /**
     * @this {EventListeners.EventListenersView}
     */
    function addEventListeners() {
      this._addObjectEventListeners(object, eventListeners);
      this._addObjectEventListeners(object, frameworkEventListenersObject.eventListeners);
    }
  }

  /**
   * @param {!SDK.RemoteObject} object
   * @param {?Array<!SDK.EventListener>} eventListeners
   */
  _addObjectEventListeners(object, eventListeners) {
    if (!eventListeners)
      return;
    for (const eventListener of eventListeners) {
      const treeItem = this._getOrCreateTreeElementForType(eventListener.type());
      treeItem.addObjectEventListener(eventListener, object);
    }
  }

  /**
   * @param {boolean} showFramework
   * @param {boolean} showPassive
   * @param {boolean} showBlocking
   */
  showFrameworkListeners(showFramework, showPassive, showBlocking) {
    const eventTypes = this._treeOutline.rootElement().children();
    for (const eventType of eventTypes) {
      let hiddenEventType = true;
      for (const listenerElement of eventType.children()) {
        const listenerOrigin = listenerElement.eventListener().origin();
        let hidden = false;
        if (listenerOrigin === SDK.EventListener.Origin.FrameworkUser && !showFramework)
          hidden = true;
        if (listenerOrigin === SDK.EventListener.Origin.Framework && showFramework)
          hidden = true;
        if (!showPassive && listenerElement.eventListener().passive())
          hidden = true;
        if (!showBlocking && !listenerElement.eventListener().passive())
          hidden = true;
        listenerElement.hidden = hidden;
        hiddenEventType = hiddenEventType && hidden;
      }
      eventType.hidden = hiddenEventType;
    }
  }

  /**
   * @param {string} type
   * @return {!EventListeners.EventListenersTreeElement}
   */
  _getOrCreateTreeElementForType(type) {
    let treeItem = this._treeItemMap.get(type);
    if (!treeItem) {
      treeItem = new EventListeners.EventListenersTreeElement(type, this._linkifier, this._changeCallback);
      this._treeItemMap.set(type, treeItem);
      treeItem.hidden = true;
      this._treeOutline.appendChild(treeItem);
    }
    this._emptyHolder.remove();
    return treeItem;
  }

  addEmptyHolderIfNeeded() {
    let allHidden = true;
    let firstVisibleChild = null;
    for (const eventType of this._treeOutline.rootElement().children()) {
      eventType.hidden = !eventType.firstChild();
      allHidden = allHidden && eventType.hidden;
      if (!firstVisibleChild && !eventType.hidden)
        firstVisibleChild = eventType;
    }
    if (allHidden && !this._emptyHolder.parentNode)
      this.element.appendChild(this._emptyHolder);
    if (firstVisibleChild)
      firstVisibleChild.select(true /* omitFocus */);
  }

  reset() {
    const eventTypes = this._treeOutline.rootElement().children();
    for (const eventType of eventTypes)
      eventType.removeChildren();
    this._linkifier.reset();
  }

  _eventListenersArrivedForTest() {
  }
};

/**
 * @unrestricted
 */
EventListeners.EventListenersTreeElement = class extends UI.TreeElement {
  /**
   * @param {string} type
   * @param {!Components.Linkifier} linkifier
   * @param {function()} changeCallback
   */
  constructor(type, linkifier, changeCallback) {
    super(type);
    this.toggleOnClick = true;
    this._linkifier = linkifier;
    this._changeCallback = changeCallback;
  }

  /**
   * @param {!UI.TreeElement} element1
   * @param {!UI.TreeElement} element2
   * @return {number}
   */
  static comparator(element1, element2) {
    if (element1.title === element2.title)
      return 0;
    return element1.title > element2.title ? 1 : -1;
  }

  /**
   * @param {!SDK.EventListener} eventListener
   * @param {!SDK.RemoteObject} object
   */
  addObjectEventListener(eventListener, object) {
    const treeElement =
        new EventListeners.ObjectEventListenerBar(eventListener, object, this._linkifier, this._changeCallback);
    this.appendChild(/** @type {!UI.TreeElement} */ (treeElement));
  }
};


/**
 * @unrestricted
 */
EventListeners.ObjectEventListenerBar = class extends UI.TreeElement {
  /**
   * @param {!SDK.EventListener} eventListener
   * @param {!SDK.RemoteObject} object
   * @param {!Components.Linkifier} linkifier
   * @param {function()} changeCallback
   */
  constructor(eventListener, object, linkifier, changeCallback) {
    super('', true);
    this._eventListener = eventListener;
    this.editable = false;
    this._setTitle(object, linkifier);
    this._changeCallback = changeCallback;
  }

  /**
   * @override
   */
  onpopulate() {
    const properties = [];
    const eventListener = this._eventListener;
    const runtimeModel = eventListener.domDebuggerModel().runtimeModel();
    properties.push(runtimeModel.createRemotePropertyFromPrimitiveValue('useCapture', eventListener.useCapture()));
    properties.push(runtimeModel.createRemotePropertyFromPrimitiveValue('passive', eventListener.passive()));
    properties.push(runtimeModel.createRemotePropertyFromPrimitiveValue('once', eventListener.once()));
    if (typeof eventListener.handler() !== 'undefined')
      properties.push(new SDK.RemoteObjectProperty('handler', eventListener.handler()));
    ObjectUI.ObjectPropertyTreeElement.populateWithProperties(this, properties, [], true, null);
  }

  /**
   * @param {!SDK.RemoteObject} object
   * @param {!Components.Linkifier} linkifier
   */
  _setTitle(object, linkifier) {
    const title = this.listItemElement.createChild('span');
    const subtitle = this.listItemElement.createChild('span', 'event-listener-tree-subtitle');
    subtitle.appendChild(linkifier.linkifyRawLocation(this._eventListener.location(), this._eventListener.sourceURL()));

    title.appendChild(
        ObjectUI.ObjectPropertiesSection.createValueElement(object, false /* wasThrown */, false /* showPreview */));

    if (this._eventListener.canRemove()) {
      const deleteButton = title.createChild('span', 'event-listener-button');
      deleteButton.textContent = Common.UIString('Remove');
      deleteButton.title = Common.UIString('Delete event listener');
      deleteButton.addEventListener('click', removeListener.bind(this), false);
      title.appendChild(deleteButton);
    }

    if (this._eventListener.isScrollBlockingType() && this._eventListener.canTogglePassive()) {
      const passiveButton = title.createChild('span', 'event-listener-button');
      passiveButton.textContent = Common.UIString('Toggle Passive');
      passiveButton.title = Common.UIString('Toggle whether event listener is passive or blocking');
      passiveButton.addEventListener('click', togglePassiveListener.bind(this), false);
      title.appendChild(passiveButton);
    }

    /**
     * @param {!Event} event
     * @this {EventListeners.ObjectEventListenerBar}
     */
    function removeListener(event) {
      event.consume();
      this._removeListenerBar();
      this._eventListener.remove();
    }

    /**
     * @param {!Event} event
     * @this {EventListeners.ObjectEventListenerBar}
     */
    function togglePassiveListener(event) {
      event.consume();
      this._eventListener.togglePassive().then(this._changeCallback());
    }
  }

  _removeListenerBar() {
    const parent = this.parent;
    parent.removeChild(this);
    if (!parent.childCount())
      parent.collapse();
    let allHidden = true;
    for (let i = 0; i < parent.childCount(); ++i) {
      if (!parent.childAt(i).hidden)
        allHidden = false;
    }
    parent.hidden = allHidden;
  }

  /**
   * @return {!SDK.EventListener}
   */
  eventListener() {
    return this._eventListener;
  }
};
