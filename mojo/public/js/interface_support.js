// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

goog.require('mojo.interfaceControl.RUN_MESSAGE_ID');
goog.require('mojo.interfaceControl.RunResponseMessageParamsSpec');
goog.require('mojo.internal');

goog.provide('mojo.internal.interfaceSupport');


/**
 * Handles incoming interface control messages on a remote or router endpoint.
 */
mojo.internal.interfaceSupport.ControlMessageHandler = class {
  /** @param {!MojoHandle} handle */
  constructor(handle) {
    /** @private {!MojoHandle} */
    this.handle_ = handle;

    /** @private {!Map<number, function()>} */
    this.pendingFlushResolvers_ = new Map;
  }

  sendRunMessage(requestId, input) {
    return new Promise(resolve => {
      mojo.internal.serializeAndSendMessage(
          this.handle_, mojo.interfaceControl.RUN_MESSAGE_ID, requestId,
          mojo.internal.kMessageFlagExpectsResponse,
          mojo.interfaceControl.RunMessageParamsSpec.$, {'input': input});
      this.pendingFlushResolvers_.set(requestId, resolve);
    });
  }

  maybeHandleControlMessage(header, buffer) {
    if (header.ordinal === mojo.interfaceControl.RUN_MESSAGE_ID) {
      const data = new DataView(buffer, header.headerSize);
      const decoder = new mojo.internal.Decoder(data, []);
      if (header.flags & mojo.internal.kMessageFlagExpectsResponse)
        return this.handleRunRequest_(header.requestId, decoder);
      else
        return this.handleRunResponse_(header.requestId, decoder);
    }

    return false;
  }

  handleRunRequest_(requestId, decoder) {
    const input = decoder.decodeStructInline(
        mojo.interfaceControl.RunMessageParamsSpec.$.$.structSpec)['input'];
    if (input.hasOwnProperty('flushForTesting')) {
      mojo.internal.serializeAndSendMessage(
          this.handle_, mojo.interfaceControl.RUN_MESSAGE_ID, requestId,
          mojo.internal.kMessageFlagIsResponse,
          mojo.interfaceControl.RunResponseMessageParamsSpec.$,
          {'output': null});
      return true;
    }

    return false;
  }

  handleRunResponse_(requestId, decoder) {
    const resolver = this.pendingFlushResolvers_.get(requestId);
    if (!resolver)
      return false;

    resolver();
    return true;
  }
};

/**
 * Captures metadata about a request which was sent by a remote, for which a
 * response is expected.
 */
mojo.internal.interfaceSupport.PendingResponse = class {
  /**
   * @param {number} requestId
   * @param {number} ordinal
   * @param {!mojo.internal.MojomType} responseStruct
   * @param {!Function} resolve
   * @param {!Function} reject
   * @private
   */
  constructor(requestId, ordinal, responseStruct, resolve, reject) {
    /** @public {number} */
    this.requestId = requestId;

    /** @public {number} */
    this.ordinal = ordinal;

    /** @public {!mojo.internal.MojomType} */
    this.responseStruct = responseStruct;

    /** @public {!Function} */
    this.resolve = resolve;

    /** @public {!Function} */
    this.reject = reject;
  }
};

/**
 * Exposed by endpoints to allow observation of remote peer closure. Any number
 * of listeners may be registered on a ConnectionErrorEventRouter, and the
 * router will dispatch at most one event in its lifetime, whenever its
 * associated bindings endpoint detects peer closure.
 * @export
 */
