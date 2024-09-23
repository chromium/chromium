// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Owns a single message pipe handle and facilitates message sending and routing
 * on behalf of all the pipe's local Endpoints.
 */
mojo.internal.interfaceSupport.Router = class {
  /**
   * @param {!MojoHandle} pipe
   * @param {boolean} setNamespaceBit
   * @public
   */
  constructor(pipe, setNamespaceBit) {
    /** @const {!MojoHandle} */
    this.pipe_ = pipe;

    /** @const {!mojo.internal.interfaceSupport.HandleReader} */
    this.reader_ = new mojo.internal.interfaceSupport.HandleReader(pipe);
    this.reader_.onRead = this.onMessageReceived_.bind(this);
    this.reader_.onError = this.onError_.bind(this);

    /** @const {!Map<number, !mojo.internal.interfaceSupport.Endpoint>} */
    this.endpoints_ = new Map();

    /** @private {number} */
    this.nextInterfaceId_ = 1;

    /** @const {number} */
    this.interfaceIdNamespace_ =
        setNamespaceBit ? mojo.internal.kInterfaceNamespaceBit : 0;

    /** @const {!mojo.internal.interfaceSupport.PipeControlMessageHandler} */
    this.pipeControlHandler_ =
        new mojo.internal.interfaceSupport.PipeControlMessageHandler(
            this, this.onPeerEndpointClosed_.bind(this));
  }

  /** @return {!MojoHandle} */
  get pipe() {
    return this.pipe_;
  }

  /** @return {number} */
  generateInterfaceId() {
    return (this.nextInterfaceId_++ | this.interfaceIdNamespace_) >>> 0;
  }

  /**
   * @param {!mojo.internal.interfaceSupport.Endpoint} endpoint
   * @param {number} interfaceId
   */
  addEndpoint(endpoint, interfaceId) {
    if (interfaceId === 0) {
      this.reader_.start();
    }
    console.assert(
        this.isReading(), 'adding a secondary endpoint with no primary');
    this.endpoints_.set(interfaceId, endpoint);
  }

  /** @param {number} interfaceId */
  removeEndpoint(interfaceId) {
    this.endpoints_.delete(interfaceId);
    if (interfaceId === 0) {
      this.reader_.stop();
    }
  }

  close() {
    console.assert(
        this.endpoints_.size === 0,
        'closing primary endpoint with secondary endpoints still bound');
    this.reader_.stopAndCloseHandle();
  }

  /** @param {number} interfaceId */
  closeEndpoint(interfaceId) {
    this.removeEndpoint(interfaceId);
    if (interfaceId === 0) {
      this.close();
    } else {
      this.pipeControlHandler_.notifyEndpointClosed(interfaceId);
    }
  }

  /** @return {boolean} */
  isReading() {
    return !this.reader_.isStopped();
  }

  /** @param {!mojo.internal.Message} message */
  send(message) {
    this.pipe_.writeMessage(message.buffer, message.handles);
  }

  /**
   * @param {!ArrayBuffer} buffer
   * @param {!Array<MojoHandle>} handles
   */
  onMessageReceived_(buffer, handles) {
    if (buffer.byteLength < mojo.internal.kMessageV0HeaderSize) {
      console.error('Rejecting undersized message');
      this.onError_();
      return;
    }

    const header = mojo.internal.deserializeMessageHeader(new DataView(buffer));
    if (this.pipeControlHandler_.maybeHandleMessage(header, buffer)) {
      return;
    }

    const endpoint = this.endpoints_.get(header.interfaceId);
    if (!endpoint) {
      console.error(
          `Received message for unknown endpoint ${header.interfaceId}`);
      return;
    }

    endpoint.onMessageReceived(header, buffer, handles);
  }

  onError_() {
    for (const endpoint of this.endpoints_.values()) {
      endpoint.onError();
    }
    this.endpoints_.clear();
  }

  /** @param {number} id */
  onPeerEndpointClosed_(id) {
    const endpoint = this.endpoints_.get(id);
    if (endpoint) {
      endpoint.onError();
    }
  }
};