mojo.internal.interfaceSupport.ConnectionErrorEventRouter = class {
  /** @public */
  constructor() {
    /** @type {!Map<number, !Function>} */
    this.listeners = new Map;

    /** @private {number} */
    this.nextListenerId_ = 0;
  }

  /**
   * @param {!Function} listener
   * @return {number} An ID which can be given to removeListener() to remove
   *     this listener.
   * @export
   */
  addListener(listener) {
    const id = ++this.nextListenerId_;
    this.listeners.set(id, listener);
    return id;
  }

  /**
   * @param {number} id An ID returned by a prior call to addListener.
   * @return {boolean} True iff the identified listener was found and removed.
   * @export
   */
  removeListener(id) {
    return this.listeners.delete(id);
  }

  /**
   * Notifies all listeners of a connection error.
   */
  dispatchErrorEvent() {
    for (const listener of this.listeners.values())
      listener();
  }
};

/**
 * @interface
 * @export
 */
mojo.internal.interfaceSupport.PendingReceiver = class {
  /**
   * @return {!MojoHandle}
   * @export
   */
  get handle() {}
};

/**
 * Generic helper used to implement all generated remote classes. Knows how to
 * serialize requests and deserialize their replies, both according to
 * declarative message structure specs.
 *
 * @template {!mojo.internal.interfaceSupport.PendingReceiver} T
 * @export
 */
mojo.internal.interfaceSupport.InterfaceRemoteBase = class {
  /**
   * @param {!function(new:T, !MojoHandle)} requestType
   * @param {MojoHandle=} opt_handle The message pipe handle to use as a remote
   *     endpoint. If null, this object must be bound with bindHandle before
   *     it can be used to send any messages.
   * @public
   */
  constructor(requestType, opt_handle) {
    /** @public {?MojoHandle} */
    this.handle = null;

    /** @private {!function(new:T, !MojoHandle)} */
    this.requestType_ = requestType;

    /** @private {?mojo.internal.interfaceSupport.HandleReader} */
    this.reader_ = null;

    /** @private {number} */
    this.nextRequestId_ = 0;

    /**
     * @private {!Map<number, !mojo.internal.interfaceSupport.PendingResponse>}
     */
    this.pendingResponses_ = new Map;

    /** @private {mojo.internal.interfaceSupport.ControlMessageHandler} */
    this.controlMessageHandler_ = null;

    /** @private {!mojo.internal.interfaceSupport.ConnectionErrorEventRouter} */
    this.connectionErrorEventRouter_ =
        new mojo.internal.interfaceSupport.ConnectionErrorEventRouter;

    if (opt_handle instanceof MojoHandle)
      this.bindHandle(opt_handle);
  }

  /**
   * @return {!T}
   */
  bindNewPipeAndPassReceiver() {
    let {handle0, handle1} = Mojo.createMessagePipe();
    this.bindHandle(handle0);
    return new this.requestType_(handle1);
  }

  /**
   * @param {!MojoHandle} handle
   * @export
   */
  bindHandle(handle) {
    if (this.handle)
      throw new Error('Remote already bound.');
    this.handle = handle;

    const reader = new mojo.internal.interfaceSupport.HandleReader(handle);
    reader.onRead = this.onMessageReceived_.bind(this);
    reader.onError = this.onError_.bind(this);
    reader.start();
    this.controlMessageHandler_ =
        new mojo.internal.interfaceSupport.ControlMessageHandler(handle);

    this.reader_ = reader;
    this.nextRequestId_ = 0;
    this.pendingResponses_ = new Map;
  }

  /** @export */
  unbind() {
    if (this.reader_)
      this.reader_.stop();
  }

  /** @export */
  close() {
    this.cleanupAndFlushPendingResponses_('Message pipe closed.');
  }

  /**
   * @return {!mojo.internal.interfaceSupport.ConnectionErrorEventRouter}
   * @export
   */
  getConnectionErrorEventRouter() {
    return this.connectionErrorEventRouter_;
  }

  /**
   * @param {number} ordinal
   * @param {!mojo.internal.MojomType} paramStruct
   * @param {?mojo.internal.MojomType} responseStruct
   * @param {!Array} args
   * @return {!Promise}
   * @export
   */
  sendMessage(ordinal, paramStruct, responseStruct, args) {
    if (!this.handle) {
      throw new Error(
        'Attempting to use an unbound remote. Try ' +
        '$.bindNewPipeAndPassReceiver() first.')
    }

    // The pipe has already been closed, so just drop the message.
    if (responseStruct && (!this.reader_ || this.reader_.isStopped()))
      return Promise.reject(new Error('The pipe has already been closed.'));

    const requestId = this.nextRequestId_++;
    const value = {};
    paramStruct.$.structSpec.fields.forEach(
        (field, index) => value[field.name] = args[index]);
    mojo.internal.serializeAndSendMessage(
        this.handle, ordinal, requestId,
        responseStruct ? mojo.internal.kMessageFlagExpectsResponse : 0,
        paramStruct, value);
    if (!responseStruct)
      return Promise.resolve();

    return new Promise((resolve, reject) => {
      this.pendingResponses_.set(
          requestId,
          new mojo.internal.interfaceSupport.PendingResponse(
              requestId, ordinal,
              /** @type {!mojo.internal.MojomType} */ (responseStruct), resolve,
              reject));
    });
  }

  /**
   * @return {!Promise}
   * @export
   */
  flushForTesting() {
    return this.controlMessageHandler_.sendRunMessage(
        this.nextRequestId_++, {'flushForTesting': {}});
  }

  /**
   * @param {!ArrayBuffer} buffer
   * @param {!Array<MojoHandle>} handles
   * @private
   */
  onMessageReceived_(buffer, handles) {
    const data = new DataView(buffer);
    const header = mojo.internal.deserializeMessageHeader(data);
    if (this.controlMessageHandler_.maybeHandleControlMessage(header, buffer))
      return;
    if (!(header.flags & mojo.internal.kMessageFlagIsResponse) ||
        header.flags & mojo.internal.kMessageFlagExpectsResponse) {
      return this.onError_('Received unexpected request message');
    }
    const pendingResponse = this.pendingResponses_.get(header.requestId);
    this.pendingResponses_.delete(header.requestId);
    if (!pendingResponse)
      return this.onError_('Received unexpected response message');
    const decoder = new mojo.internal.Decoder(
        new DataView(buffer, header.headerSize), handles);
    const responseValue = decoder.decodeStructInline(
        /** @type {!mojo.internal.StructSpec} */ (
            pendingResponse.responseStruct.$.structSpec));
    if (!responseValue)
      return this.onError_('Received malformed response message');
    if (header.ordinal !== pendingResponse.ordinal)
      return this.onError_('Received malformed response message');

    pendingResponse.resolve(responseValue);
  }

  /**
   * @param {string=} opt_reason
   * @private
   */
  onError_(opt_reason) {
    this.cleanupAndFlushPendingResponses_(opt_reason);
    this.connectionErrorEventRouter_.dispatchErrorEvent();
  }

  /**
   * @param {string=} opt_reason
   * @private
   */
  cleanupAndFlushPendingResponses_(opt_reason) {
    this.reader_.stopAndCloseHandle();
    this.reader_ = null;
    for (const id of this.pendingResponses_.keys())
      this.pendingResponses_.get(id).reject(new Error(opt_reason));
    this.pendingResponses_ = new Map;
  }
};

/**
 * Wrapper around mojo.internal.interfaceSupport.InterfaceRemoteBase that
 * exposes the subset of InterfaceRemoteBase's method that users are allowed
 * to use.
 * @template T
 * @export
 */
mojo.internal.interfaceSupport.InterfaceRemoteBaseWrapper = class {
  /**
   * @param {!mojo.internal.interfaceSupport.InterfaceRemoteBase<T>} remote
   * @public
   */
  constructor(remote) {
    /** @private {!mojo.internal.interfaceSupport.InterfaceRemoteBase<T>} */
    this.remote_ = remote;
  }

  /**
   * @return {!T}
   * @export
   */
  bindNewPipeAndPassReceiver() {
    return this.remote_.bindNewPipeAndPassReceiver();
  }

  /** @export */
  close() {
    this.remote_.close();
  }

  /**
   * @return {!Promise}
   * @export
   */
  flushForTesting() {
    return this.remote_.flushForTesting();
  }
}