/**
 * Something which can receive notifications from an Endpoint; generally this is
 * the Endpoint's owner.
 * @interface
 */
mojo.internal.interfaceSupport.EndpointClient = class {
  /**
   * @param {!mojo.internal.interfaceSupport.Endpoint} endpoint
   * @param {!mojo.internal.MessageHeader} header
   * @param {!ArrayBuffer} buffer
   * @param {!Array<MojoHandle>} handles
   */
  onMessageReceived(endpoint, header, buffer, handles) {}

  /**
   * @param {!mojo.internal.interfaceSupport.Endpoint} endpoint
   * @param {string=} reason
   */
  onError(endpoint, reason = undefined) {}
};

/**
 * Encapsulates a single interface endpoint on a multiplexed Router object. This
 * may be the primary (possibly only) endpoint on a pipe, or a secondary
 * associated interface endpoint.
 */
mojo.internal.interfaceSupport.Endpoint = class {
  /**
   * @param {mojo.internal.interfaceSupport.Router=} router
   * @param {number=} interfaceId
   */
  constructor(router = null, interfaceId = 0) {
    /** @private {mojo.internal.interfaceSupport.Router} */
    this.router_ = router;

    /** @private {number} */
    this.interfaceId_ = interfaceId;

    /** @private {mojo.internal.interfaceSupport.ControlMessageHandler} */
    this.controlMessageHandler_ =
        new mojo.internal.interfaceSupport.ControlMessageHandler(this);

    /** @private {mojo.internal.interfaceSupport.EndpointClient} */
    this.client_ = null;

    /** @private {number} */
    this.nextRequestId_ = 0;

    /** @private {mojo.internal.interfaceSupport.Endpoint} */
    this.localPeer_ = null;
  }

  /**
   * @return {{
   *   endpoint0: !mojo.internal.interfaceSupport.Endpoint,
   *   endpoint1: !mojo.internal.interfaceSupport.Endpoint,
   * }}
   */
  static createAssociatedPair() {
    const endpoint0 = new mojo.internal.interfaceSupport.Endpoint();
    const endpoint1 = new mojo.internal.interfaceSupport.Endpoint();
    endpoint1.localPeer_ = endpoint0;
    endpoint0.localPeer_ = endpoint1;
    return {endpoint0, endpoint1};
  }

  /** @return {mojo.internal.interfaceSupport.Router} */
  get router() {
    return this.router_;
  }

  /** @return {boolean} */
  isPrimary() {
    return this.router_ !== null && this.interfaceId_ === 0;
  }

  /** @return {!MojoHandle} */
  releasePipe() {
    console.assert(this.isPrimary(), 'secondary endpoint cannot release pipe');
    return this.router_.pipe;
  }

  /** @return {boolean} */
  get isPendingAssociation() {
    return this.localPeer_ !== null;
  }

  /**
   * @param {string} interfaceName
   * @param {string} scope
   */
  bindInBrowser(interfaceName, scope) {
    console.assert(
        this.isPrimary() && !this.router_.isReading(),
        'endpoint is either associated or already bound');
    Mojo.bindInterface(interfaceName, this.router_.pipe, scope);
  }

  /**
   * @param {!mojo.internal.interfaceSupport.Endpoint} endpoint
   * @return {number}
   */
  associatePeerOfOutgoingEndpoint(endpoint) {
    console.assert(this.router_, 'cannot associate with unbound endpoint');
    const peer = endpoint.localPeer_;
    endpoint.localPeer_ = peer.localPeer_ = null;

    const id = this.router_.generateInterfaceId();
    peer.router_ = this.router_;
    peer.interfaceId_ = id;
    if (peer.client_) {
      this.router_.addEndpoint(peer, id);
    }
    return id;
  }

  /** @return {number} */
  generateRequestId() {
    const id = this.nextRequestId_++;
    if (this.nextRequestId_ > 0xffffffff) {
      this.nextRequestId_ = 0;
    }
    return id;
  }

  /**
   * @param {number} ordinal
   * @param {number} requestId
   * @param {number} flags
   * @param {!mojo.internal.MojomType} paramStruct
   * @param {!Object} value
   */
  send(ordinal, requestId, flags, paramStruct, value) {
    const message = new mojo.internal.Message(
        this, this.interfaceId_, flags, ordinal, requestId,
        /** @type {!mojo.internal.StructSpec} */ (paramStruct.$.structSpec),
        value);
    console.assert(
        this.router_, 'cannot send message on unassociated unbound endpoint');
    this.router_.send(message);
  }

  /** @param {mojo.internal.interfaceSupport.EndpointClient} client */
  start(client) {
    console.assert(!this.client_, 'endpoint already started');
    this.client_ = client;
    if (this.router_) {
      this.router_.addEndpoint(this, this.interfaceId_);
    }
  }

  /** @return {boolean} */
  get isStarted() {
    return this.client_ !== null;
  }

  stop() {
    if (this.router_) {
      this.router_.removeEndpoint(this.interfaceId_);
    }
    this.client_ = null;
    this.controlMessageHandler_ = null;
  }

  close() {
    if (this.router_) {
      this.router.closeEndpoint(this.interfaceId_);
    }
    this.client_ = null;
    this.controlMessageHandler_ = null;
  }

  async flushForTesting() {
    return this.controlMessageHandler_.sendRunMessage({'flushForTesting': {}});
  }

  /**
   * @param {!mojo.internal.MessageHeader} header
   * @param {!ArrayBuffer} buffer
   * @param {!Array<MojoHandle>} handles
   */
  onMessageReceived(header, buffer, handles) {
    console.assert(this.client_, 'endpoint has no client');
    const handled =
        this.controlMessageHandler_.maybeHandleControlMessage(header, buffer);
    if (handled) {
      return;
    }

    this.client_.onMessageReceived(this, header, buffer, handles);
  }

  onError() {
    if (this.client_) {
      this.client_.onError(this);
    }
  }
};