/**
 * Helper used by generated EventRouter types to dispatch incoming interface
 * messages as Event-like things.
 * @export
 */
mojo.internal.interfaceSupport.CallbackRouter = class {
  constructor() {
    /** @type {!Map<number, !Function>} */
    this.removeCallbacks = new Map;

    /** @private {number} */
    this.nextListenerId_ = 0;
  }

  /** @return {number} */
  getNextId() {
    return ++this.nextListenerId_;
  }

  /**
   * @param {number} id An ID returned by a prior call to addListener.
   * @return {boolean} True iff the identified listener was found and removed.
   * @export
   */
  removeListener(id) {
    this.removeCallbacks.get(id)();
    return this.removeCallbacks.delete(id);
  }
};

/**
 * Helper used by generated CallbackRouter types to dispatch incoming interface
 * messages to listeners.
 * @export
 */
mojo.internal.interfaceSupport.InterfaceCallbackReceiver = class {
  /**
   * @public
   * @param {!mojo.internal.interfaceSupport.CallbackRouter} callbackRouter
   */
  constructor(callbackRouter) {
    /** @private {!Map<number, !Function>} */
    this.listeners_ = new Map;

    /** @private {!mojo.internal.interfaceSupport.CallbackRouter} */
    this.callbackRouter_ = callbackRouter;
  }

  /**
   * @param {!Function} listener
   * @return {number} A unique ID for the added listener.
   * @export
   */
  addListener(listener) {
    const id = this.callbackRouter_.getNextId();
    this.listeners_.set(id, listener);
    this.callbackRouter_.removeCallbacks.set(id, () => {
      return this.listeners_.delete(id);
    });
    return id;
  }

  /**
   * @param {boolean} expectsResponse
   * @return {!Function}
   * @export
   */
  createReceiverHandler(expectsResponse) {
    if (expectsResponse)
      return this.dispatchWithResponse_.bind(this);
    return this.dispatch_.bind(this);
  }

  /**
   * @param {...*} varArgs
   * @private
   */
  dispatch_(varArgs) {
    const args = Array.from(arguments);
    this.listeners_.forEach(listener => listener.apply(null, args));
  }

  /**
   * @param {...*} varArgs
   * @return {?Object}
   * @private
   */
  dispatchWithResponse_(varArgs) {
    const args = Array.from(arguments);
    const returnValues = Array.from(this.listeners_.values())
                             .map(listener => listener.apply(null, args));

    let returnValue;
    for (const value of returnValues) {
      if (value === undefined)
        continue;
      if (returnValue !== undefined)
        throw new Error('Multiple listeners attempted to reply to a message');
      returnValue = value;
    }

    return returnValue;
  }
};

/**
 * Wraps message handlers attached to an InterfaceReceiver.
 */
mojo.internal.interfaceSupport.MessageHandler = class {
  /**
   * @param {!mojo.internal.MojomType} paramStruct
   * @param {?mojo.internal.MojomType} responseStruct
   * @param {!Function} handler
   * @private
   */
  constructor(paramStruct, responseStruct, handler) {
    /** @public {!mojo.internal.MojomType} */
    this.paramStruct = paramStruct;

    /** @public {?mojo.internal.MojomType} */
    this.responseStruct = responseStruct;

    /** @public {!Function} */
    this.handler = handler;
  }
};

/**
 * Generic helper that listens for incoming request messages on a message pipe,
 * dispatching them to any registered handlers. Handlers are registered against
 * a specific ordinal message number. It has methods to perform operations
 * related to the interface pipe e.g. bind the pipe, close it, etc. Should only
 * be used by the generated receiver classes.
 * @template T
 * @export
 */