/**
 * Creates a new Endpoint wrapping a given pipe handle.
 *
 * @param {!MojoHandle|!mojo.internal.interfaceSupport.Endpoint} pipeOrEndpoint
 * @param {boolean=} setNamespaceBit
 * @return {!mojo.internal.interfaceSupport.Endpoint}
 */
mojo.internal.interfaceSupport.createEndpoint = function(
    pipeOrEndpoint, setNamespaceBit = false) {
  // `watch` is defined on MojoHandle but not Endpoint, so if it is not defined
  // we know this is an Endpoint.
  if (pipeOrEndpoint.watch === undefined) {
    return /** @type {!mojo.internal.interfaceSupport.Endpoint} */(
        pipeOrEndpoint);
  }
  return new mojo.internal.interfaceSupport.Endpoint(
      new mojo.internal.interfaceSupport.Router(
          /** @type {!MojoHandle} */(pipeOrEndpoint), setNamespaceBit),
      0);
};

/**
 * Returns its input if given an existing Endpoint. If given a pipe handle,
 * creates a new Endpoint to own it and returns that. This is a helper for
 * generated PendingReceiver constructors since they can accept either type as
 * input.
 *
 * @param {!MojoHandle|!mojo.internal.interfaceSupport.Endpoint} handle
 * @return {!mojo.internal.interfaceSupport.Endpoint}
 * @export
 */
mojo.internal.interfaceSupport.getEndpointForReceiver = function(handle) {
  return mojo.internal.interfaceSupport.createEndpoint(handle);
};

/**
 * @param {!mojo.internal.interfaceSupport.Endpoint} endpoint
 * @param {string} interfaceName
 * @param {string} scope
 * @export
 */
mojo.internal.interfaceSupport.bind = function(endpoint, interfaceName, scope) {
  endpoint.bindInBrowser(interfaceName, scope);
};