mojo.internal.interfaceSupport.InterfaceReceiverHelperInternal = class {
  /**
   * @param {!function(new:T)} remoteType
   * @public
   */
  constructor(remoteType) {
    /**
     * @private {!Map<MojoHandle,
     *     !mojo.internal.interfaceSupport.HandleReader>}
     */
    this.readers_ = new Map;

    /** @private {!function(new:T)} */
    this.remoteType_ = remoteType;
    /**
     * @private {!Map<number, !mojo.internal.interfaceSupport.MessageHandler>}
     */
    this.messageHandlers_ = new Map;

    /** @private {mojo.internal.interfaceSupport.ControlMessageHandler} */
    this.controlMessageHandler_ = null;

    /** @private {!mojo.internal.interfaceSupport.ConnectionErrorEventRouter} */
    this.connectionErrorEventRouter_ =
      new mojo.internal.interfaceSupport.ConnectionErrorEventRouter;
  }

  /**
   * @param {number} ordinal
   * @param {!mojo.internal.MojomType} paramStruct
   * @param {?mojo.internal.MojomType} responseStruct
   * @param {!Function} handler
   * @export
   */
  registerHandler(ordinal, paramStruct, responseStruct, handler) {
    this.messageHandlers_.set(
        ordinal,
        new mojo.internal.interfaceSupport.MessageHandler(
            paramStruct, responseStruct, handler));
  }

  /**
   * @param {!MojoHandle} handle
   * @export
   */
  bindHandle(handle) {
    const reader = new mojo.internal.interfaceSupport.HandleReader(handle);
    this.readers_.set(handle, reader);
    reader.onRead = this.onMessageReceived_.bind(this, handle);
    reader.onError = this.onError_.bind(this, handle);
    reader.start();
    this.controlMessageHandler_ =
        new mojo.internal.interfaceSupport.ControlMessageHandler(handle);
  }

  /**
   * @return {!T}
   * @export
   */
  bindNewPipeAndPassRemote() {
    let remote = new this.remoteType_;
    this.bindHandle(remote.$.bindNewPipeAndPassReceiver().handle);
    return remote;
  }

  /** @export */
  closeBindings() {
    for (const reader of this.readers_.values())
      reader.stopAndCloseHandle();
    this.readers_.clear();
  }

  /**
   * @return {!mojo.internal.interfaceSupport.ConnectionErrorEventRouter}
   * @export
   */
  getConnectionErrorEventRouter() {
    return this.connectionErrorEventRouter_;
  }

  /**
   * @param {!MojoHandle} handle
   * @param {!ArrayBuffer} buffer
   * @param {!Array<MojoHandle>} handles
   * @private
   */
  onMessageReceived_(handle, buffer, handles) {
    const data = new DataView(buffer);
    const header = mojo.internal.deserializeMessageHeader(data);
    if (this.controlMessageHandler_.maybeHandleControlMessage(header, buffer))
      return;
    if (header.flags & mojo.internal.kMessageFlagIsResponse)
      throw new Error('Received unexpected response on interface receiver');
    const handler = this.messageHandlers_.get(header.ordinal);
    if (!handler)
      throw new Error('Received unknown message');
    const decoder = new mojo.internal.Decoder(
        new DataView(buffer, header.headerSize), handles);
    const request = decoder.decodeStructInline(
        /** @type {!mojo.internal.StructSpec} */ (
            handler.paramStruct.$.structSpec));
    if (!request)
      throw new Error('Received malformed message');

    let result = handler.handler.apply(
        null,
        handler.paramStruct.$.structSpec.fields.map(
            field => request[field.name]));

    // If the message expects a response, the handler must return either a
    // well-formed response object, or a Promise that will eventually yield one.
    if (handler.responseStruct) {
      if (result === undefined) {
        this.onError_(handle);
        throw new Error(
            'Message expects a reply but its handler did not provide one.');
      }

      if (!(result instanceof Promise))
        result = Promise.resolve(result);

      result
          .then(value => {
            mojo.internal.serializeAndSendMessage(
                handle, header.ordinal, header.requestId,
                mojo.internal.kMessageFlagIsResponse,
                /** @type {!mojo.internal.MojomType} */
                (handler.responseStruct), value);
          })
          .catch(() => {
            // If the handler rejects, that means it didn't like the request's
            // contents for whatever reason. We close the binding to prevent
            // further messages from being received from that client.
            this.onError_(handle);
          });
    }
  }

  /**
   * @param {!MojoHandle} handle
   * @private
   */
  onError_(handle) {
    const reader = this.readers_.get(handle);
    if (!reader)
      return;
    this.connectionErrorEventRouter_.dispatchErrorEvent();
    reader.stopAndCloseHandle();
    this.readers_.delete(handle);
  }
};