mojo.internal.interfaceSupport.PipeControlMessageHandler = class {
  /**
   * @param {!mojo.internal.interfaceSupport.Router} router
   * @param {function(number)} onDisconnect
   */
  constructor(router, onDisconnect) {
    /** @const {!mojo.internal.interfaceSupport.Router} */
    this.router_ = router;

    /** @const {function(number)} */
    this.onDisconnect_ = onDisconnect;
  }

  /**
   * @param {!mojo.pipeControl.RunOrClosePipeInput} input
   */
  send(input) {
    const message = new mojo.internal.Message(
        null, 0xffffffff, 0, mojo.pipeControl.RUN_OR_CLOSE_PIPE_MESSAGE_ID, 0,
        /** @type {!mojo.internal.StructSpec} */
        (mojo.pipeControl.RunOrClosePipeMessageParamsSpec.$.$.structSpec),
        {'input': input});
    this.router_.send(message);
  }

  /**
   * @param {!mojo.internal.MessageHeader} header
   * @param {!ArrayBuffer} buffer
   * @return {boolean}
   */
  maybeHandleMessage(header, buffer) {
    if (header.ordinal !== mojo.pipeControl.RUN_OR_CLOSE_PIPE_MESSAGE_ID) {
      return false;
    }

    const data = new DataView(buffer, header.headerSize);
    const decoder = new mojo.internal.Decoder(data, []);
    const spec = /** @type {!mojo.internal.StructSpec} */ (
        mojo.pipeControl.RunOrClosePipeMessageParamsSpec.$.$.structSpec);
    const input = decoder.decodeStructInline(spec)['input'];
    if (input.hasOwnProperty('peerAssociatedEndpointClosedEvent')) {
      this.onDisconnect_(input['peerAssociatedEndpointClosedEvent']['id']);
      return true;
    }

    return true;
  }

  /**@param {number} interfaceId */
  notifyEndpointClosed(interfaceId) {
    this.send({'peerAssociatedEndpointClosedEvent': {'id': interfaceId}});
  }
};

/**
 * Handles incoming interface control messages on an interface endpoint.
 */
mojo.internal.interfaceSupport.ControlMessageHandler = class {
  /** @param {!mojo.internal.interfaceSupport.Endpoint} endpoint */
  constructor(endpoint) {
    /** @private {!mojo.internal.interfaceSupport.Endpoint} */
    this.endpoint_ = endpoint;

    /** @private {!Map<number, function()>} */
    this.pendingFlushResolvers_ = new Map;
  }

  sendRunMessage(input) {
    const requestId = this.endpoint_.generateRequestId();
    return new Promise(resolve => {
      this.endpoint_.send(
          mojo.interfaceControl.RUN_MESSAGE_ID, requestId,
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
      this.endpoint_.send(
          mojo.interfaceControl.RUN_MESSAGE_ID, requestId,
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
 *
 * @typedef {{
 *   requestId: number,
 *   ordinal: number,
 *   responseStruct: !mojo.internal.MojomType,
 *   resolve: !Function,
 *   reject: !Function,
 * }}
 */
mojo.internal.interfaceSupport.PendingResponse;

/**
 * Exposed by endpoints to allow observation of remote peer closure. Any number
 * of listeners may be registered on a ConnectionErrorEventRouter, and the
 * router will dispatch at most one event in its lifetime, whenever its endpoint
 * detects peer closure.
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
   * @return {!mojo.internal.interfaceSupport.Endpoint}
   * @export
   */
  get handle() {}
};

/**
 * Generic helper used to implement all generated remote classes. Knows how to
 * serialize requests and deserialize their replies, both according to
 * declarative message structure specs.
 *
 * TODO(crbug.com/40102194): Use a bounded generic type instead of
 * mojo.internal.interfaceSupport.PendingReceiver.
 * @implements {mojo.internal.interfaceSupport.EndpointClient}
 * @export
 */
mojo.internal.interfaceSupport.InterfaceRemoteBase = class {
  /**
   * @param {!function(new:mojo.internal.interfaceSupport.PendingReceiver,
   *     !mojo.internal.interfaceSupport.Endpoint)} requestType
   * @param {MojoHandle|mojo.internal.interfaceSupport.Endpoint=} handle
   *     The pipe or endpoint handle to use as a remote endpoint. If omitted,
   *     this object must be bound with bindHandle before it can be used to send
   *     messages.
   * @public
   */
  constructor(requestType, handle = undefined) {
    /** @private {mojo.internal.interfaceSupport.Endpoint} */
    this.endpoint_ = null;

    /**
     * @private {!function(new:mojo.internal.interfaceSupport.PendingReceiver,
     *     !mojo.internal.interfaceSupport.Endpoint)}
     */
    this.requestType_ = requestType;

    /**
     * @private {!Map<number, !mojo.internal.interfaceSupport.PendingResponse>}
     */
    this.pendingResponses_ = new Map;

    /** @const {!mojo.internal.interfaceSupport.ConnectionErrorEventRouter} */
    this.connectionErrorEventRouter_ =
        new mojo.internal.interfaceSupport.ConnectionErrorEventRouter;

    if (handle) {
      this.bindHandle(handle);
    }
  }

  /** @return {mojo.internal.interfaceSupport.Endpoint} */
  get endpoint() {
    return this.endpoint_;
  }

  /**
   * @return {!mojo.internal.interfaceSupport.PendingReceiver}
   */
  bindNewPipeAndPassReceiver() {
    let {handle0, handle1} = Mojo.createMessagePipe();
    this.bindHandle(handle0);
    return new this.requestType_(
        mojo.internal.interfaceSupport.createEndpoint(handle1));
  }

  /**
   * @param {!MojoHandle|!mojo.internal.interfaceSupport.Endpoint} handle
   * @export
   */
  bindHandle(handle) {
    console.assert(!this.endpoint_, 'already bound');
    handle = mojo.internal.interfaceSupport.createEndpoint(
        handle, /* setNamespaceBit */ true);
    this.endpoint_ = handle;
    this.endpoint_.start(this);
    this.pendingResponses_ = new Map;
  }

  /** @export */
  associateAndPassReceiver() {
    console.assert(!this.endpoint_, 'cannot associate when already bound');
    const {endpoint0, endpoint1} =
        mojo.internal.interfaceSupport.Endpoint.createAssociatedPair();
    this.bindHandle(endpoint0);
    return new this.requestType_(endpoint1);
  }

  /**
   * @return {?mojo.internal.interfaceSupport.Endpoint}
   * @export
   */
  unbind() {
    if (!this.endpoint_) {
      return null;
    }
    const endpoint = this.endpoint_;
    this.endpoint_ = null;
    endpoint.stop();
    return endpoint;
  }

  /** @export */
  close() {
    this.cleanupAndFlushPendingResponses_('Message pipe closed.');
    if (this.endpoint_) {
      this.endpoint_.close();
    }
    this.endpoint_ = null;
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
   * @param {?mojo.internal.MojomType} maybeResponseStruct
   * @param {!Array} args
   * @return {!Promise}
   * @export
   */
  sendMessage(ordinal, paramStruct, maybeResponseStruct, args) {
    // The pipe has already been closed, so just drop the message.
    if (maybeResponseStruct && (!this.endpoint_ || !this.endpoint_.isStarted)) {
      return Promise.reject(new Error('The pipe has already been closed.'));
    }

    // Turns a functions args into an object where each property corresponds to
    // an argument.
    //
    // Each argument in `args` has a single corresponding field in `fields`
    // except for optional numerics which map to two fields in `fields`. This
    // means args' indexes don't exactly match `fields`'s. As we iterate
    // over the fields we keep track of how many optional numeric args we've
    // seen to get the right `args` index.
    const value = {};
    let nullableValueKindFields = 0;
    paramStruct.$.structSpec.fields.forEach((field, index) => {
      const fieldArgsIndex = index - nullableValueKindFields;
      if (!mojo.internal.isNullableValueKindField(field)) {
        value[field.name] = args[fieldArgsIndex];
        return;
      }

      const props = field.nullableValueKindProperties;
      if (props.isPrimary) {
        nullableValueKindFields++;
        value[props.originalFieldName] = args[fieldArgsIndex];
      }
    });

    const requestId = this.endpoint_.generateRequestId();
    this.endpoint_.send(
      ordinal, requestId,
      maybeResponseStruct ? mojo.internal.kMessageFlagExpectsResponse : 0,
      paramStruct, value);
    if (!maybeResponseStruct) {
      return Promise.resolve();
    }

    const responseStruct =
        /** @type {!mojo.internal.MojomType} */ (maybeResponseStruct);
    return new Promise((resolve, reject) => {
      this.pendingResponses_.set(
          requestId, {requestId, ordinal, responseStruct, resolve, reject});
    });
  }

  /**
   * @return {!Promise}
   * @export
   */
  flushForTesting() {
    return this.endpoint_.flushForTesting();
  }

  /** @override */
  onMessageReceived(endpoint, header, buffer, handles) {
    if (!(header.flags & mojo.internal.kMessageFlagIsResponse) ||
        header.flags & mojo.internal.kMessageFlagExpectsResponse) {
      return this.onError(endpoint, 'Received unexpected request message');
    }
    const pendingResponse = this.pendingResponses_.get(header.requestId);
    this.pendingResponses_.delete(header.requestId);
    if (!pendingResponse)
      return this.onError(endpoint, 'Received unexpected response message');
    const decoder = new mojo.internal.Decoder(
        new DataView(buffer, header.headerSize), handles, {endpoint});
    const responseValue = decoder.decodeStructInline(
        /** @type {!mojo.internal.StructSpec} */ (
            pendingResponse.responseStruct.$.structSpec));
    if (!responseValue)
      return this.onError(endpoint, 'Received malformed response message');
    if (header.ordinal !== pendingResponse.ordinal)
      return this.onError(endpoint, 'Received malformed response message');

    pendingResponse.resolve(responseValue);
  }

  /** @override */
  onError(endpoint, reason = undefined) {
    this.cleanupAndFlushPendingResponses_(reason);
    this.connectionErrorEventRouter_.dispatchErrorEvent();
  }

  /**
   * @param {string=} reason
   * @private
   */
  cleanupAndFlushPendingResponses_(reason = undefined) {
    if (this.endpoint_) {
      this.endpoint_.stop();
    }
    for (const id of this.pendingResponses_.keys()) {
      this.pendingResponses_.get(id).reject(new Error(reason));
    }
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

  /**
   * @return {!T}
   * @export
   */
  associateAndPassReceiver() {
    return this.remote_.associateAndPassReceiver();
  }

  /**
   * @return {boolean}
   * @export
   */
  isBound() {
    return this.remote_.endpoint_ !== null;
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
 *
 * @typedef {{
 *   paramStruct: !mojo.internal.MojomType,
 *   responseStruct: ?mojo.internal.MojomType,
 *   handler: !Function,
 * }}
 */
mojo.internal.interfaceSupport.MessageHandler;

/**
 * Generic helper that listens for incoming request messages on one or more
 * endpoints of the same interface type, dispatching them to registered
 * handlers. Handlers are registered against a specific ordinal message number.
 *
 * @template T
 * @implements {mojo.internal.interfaceSupport.EndpointClient}
 * @export
 */
mojo.internal.interfaceSupport.InterfaceReceiverHelperInternal = class {
  /**
   * @param {!function(new:T,
   *     (!MojoHandle|!mojo.internal.interfaceSupport.Endpoint)=)} remoteType
   * @public
   */
  constructor(remoteType) {
    /** @private {!Set<!mojo.internal.interfaceSupport.Endpoint>} endpoints */
    this.endpoints_ = new Set();

    /**
     * @private {!function(new:T,
     *     (!MojoHandle|!mojo.internal.interfaceSupport.Endpoint)=)}
     */
    this.remoteType_ = remoteType;

    /**
     * @private {!Map<number, !mojo.internal.interfaceSupport.MessageHandler>}
     */
    this.messageHandlers_ = new Map;

    /** @const {!mojo.internal.interfaceSupport.ConnectionErrorEventRouter} */
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
    this.messageHandlers_.set(ordinal, {paramStruct, responseStruct, handler});
  }

  /**
   * @param {!MojoHandle|!mojo.internal.interfaceSupport.Endpoint} handle
   * @export
   */
  bindHandle(handle) {
    handle = mojo.internal.interfaceSupport.createEndpoint(handle);
    this.endpoints_.add(handle);
    handle.start(this);
  }

  /**
   * @return {!T}
   * @export
   */
  bindNewPipeAndPassRemote() {
    let remote = new this.remoteType_();
    this.bindHandle(remote.$.bindNewPipeAndPassReceiver().handle);
    return remote;
  }

  /**
   * @return {!T}
   * @export
   */
  associateAndPassRemote() {
    const {endpoint0, endpoint1} =
        mojo.internal.interfaceSupport.Endpoint.createAssociatedPair();
    this.bindHandle(endpoint0);
    return new this.remoteType_(endpoint1);
  }

  /** @export */
  closeBindings() {
    for (const endpoint of this.endpoints_) {
      endpoint.close();
    }
    this.endpoints_.clear();
  }

  /**
   * @return {!mojo.internal.interfaceSupport.ConnectionErrorEventRouter}
   * @export
   */
  getConnectionErrorEventRouter() {
    return this.connectionErrorEventRouter_;
  }

  /**
   * @return {!Promise}
   * @export
   */
  async flush() {
    for (let endpoint of this.endpoints_) {
      await endpoint.flushForTesting();
    }
  }

  /** @override */
  onMessageReceived(endpoint, header, buffer, handles) {
    if (header.flags & mojo.internal.kMessageFlagIsResponse)
      throw new Error('Received unexpected response on interface receiver');
    const handler = this.messageHandlers_.get(header.ordinal);
    if (!handler)
      throw new Error('Received unknown message');
    const decoder = new mojo.internal.Decoder(
        new DataView(buffer, header.headerSize), handles, {endpoint});
    const request = decoder.decodeStructInline(
        /** @type {!mojo.internal.StructSpec} */ (
            handler.paramStruct.$.structSpec));
    if (!request)
      throw new Error('Received malformed message');

    // Each field in `handler.paramStruct.$.structSpec.fields` corresponds to
    // an argument, except for optional numerics where two fields correspond to
    // a single argument.
    const args = [];
    for (const field of handler.paramStruct.$.structSpec.fields) {
      if (!mojo.internal.isNullableValueKindField(field)) {
        args.push(request[field.name]);
        continue;
      }

      const props = field.nullableValueKindProperties;
      if (!props.isPrimary) {
        continue;
      }
      args.push(request[props.originalFieldName]);
    }

    let result = handler.handler.apply(null, args);

    // If the message expects a response, the handler must return either a
    // well-formed response object, or a Promise that will eventually yield one.
    if (handler.responseStruct) {
      if (result === undefined) {
        this.onError(endpoint);
        throw new Error(
            'Message expects a reply but its handler did not provide one.');
      }

      if (typeof result != 'object' || result.constructor.name != 'Promise') {
        result = Promise.resolve(result);
      }

      result
          .then(value => {
            endpoint.send(
                header.ordinal, header.requestId,
                mojo.internal.kMessageFlagIsResponse,
                /** @type {!mojo.internal.MojomType} */
                (handler.responseStruct), value);
          })
          .catch(() => {
            // If the handler rejects, that means it didn't like the request's
            // contents for whatever reason. We close the binding to prevent
            // further messages from being received from that client.
            this.onError(endpoint);
          });
    }
  }

  /** @override */
  onError(endpoint, reason = undefined) {
    this.endpoints_.delete(endpoint);
    endpoint.close();
    this.connectionErrorEventRouter_.dispatchErrorEvent();
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
   * @param {!MojoHandle|!mojo.internal.interfaceSupport.Endpoint} handle
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

  /**
   * @return {!T}
   * @export
   */
  associateAndPassRemote() {
    return this.helper_internal_.associateAndPassRemote();
  }

  /** @export */
  close() {
    this.helper_internal_.closeBindings();
  }

  /**
   * @return {!Promise}
   * @export
   */
  flush() {
    return this.helper_internal_.flush();
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
    /** @private {!MojoHandle} */
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
    if (!this.watcher_) {
      return;
    }
    this.watcher_.cancel();
    this.watcher_ = null;
  }

  stopAndCloseHandle() {
    if (this.watcher_) {
      this.stop();
    }
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