/**
 * Generic helper used to perform operations related to the interface pipe e.g.
 * bind the pipe, close it, flush it for testing, etc. Wraps
 * mojo.internal.interfaceSupport.InterfaceReceiverHelperInternal and exposes a
 * subset of methods that meant to be used by users of a receiver class.
 *
 * @template T
 * @export
 */
mojo.internal.interfaceSupport.InterfaceReceiverHelper = class {
  /**
   * @param {!mojo.internal.interfaceSupport.InterfaceReceiverHelperInternal<T>}
   *     helper_internal
   * @public
   */
  constructor(helper_internal) {
    /**
     * @private {!mojo.internal.interfaceSupport.InterfaceReceiverHelperInternal<T>}
     */
    this.helper_internal_ = helper_internal;
  }

  /**
   * Binds a new handle to this object. Messages which arrive on the handle will
   * be read and dispatched to this object.
   *
   * @param {!MojoHandle} handle
   * @export
   */
  bindHandle(handle) {
    this.helper_internal_.bindHandle(handle);
  }

  /**
   * @return {!T}
   * @export
   */
  bindNewPipeAndPassRemote() {
    return this.helper_internal_.bindNewPipeAndPassRemote();
  }

  /** @export */
  close() {
    this.helper_internal_.closeBindings();
  }
}

/**
 * Watches a MojoHandle for readability or peer closure, forwarding either event
 * to one of two callbacks on the reader. Used by both InterfaceRemoteBase and
 * InterfaceReceiverHelperInternal to watch for incoming messages.
 */
mojo.internal.interfaceSupport.HandleReader = class {
  /**
   * @param {!MojoHandle} handle
   * @private
   */
  constructor(handle) {
    /** @private {MojoHandle} */
    this.handle_ = handle;

    /** @public {?function(!ArrayBuffer, !Array<MojoHandle>)} */
    this.onRead = null;

    /** @public {!Function} */
    this.onError = () => {};

    /** @public {?MojoWatcher} */
    this.watcher_ = null;
  }

  isStopped() {
    return this.watcher_ === null;
  }

  start() {
    this.watcher_ = this.handle_.watch({readable: true}, this.read_.bind(this));
  }

  stop() {
    if (!this.watcher_)
      return;
    this.watcher_.cancel();
    this.watcher_ = null;
  }

  stopAndCloseHandle() {
    if (!this.watcher_)
      return;
    this.stop();
    this.handle_.close();
  }

  /** @private */
  read_(result) {
    for (;;) {
      if (!this.watcher_)
        return;

      const read = this.handle_.readMessage();

      // No messages available.
      if (read.result == Mojo.RESULT_SHOULD_WAIT)
        return;

      // Remote endpoint has been closed *and* no messages available.
      if (read.result == Mojo.RESULT_FAILED_PRECONDITION) {
        this.onError();
        return;
      }

      // Something terrible happened.
      if (read.result != Mojo.RESULT_OK)
        throw new Error('Unexpected error on HandleReader: ' + read.result);

      this.onRead(read.buffer, read.handles);
    }
  }
};
