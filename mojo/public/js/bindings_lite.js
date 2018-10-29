// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/** @export */
var mojo = {};

/** @export */
mojo.internal = {};

/** @export */
mojo.config = {};

/** @export */
mojo.config.globalScope = window;

/** @export */
mojo.config.autoLoadMojomDeps = true;

// Whether to automatically load mojom dependencies.
// For example, if foo.mojom imports bar.mojom, |mojoAutoLoadMojomDeps| set to
// true means that loading foo.mojom.js will insert a <script> tag to load
// bar.mojom.js, if it hasn't been loaded.
//
// The URL of bar.mojom.js is determined by the relative path of bar.mojom
// (relative to the position of foo.mojom at build time) and the URL of
// foo.mojom.js.
if ('mojoAutoLoadMojomDeps' in window)
  mojo.config.autoLoadMojomDeps = window['mojoAutoLoadMojomDeps'];

/** @enum {number} */
mojo.internal.LoadState = {
  PENDING_LOAD: 1,
  LOADED: 2
};

/** @const {!Map<string, mojo.internal.LoadState>} */
mojo.internal.mojomRegistry = new Map;

/**
 * @param {string} namespace
 * @export
 */
mojo.internal.exposeNamespace = function(namespace) {
  let current = mojo.config.globalScope;
  const parts = namespace.split('.');

  for (let part; parts.length && (part = parts.shift());) {
    if (!current[part])
      current[part] = {};
    current = current[part];
  }

  return current;
};

/**
 * @param {string} id
 * @return {boolean}
 * @export
 */
mojo.internal.isMojomPendingLoad = function(id) {
  return mojo.internal.mojomRegistry.get(id) ===
      mojo.internal.LoadState.PENDING_LOAD;
};

/**
 * @param {string} id
 * @return {boolean}
 * @export
 */
mojo.internal.isMojomLoaded = function(id) {
  return mojo.internal.mojomRegistry.get(id) === mojo.internal.LoadState.LOADED;
};

/**
 * @param {string} id
 * @export
 */
mojo.internal.markMojomPendingLoad = function(id) {
  if (mojo.internal.isMojomLoaded(id)) {
    throw new Error('The following mojom file has been loaded: ' + id);
  }

  mojo.internal.mojomRegistry.set(id, mojo.internal.LoadState.PENDING_LOAD);
};

/**
 * @param {string} id
 * @export
 */
mojo.internal.markMojomLoaded = function(id) {
  mojo.internal.mojomRegistry.set(id, mojo.internal.LoadState.LOADED);
};

/**
 * @param {string} id
 * @param {string} relativePath
 * @export
 */
mojo.internal.loadMojomIfNecessary = function(id, relativePath) {
  if (mojo.internal.mojomRegistry.has(id))
    return;

  if (!('document' in mojo.config.globalScope)) {
    throw new Error(
        'Mojom dependency autoloading is not implemented in workers. ' +
        'Please see config variable mojo.config.autoLoadMojomDeps for more ' +
        'details.');
  }

  mojo.internal.markMojomPendingLoad(id);
  const url = new URL(relativePath, document.currentScript.src).href;
  mojo.config.globalScope.document.write(
      `<script type="text/javascript" src="${url}"></script>`);
};

/**
 * Captures metadata about a request which was sent by a local proxy, for which
 * a response is expected.
 */
mojo.internal.PendingResponse = class {
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
 * Generic helper used to implement all generated proxy classes. Knows how to
 * serialize requests and deserialize their replies, both according to
 * declarative message structure specs.
 * @export
 */
mojo.internal.InterfaceProxyBase = class {
  /**
   * @param {MojoHandle=} opt_handle The message pipe handle to use as a proxy
   *     endpoint. If null, this object must be bound with bindHandle before
   *     it can be used to send any messages.
   * @public
   */
  constructor(opt_handle) {
    /** @public {?MojoHandle} */
    this.handle = null;

    /** @private {?mojo.internal.HandleReader} */
    this.reader_ = null;

    /** @private {number} */
    this.nextRequestId_ = 0;

    /** @private {!Map<number, !mojo.internal.PendingResponse>} */
    this.pendingResponses_ = new Map;

    if (opt_handle instanceof MojoHandle)
      this.bindHandle(opt_handle);
  }

  /**
   * @param {!MojoHandle} handle
   * @export
   */
  bindHandle(handle) {
    if (this.handle)
      throw new Error('Proxy already bound.');
    this.handle = handle;

    const reader = new mojo.internal.HandleReader(handle);
    reader.onRead = this.onMessageReceived_.bind(this);
    reader.onError = this.onError_.bind(this);
    reader.start();

    this.reader_ = reader;
    this.nextRequestId_ = 0;
    this.pendingResponses_ = new Map;
  }

  /** @export */
  unbind() {
    if (this.reader_)
      this.reader_.stop();
  }

  /**
   * @param {number} ordinal
   * @param {!mojo.internal.Type} paramStruct
   * @param {!mojo.internal.Type} responseStruct
   * @param {!Array} args
   * @return {!Promise}
   * @export
   */
  sendMessage(ordinal, paramStruct, responseStruct, args) {
    if (!this.handle) {
      throw new Error(
          'Attempting to use an unbound proxy. Try createRequest() first.')
    }

    // The pipe has already been closed, so just drop the message.
    if (this.reader_.isStopped())
      return Promise.reject();

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
          new mojo.internal.PendingResponse(
              requestId, ordinal, responseStruct, resolve, reject));
    });
  }

  /**
   * @param {!ArrayBuffer} buffer
   * @param {!Array<MojoHandle>} handles
   * @private
   */
  onMessageReceived_(buffer, handles) {
    const data = new DataView(buffer);
    const header = mojo.internal.deserializeMessageHeader(data);
    if (!(header.flags & mojo.internal.kMessageFlagIsResponse) ||
        header.flags & mojo.internal.kMessageFlagExpectsResponse) {
      return this.onError_('Received unexpected request message');
    }
    const pendingResponse = this.pendingResponses_.get(header.requestId);
    this.pendingResponses_.delete(header.requestId);
    if (!pendingResponse)
      return this.onError_('Received unexpected response message');
    const decoder = new mojo.internal.Decoder(data, handles, header.headerSize);
    const responseValue =
        decoder.decodeStructInline(pendingResponse.responseStruct.$.structSpec);
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
    this.reader_.stopAndCloseHandle();
    this.reader_ = null;
    for (const id of this.pendingResponses_.keys())
      this.pendingResponses_.get(id).reject(new Error(opt_reason));
    this.pendingResponses_ = new Map;
  }
};

/**
 * Helper used by generated EventRouter types to dispatch incoming interface
 * messages as Event-like things.
 * @export
 */
mojo.internal.InterfaceCallbackTarget = class {
  /** @public */
  constructor() {
    /** @private {!Map<number, !Function>} */
    this.listeners_ = new Map;

    /** @private {number} */
    this.nextListenerId_ = 0;
  }

  /**
   * @param {!Function} listener
   * @return {number} A unique ID for the added listener.
   * @export
   */
  addListener(listener) {
    const id = ++this.nextListenerId_;
    this.listeners_.set(id, listener);
    return id;
  }

  /**
   * @param {number} id An ID returned by a prior call to addListener.
   * @return {boolean} True iff the identified listener was found and removed.
   * @export
   */
  removeListener(id) {
    return this.listeners_.delete(id);
  }

  /**
   * @param {boolean} expectsResponse
   * @return {!Function}
   * @export
   */
  createTargetHandler(expectsResponse) {
    if (expectsResponse)
      return this.dispatchWithResponse_.bind(this);
    return this.dispatch_.bind(this);
  }

  /**
   * @param {...*}
   * @private
   */
  dispatch_() {
    const args = Array.from(arguments);
    this.listeners_.forEach(listener => listener.apply(null, args));
  }

  /**
   * @param {...*}
   * @return {?Object}
   * @private
   */
  dispatchWithResponse_() {
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
 * Wraps message handlers attached to an InterfaceTarget.
 */
mojo.internal.MessageHandler = class {
  /**
   * @param {!mojo.internal.MojomType} paramStruct
   * @param {!mojo.internal.MojomType} responseStruct
   * @param {!Function} handler
   * @private
   */
  constructor(paramStruct, responseStruct, handler) {
    /** @public {!mojo.internal.MojomType} */
    this.paramStruct = paramStruct;

    /** @public {!mojo.internal.MojomType} */
    this.responseStruct = responseStruct;

    /** @public {!Function} */
    this.handler = handler;
  }
};

/**
 * Listens for incoming request messages on a message pipe, dispatching them to
 * any registered handlers. Handlers are registered against a specific ordinal
 * message number.
 * @export
 */
mojo.internal.InterfaceTarget = class {
  /** @public */
  constructor() {
    /** @private {!Map<MojoHandle, !mojo.internal.HandleReader>} */
    this.readers_ = new Map;

    /** @private {!Map<number, !mojo.internal.MessageHandler>} */
    this.messageHandlers_ = new Map;
  }

  /**
   * @param {number} ordinal
   * @param {!mojo.internal.MojomType} paramStruct
   * @param {!mojo.internal.MojomType} responseStruct
   * @param {!Function} handler
   * @export
   */
  registerHandler(ordinal, paramStruct, responseStruct, handler) {
    this.messageHandlers_.set(
        ordinal,
        new mojo.internal.MessageHandler(paramStruct, responseStruct, handler));
  }

  /**
   * @param {!MojoHandle} handle
   * @export
   */
  bindHandle(handle) {
    const reader = new mojo.internal.HandleReader(handle);
    this.readers_.set(handle, reader);
    reader.onRead = this.onMessageReceived_.bind(this, handle);
    reader.onError = this.onError_.bind(this, handle);
    reader.start();
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
    if (header.flags & mojo.internal.kMessageFlagIsResponse)
      throw new Error('Received unexpected response on interface target');
    const handler = this.messageHandlers_.get(header.ordinal);
    if (!handler)
      throw new Error('Received unknown message');
    const decoder = new mojo.internal.Decoder(data, handles, header.headerSize);
    const request =
        decoder.decodeStructInline(handler.paramStruct.$.structSpec);
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
                mojo.internal.kMessageFlagIsResponse, handler.responseStruct,
                value);
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
    reader.stopAndCloseHandle();
    this.readers_.delete(handle);
  }
};

/**
 * Watches a MojoHandle for readability or peer closure, forwarding either event
 * to one of two callbacks on the reader. Used by both InterfaceProxyBase and
 * InterfaceTarget to watch for incoming messages.
 */
mojo.internal.HandleReader = class {
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

/** @const {number} */
mojo.internal.kArrayHeaderSize = 8;

/** @const {number} */
mojo.internal.kStructHeaderSize = 8;

/** @const {number} */
mojo.internal.kMessageV0HeaderSize = 24;

/** @const {number} */
mojo.internal.kMessageV1HeaderSize = 32;

/** @const {number} */
mojo.internal.kMapDataSize = 24;

/** @const {number} */
mojo.internal.kEncodedInvalidHandleValue = 0xffffffff;

/** @const {number} */
mojo.internal.kMessageFlagExpectsResponse = 1 << 0;

/** @const {number} */
mojo.internal.kMessageFlagIsResponse = 1 << 1;

/** @const {boolean} */
mojo.internal.kHostLittleEndian = (function() {
  const wordBytes = new Uint8Array(new Uint16Array([1]).buffer);
  return !!wordBytes[0];
})();

/**
 * @param {*} x
 * @return {boolean}
 */
mojo.internal.isNullOrUndefined = function(x) {
  return x === null || x === undefined;
};

/**
 * @param {number} size
 * @param {number} alignment
 * @return {number}
 */
mojo.internal.align = function(size, alignment) {
  return size + (alignment - (size % alignment)) % alignment;
};

/**
 * @param {!DataView} dataView
 * @param {number} byteOffset
 * @param {number} value
 */
mojo.internal.setInt64 = function(dataView, byteOffset, value) {
  if (mojo.internal.kHostLittleEndian) {
    dataView.setInt32(
        byteOffset, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
    dataView.setInt32(
        byteOffset + 4, Number(BigInt(value) >> BigInt(32)),
        mojo.internal.kHostLittleEndian);
  } else {
    dataView.setInt32(
        byteOffset, Number(BigInt(value) >> BigInt(32)),
        mojo.internal.kHostLittleEndian);
    dataView.setInt32(
        byteOffset + 4, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
  }
};

/**
 * @param {!DataView} dataView
 * @param {number} byteOffset
 * @param {number} value
 */
mojo.internal.setUint64 = function(dataView, byteOffset, value) {
  if (mojo.internal.kHostLittleEndian) {
    dataView.setUint32(
        byteOffset, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
    dataView.setUint32(
        byteOffset + 4, Number(BigInt(value) >> BigInt(32)),
        mojo.internal.kHostLittleEndian);
  } else {
    dataView.setUint32(
        byteOffset, Number(BigInt(value) >> BigInt(32)),
        mojo.internal.kHostLittleEndian);
    dataView.setUint32(
        byteOffset + 4, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
  }
};

/**
 * @param {!DataView} dataView
 * @param {number} byteOffset
 * @return {number}
 */
mojo.internal.getInt64 = function(dataView, byteOffset) {
  let low, high;
  if (mojo.internal.kHostLittleEndian) {
    low = dataView.getInt32(byteOffset, mojo.internal.kHostLittleEndian);
    high = dataView.getInt32(byteOffset + 4, mojo.internal.kHostLittleEndian);
  } else {
    low = dataView.getInt32(byteOffset + 4, mojo.internal.kHostLittleEndian);
    high = dataView.getInt32(byteOffset, mojo.internal.kHostLittleEndian);
  }
  const value = (BigInt(high) << BigInt(32)) | BigInt(low);
  if (value <= BigInt(Number.MAX_SAFE_INTEGER) &&
      value >= BigInt(Number.MIN_SAFE_INTEGER)) {
    return Number(value);
  }
  return value;
};

/**
 * @param {!DataView} dataView
 * @param {number} byteOffset
 * @return {number}
 */
mojo.internal.getUint64 = function(dataView, byteOffset) {
  let low, high;
  if (mojo.internal.kHostLittleEndian) {
    low = dataView.getUint32(byteOffset, mojo.internal.kHostLittleEndian);
    high = dataView.getUint32(byteOffset + 4, mojo.internal.kHostLittleEndian);
  } else {
    low = dataView.getUint32(byteOffset + 4, mojo.internal.kHostLittleEndian);
    high = dataView.getUint32(byteOffset, mojo.internal.kHostLittleEndian);
  }
  const value = (BigInt(high) << BigInt(32)) | BigInt(low);
  if (value <= BigInt(Number.MAX_SAFE_INTEGER))
    return Number(value);
  return value;
};

/** Owns an outgoing message buffer and facilitates serialization. */
mojo.internal.Message = class {
  /**
   * @param {number} flags
   * @param {number} ordinal
   * @param {number=} opt_requestId
   * @private
   */
  constructor(flags, ordinal, opt_requestId) {
    let headerSize, version;
    if ((flags &
         (mojo.internal.kMessageFlagExpectsResponse |
          mojo.internal.kMessageFlagIsResponse)) == 0) {
      headerSize = mojo.internal.kMessageV0HeaderSize;
      version = 0;
    } else {
      headerSize = mojo.internal.kMessageV1HeaderSize;
      version = 1;
    }

    /** @public {!ArrayBuffer} */
    this.buffer;

    /** @public {!DataView} */
    this.data;

    /** @private {number} */
    this.bitOffset_;

    this.resize_(headerSize);

    const header = this.data;
    header.setUint32(0, headerSize, mojo.internal.kHostLittleEndian);
    header.setUint32(4, version, mojo.internal.kHostLittleEndian);
    header.setUint32(8, 0);  // Interface ID (only for associated interfaces)
    header.setUint32(12, ordinal, mojo.internal.kHostLittleEndian);
    header.setUint32(16, flags, mojo.internal.kHostLittleEndian);
    header.setUint32(20, 0);  // Padding
    if (version > 0) {
      mojo.internal.setUint64(
          this.data, 24,
          /** @type {number} */ (opt_requestId));
    }

    this.handles = [];

    /** @private {number} */
    this.cursor_ = headerSize;

    /** @private {!Array<{pointerOffset: number, execute: Function}>} */
    this.deferredEncodings_ = [];
  }

  /**
   * @param {number} newByteLength
   * @private
   */
  resize_(newByteLength) {
    if (this.buffer && newByteLength === this.buffer.byteLength)
      return;
    const newBuffer = new ArrayBuffer(newByteLength);
    if (this.buffer)
      new Uint8Array(newBuffer).set(new Uint8Array(this.buffer));
    this.buffer = newBuffer;
    this.data = new DataView(newBuffer);
    this.bitOffset_ = 0;
  }

  /**
   * @param {number} additionalByteLength
   * @private
   */
  grow_(additionalByteLength) {
    const offset = this.buffer.byteLength;
    this.resize_(offset + additionalByteLength);
    return offset;
  }

  /**
   * @param {number} alignment
   * @private
   */
  alignCursor_(alignment) {
    if (this.bitOffset_) {
      this.cursor_++;
      this.bitOffset_ = 0;
    }
    this.cursor_ = mojo.internal.align(this.cursor_, alignment);
  }

  /**
   * @param {number} alignment
   * @private
   */
  alignAndGrowToCursor_(alignment) {
    this.alignCursor_(alignment);
    if (this.cursor_ > this.buffer.byteLength)
      this.resize_(this.cursor_);
  }

  /**
   * @param {number} amount
   * @param {number=} opt_alignment
   * @private
   */
  advanceCursor_(amount, opt_alignment) {
    // As a general rule, a value of N bytes should be aligned to N bytes. This
    // is the default behavior.
    if (!opt_alignment)
      opt_alignment = amount;
    this.alignCursor_(opt_alignment);

    const offset = this.cursor_;
    this.cursor_ += amount;
    if (this.cursor_ > this.buffer.byteLength)
      this.resize_(this.cursor_);
    return offset;
  }

  /** @private */
  executeDeferredEncodings_() {
    let encoding;
    while (encoding = this.deferredEncodings_.shift()) {
      this.alignAndGrowToCursor_(8);
      const relativeOffset = this.cursor_ - encoding.pointerOffset;
      mojo.internal.setUint64(
          this.data, encoding.pointerOffset, relativeOffset);
      encoding.execute();
    }
  }

  appendNullOffset() {
    // New bytes are already zero-initialized. This is a fast-path since
    // encoding null pointers is much more common than encoding actual 64-bit
    // integer values.
    this.advanceCursor_(8);
  }

  appendBool(value) {
    if (this.cursor_ === this.buffer.byteLength)
      this.grow_(1);
    const oldValue = this.data.getUint8(this.cursor_);
    if (value)
      this.data.setUint8(this.cursor_, oldValue | (1 << this.bitOffset_));
    else
      this.data.setUint8(this.cursor_, oldValue & ~(1 << this.bitOffset_));
    this.bitOffset_ += 1;
    if (this.bitOffset_ == 8) {
      this.bitOffset_ = 0;
      this.cursor_++;
    }
  }

  appendInt8(value) {
    const offset = this.advanceCursor_(1);
    this.data.setInt8(offset, value);
  }

  appendUint8(value) {
    const offset = this.advanceCursor_(1);
    this.data.setUint8(offset, value);
  }

  appendInt16(value) {
    const offset = this.advanceCursor_(2);
    this.data.setInt16(offset, value, mojo.internal.kHostLittleEndian);
  }

  appendUint16(value) {
    const offset = this.advanceCursor_(1);
    this.data.setUint16(offset, value, mojo.internal.kHostLittleEndian);
  }

  appendInt32(value) {
    const offset = this.advanceCursor_(4);
    this.data.setInt32(offset, value, mojo.internal.kHostLittleEndian);
  }

  appendUint32(value) {
    const offset = this.advanceCursor_(4);
    this.data.setUint32(offset, value, mojo.internal.kHostLittleEndian);
  }

  appendInt64(value) {
    const offset = this.advanceCursor_(8);
    mojo.internal.setInt64(this.data, offset, value);
  }

  appendUint64(value) {
    const offset = this.advanceCursor_(8);
    mojo.internal.setUint64(this.data, offset, value);
  }

  appendFloat(value) {
    const offset = this.advanceCursor_(4);
    this.data.setFloat32(offset, value, mojo.internal.kHostLittleEndian);
  }

  appendDouble(value) {
    const offset = this.advanceCursor_(8);
    this.data.setFloat64(offset, value, mojo.internal.kHostLittleEndian);
  }

  appendHandle(value) {
    this.appendUint32(this.handles.length);
    this.handles.push(value);
  }

  appendString(value) {
    if (!mojo.internal.Message.textEncoder)
      mojo.internal.Message.textEncoder = new TextEncoder('utf-8');
    this.appendArray(
        {elementType: mojo['mojom']['Uint8']},
        mojo.internal.Message.textEncoder.encode(value));
  }

  deferEncoding(encoder) {
    this.deferredEncodings_.push({
      pointerOffset: this.advanceCursor_(8),
      execute: encoder,
    });
  }

  /**
   * @param {!mojo.internal.ArraySpec} arraySpec
   * @param {*} value
   */
  appendArray(arraySpec, value) {
    this.deferEncoding(this.appendArrayInline.bind(this, arraySpec, value));
  }

  /**
   * @param {!mojo.internal.ArraySpec} arraySpec
   * @param {*} value
   */
  appendArrayInline(arraySpec, value) {
    let size;
    if (arraySpec.elementType === mojo.mojom.Bool) {
      size = mojo.internal.kArrayHeaderSize + (value.length + 7) >> 3;
    } else {
      size = mojo.internal.kArrayHeaderSize +
          value.length * arraySpec.elementType.$.arrayElementSize;
    }

    const header = this.advanceCursor_(8);
    this.data.setUint32(header, size, mojo.internal.kHostLittleEndian);
    this.data.setUint32(
        header + 4, value.length, mojo.internal.kHostLittleEndian);

    if (arraySpec.elementType === mojo.mojom.Bool) {
      let bit = 0;
      let index = header + 8;
      let byte = 0;
      for (const e of value) {
        if (bit == 8) {
          this.data.setUint8(index, byte);
          bit = 0;
          byte = 0;
          index++;
        }
        if (e)
          byte += (1 << bit);
        bit++;
      }
      this.data.setUint8(index, byte);
      this.alignAndGrowToCursor_(8);
      return;
    }

    for (const e of value) {
      if (e === null) {
        if (!arraySpec.elementNullable) {
          throw new Error(
              'Trying to send a null element in an array of ' +
              'non-nullable elements');
        }
        arraySpec.elementType.$.encodeNull();
      }
      arraySpec.elementType.$.encode(e, this);
    }
    this.alignAndGrowToCursor_(8);
  }

  /**
   * @param {!mojo.internal.MapSpec} mapSpec
   * @param {*} value
   */
  appendMap(mapSpec, value) {
    this.deferEncoding(this.appendMapInline.bind(this, mapSpec, value));
  }

  /**
   * @param {!mojo.internal.MapSpec} mapSpec
   * @param {*} value
   */
  appendMapInline(mapSpec, value) {
    let keys, values;
    if (value instanceof Map) {
      keys = Array.from(value.keys());
      values = Array.from(value.values());
    } else {
      keys = Object.keys(value);
      values = keys.map(k => value[k]);
    }

    const header = this.advanceCursor_(mojo.internal.kStructHeaderSize, 8);
    this.data.setUint32(
        header, mojo.internal.kMapDataSize, mojo.internal.kHostLittleEndian);
    this.data.setUint32(header + 4, 0);
    this.appendArray({elementType: mapSpec.keyType}, keys);
    this.appendArray(
        {
          elementType: mapSpec.valueType,
          elementNullable: mapSpec.valueNullable
        },
        values);
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @param {!Object} value
   */
  appendStruct(structSpec, value) {
    this.deferEncoding(this.appendStructInline.bind(this, structSpec, value));
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @param {!Object} value
   */
  appendStructInline(structSpec, value) {
    const header = this.advanceCursor_(8);

    for (const field of structSpec.fields) {
      if (!value || !(value instanceof Object) ||
          mojo.internal.isNullOrUndefined(value[field.name])) {
        if (!field.nullable) {
          throw new Error(
              structSpec.name + ' missing value for non-nullable ' +
              'field "' + field.name + '"');
        }
        field.type.$.encodeNull(this);
        continue;
      }

      field.type.$.encode(value[field.name], this);
    }

    this.alignAndGrowToCursor_(8);
    this.data.setUint32(
        header, this.cursor_ - header, mojo.internal.kHostLittleEndian);
    this.data.setUint32(header + 4, 0);  // TODO: Support versioning.
  }
};

/** @type {TextEncoder} */
mojo.internal.Message.textEncoder = null;

/**
 * Helps decode incoming messages. Decoders may be created recursively to
 * decode partial message fragments indexed by indirect message offsets, as with
 * encoded arrays and nested structs.
 */
mojo.internal.Decoder = class {
  /**
   * @param {!DataView} data
   * @param {!Array<MojoHandle>} handles
   * @param {number} cursor
   * @private
   */
  constructor(data, handles, cursor) {
    /** @private {!DataView} */
    this.data_ = data;

    /** @private {!Array<MojoHandle>} */
    this.handles_ = handles;

    /** @private {number} */
    this.cursor_ = cursor;

    /** @private {number} */
    this.bitOffset_ = 0;

    /** @private {number} */
    this.lastBoolOffset_ = 0;
  }

  /**
   * @param {number} alignment
   * @private
   */
  alignCursor_(alignment) {
    if (this.bitOffset_ > 0) {
      this.cursor_++;
      this.bitOffset_ = 0;
    }
    this.cursor_ = mojo.internal.align(this.cursor_, alignment);
  }

  /**
   * @param {number} amount
   * @return {number}
   * @private
   */
  alignAndAdvanceCursor_(amount) {
    this.alignCursor_(amount);
    const cursor = this.cursor_;
    this.cursor_ += amount;
    return cursor;
  }

  decodeBool() {
    if (this.cursor_ != this.lastBoolOffset_)
      this.bitOffset_ = 0;
    const offset = this.cursor_;
    const bit = this.bitOffset_++;
    this.lastBoolOffset_ = offset;
    if (this.bitOffset_ == 8) {
      this.cursor_++;
      this.bitOffset_ = 0;
    }
    return !!(this.data_.getUint8(offset) & (1 << bit));
  }

  decodeInt8() {
    return this.data_.getInt8(this.cursor_++);
  }

  decodeUint8() {
    return this.data_.getUint8(this.cursor_++);
  }

  decodeInt16() {
    return this.data_.getInt16(
        this.alignAndAdvanceCursor_(2), mojo.internal.kHostLittleEndian);
  }

  decodeUint16() {
    return this.data_.getUint16(
        this.alignAndAdvanceCursor_(2), mojo.internal.kHostLittleEndian);
  }

  decodeInt32() {
    return this.data_.getInt32(
        this.alignAndAdvanceCursor_(4), mojo.internal.kHostLittleEndian);
  }

  decodeUint32() {
    return this.data_.getUint32(
        this.alignAndAdvanceCursor_(4), mojo.internal.kHostLittleEndian);
  }

  decodeInt64() {
    return mojo.internal.getInt64(this.data_, this.alignAndAdvanceCursor_(8));
  }

  decodeUint64() {
    return mojo.internal.getUint64(this.data_, this.alignAndAdvanceCursor_(8));
  }

  decodeFloat() {
    return this.data_.getFloat32(
        this.alignAndAdvanceCursor_(4), mojo.internal.kHostLittleEndian);
  }

  decodeDouble() {
    return this.data_.getFloat64(
        this.alignAndAdvanceCursor_(4), mojo.internal.kHostLittleEndian);
  }

  decodeHandle() {
    const index = this.data_.getUint32(
        this.alignAndAdvanceCursor_(4), mojo.internal.kHostLittleEndian);
    if (index == 0xffffffff)
      return null;
    if (index >= this.handles_.length)
      throw new Error('Decoded invalid handle index');
    return this.handles_[index];
  }

  decodeString() {
    if (!mojo.internal.Decoder.textDecoder)
      mojo.internal.Decoder.textDecoder = new TextDecoder('utf-8');
    return mojo.internal.Decoder.textDecoder.decode(
        new Uint8Array(this.decodeArray({
          elementType: mojo.mojom.Uint8,
        })).buffer);
  }

  decodeOffset() {
    this.alignCursor_(8);
    const cursor = this.cursor_;
    const offset = this.decodeUint64();
    if (offset == 0)
      return 0;
    return cursor + offset;
  }

  /**
   * @param {!mojo.internal.ArraySpec} arraySpec
   * @return {!Array}
   */
  decodeArray(arraySpec) {
    const arrayOffset = this.decodeOffset();
    if (!arrayOffset)
      return null;

    const elementDecoder =
        new mojo.internal.Decoder(this.data_, this.handles_, arrayOffset);
    return elementDecoder.decodeArrayInline(arraySpec)
  }

  /**
   * @param {!mojo.internal.ArraySpec} arraySpec
   * @return {!Array}
   */
  decodeArrayInline(arraySpec) {
    const size = this.decodeUint32();
    const numElements = this.decodeUint32();
    if (!numElements)
      return [];

    const result = [];
    if (arraySpec.elementType === mojo.mojom.Bool) {
      let bit = 8;
      let byteValue = this.decodeUint8();
      for (let i = 0; i < numElements; ++i) {
        if (bit == 8) {
          bit = 0;
          byteValue = this.decodeUint8();
        }
        result.push(!!(byteValue & (1 << bit)));
        ++bit;
      }
    } else {
      for (let i = 0; i < numElements; ++i) {
        const element = arraySpec.elementType.$.decode(this);
        if (element === null && !arraySpec.elementNullable)
          throw new Error('Received unexpected array element');
        result.push(element);
      }
    }
    return result;
  }

  /**
   * @param {!mojo.internal.MapSpec} mapSpec
   * @return {!Object|!Map}
   */
  decodeMap(mapSpec) {
    const mapOffset = this.decodeOffset();
    if (!mapOffset)
      return null;

    const mapStructSize =
        this.data_.getUint32(mapOffset, mojo.internal.kHostLittleEndian);
    const mapStructVersion =
        this.data_.getUint32(mapOffset + 4, mojo.internal.kHostLittleEndian);
    if (mapStructSize != mojo.internal.kMapDataSize || mapStructVersion != 0)
      throw new Error('Received invalid map data');

    const keysDecoder = new mojo.internal.Decoder(
        this.data_, this.handles_,
        mojo.internal.getUint64(this.data_, mapOffset + 8) + mapOffset + 8);
    const valuesDecoder = new mojo.internal.Decoder(
        this.data_, this.handles_,
        mojo.internal.getUint64(this.data_, mapOffset + 16) + mapOffset + 16);
    const keys = keysDecoder.decodeArray({elementType: mapSpec.keyType});
    const values = valuesDecoder.decodeArray({
      elementType: mapSpec.valueType,
      elementNullable: mapSpec.valueNullable
    });
    if (keys.length != values.length)
      throw new Error('Received invalid map data');
    if (!mapSpec.keyType.$.isValidObjectKeyType) {
      const map = new Map;
      for (let i = 0; i < keys.length; ++i)
        map.set(keys[i], values[i]);
      return map;
    }

    const map = {};
    for (let i = 0; i < keys.length; ++i)
      map[keys[i]] = values[i];
    return map;
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @return {Object}
   */
  decodeStruct(structSpec) {
    const structOffset = this.decodeOffset();
    if (!structOffset)
      return null;

    const decoder =
        new mojo.internal.Decoder(this.data_, this.handles_, structOffset);
    return decoder.decodeStructInline(structSpec);
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @return {!Object}
   */
  decodeStructInline(structSpec) {
    const size = this.decodeUint32();
    const version = this.decodeUint32();
    const result = {};
    for (const field of structSpec.fields) {
      const value = field.type.$.decode(this);
      if (value === null && !field.nullable) {
        throw new Error(
            'Received ' + structSpec.name + ' with invalid null field ' +
            '"' + field.name + '"')
      }
      result[field.name] = value;
    }
    return result;
  }

  decodeInterfaceProxy(type) {
    const handle = this.decodeHandle();
    const version = this.decodeUint32();  // TODO: support versioning
    if (!handle)
      return null;
    return new type(handle);
  }

  decodeInterfaceRequest(type) {
    const handle = this.decodeHandle();
    if (!handle)
      return null;
    return new type(handle);
  }
};

/** @type {TextDecoder} */
mojo.internal.Decoder.textDecoder = null;

/**
 * @param {!MojoHandle} handle
 * @param {number} ordinal
 * @param {number} requestId
 * @param {number} flags
 * @param {!mojo.internal.MojomType} paramStruct
 * @param {!Object} value
 */
mojo.internal.serializeAndSendMessage = function(
    handle, ordinal, requestId, flags, paramStruct, value) {
  const message = new mojo.internal.Message(flags, ordinal, requestId);
  message.appendStructInline(
      /* @type {!mojo.internal.StructSpec} */ (paramStruct.$.structSpec),
      value);
  message.executeDeferredEncodings_();
  handle.writeMessage(message.buffer, message.handles);
};

/**
 * @param {!DataView} data
 * @return {{
 *     headerSize: number,
 *     ordinal: number,
 *     flags: number,
 *     requestId: number,
 * }}
 */
mojo.internal.deserializeMessageHeader = function(data) {
  const headerSize = data.getUint32(0, mojo.internal.kHostLittleEndian);
  const headerVersion = data.getUint32(4, mojo.internal.kHostLittleEndian);
  if ((headerVersion == 0 &&
       headerSize != mojo.internal.kMessageV0HeaderSize) ||
      (headerVersion == 1 &&
       headerSize != mojo.internal.kMessageV1HeaderSize) ||
      headerVersion > 2) {
    throw new Error('Received invalid message header');
  }
  if (headerVersion == 2)
    throw new Error('v2 messages not yet supported');
  const header = {
    headerSize: headerSize,
    ordinal: data.getUint32(12, mojo.internal.kHostLittleEndian),
    flags: data.getUint32(16, mojo.internal.kHostLittleEndian),
  };
  if (headerVersion > 0)
    header.requestId = data.getUint32(24, mojo.internal.kHostLittleEndian);
  else
    header.requestId = 0;
  return header;
};

/**
 * @typedef {{
 *   encode: function(*, !mojo.internal.Message),
 *   decode: function(!mojo.internal.Decoder):*,
 *   isValidObjectKeyType: boolean,
 *   arrayElementSize: (number|undefined),
 *   arraySpec: (!mojo.internal.ArraySpec|undefined),
 *   mapSpec: (!mojo.internal.MapSpec|undefined),
 *   structSpec: (!mojo.internal.StructSpec|undefined),
 * }}
 */
mojo.internal.MojomTypeInfo;

/**
 * @typedef {{
 *   $: !mojo.internal.MojomTypeInfo
 * }}
 */
mojo.internal.MojomType;

/**
 * @typedef {{
 *   elementType: !mojo.internal.MojomType,
 *   elementNullable: (boolean|undefined)
 * }}
 */
mojo.internal.ArraySpec;

/**
 * @typedef {{
 *   keyType: !mojo.internal.MojomType,
 *   valueType: !mojo.internal.MojomType,
 *   valueNullable: boolean
 * }}
 */
mojo.internal.MapSpec;

/**
 * @typedef {{
 *   name: string,
 *   type: !mojo.internal.MojomType,
 *   defaultValue: *,
 *   nullable: boolean,
 * }}
 */
mojo.internal.StructFieldSpec;

/**
 * @typedef {{
 *   type: !mojo.internal.MojomType,
 *   fields: !Array<!mojo.internal.StructFieldSpec>,
 * }}
 */
mojo.internal.StructSpec;

/**
 * Mojom type specifications and corresponding encode/decode routines. These
 * are stored in struct and union specifications to describe how fields should
 * be serialized and deserialized.
 *
 * @const
 * @export
 */
mojo.mojom = {};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Bool = {
  $: {
    encode: function(value, message) {
      message.appendBool(value);
    },
    decode: decoder => decoder.decodeBool(),
    isValidObjectKeyType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Int8 = {
  $: {
    encode: function(value, message) {
      message.appendInt8(value);
    },
    decode: decoder => decoder.decodeInt8(),
    arrayElementSize: 1,
    isValidObjectKeyType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Uint8 = {
  $: {
    encode: function(value, message) {
      message.appendUint8(value);
    },
    decode: decoder => decoder.decodeUint8(),
    arrayElementSize: 1,
    isValidObjectKeyType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Int16 = {
  $: {
    encode: function(value, message) {
      message.appendInt16(value);
    },
    decode: decoder => decoder.decodeInt16(),
    arrayElementSize: 2,
    isValidObjectKeyType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Uint16 = {
  $: {
    encode: function(value, message) {
      message.appendUint16(value);
    },
    decode: decoder => decoder.decodeUint16(),
    arrayElementSize: 2,
    isValidObjectKeyType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Int32 = {
  $: {
    encode: function(value, message) {
      message.appendInt32(value);
    },
    decode: decoder => decoder.decodeInt32(),
    arrayElementSize: 4,
    isValidObjectKeyType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Uint32 = {
  $: {
    encode: function(value, message) {
      message.appendUint32(value);
    },
    decode: decoder => decoder.decodeUint32(),
    arrayElementSize: 4,
    isValidObjectKeyType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Int64 = {
  $: {
    encode: function(value, message) {
      message.appendInt64(value);
    },
    decode: decoder => decoder.decodeInt64(),
    arrayElementSize: 8,
    isValidObjectKeyType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Uint64 = {
  $: {
    encode: function(value, message) {
      message.appendUint64(value);
    },
    decode: decoder => decoder.decodeUint64(),
    arrayElementSize: 8,
    isValidObjectKeyType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Float = {
  $: {
    encode: function(value, message) {
      message.appendFloat(value);
    },
    decode: decoder => decoder.decodeFloat(),
    arrayElementSize: 4,
    isValidObjectKeyType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Double = {
  $: {
    encode: function(value, message) {
      message.appendDouble(value);
    },
    decode: decoder => decoder.decodeDouble(),
    arrayElementSize: 8,
    isValidObjectKeyType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Handle = {
  $: {
    encode: function(value, message) {
      message.appendHandle(value);
    },
    encodeNull: function(message) {
      message.appendUint32(0);
    },
    decode: decoder => decoder.decodeHandle(),
    arrayElementSize: 4,
    isValidObjectKeyType: false,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.String = {
  $: {
    encode: function(value, message) {
      message.appendString(value);
    },
    encodeNull: function(message) {
      message.appendNullOffset();
    },
    decode: decoder => decoder.decodeString(),
    arrayElementSize: 8,
    isValidObjectKeyType: true,
  }
};

/**
 * @param {!mojo.internal.MojomType} elementType
 * @param {boolean} elementNullable
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Array = function(elementType, elementNullable) {
  /** @type {!mojo.internal.ArraySpec} */
  const arraySpec = {
    elementType: elementType,
    elementNullable: elementNullable,
  };
  return {
    $: {
      arraySpec: arraySpec,
      encode: function(value, message) {
        message.appendArray(arraySpec, value);
      },
      encodeNull: function(message) {
        message.appendNullOffset();
      },
      decode: decoder => decoder.decodeArray(arraySpec),
      arrayElementSize: 8,
      isValidObjectKeyType: false,
    },
  };
};

/**
 * @param {!mojo.internal.MojomType} keyType
 * @param {!mojo.internal.MojomType} valueType
 * @param {boolean} valueNullable
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Map = function(keyType, valueType, valueNullable) {
  /** @type {!mojo.internal.MapSpec} */
  const mapSpec = {
    keyType: keyType,
    valueType: valueType,
    valueNullable: valueNullable,
  };
  return {
    $: {
      mapSpec: mapSpec,
      encode: function(value, message) {
        message.appendMap(mapSpec, value);
      },
      encodeNull: function(message) {
        message.appendNullOffset();
      },
      decode: decoder => decoder.decodeMap(mapSpec),
      arrayElementSize: 8,
      isValidObjectKeyType: false,
    },
  };
};

/**
 * @param {!Object} properties
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.Enum = function(properties) {
  return {
    $: {
      encode: function(value, message) {
        // TODO: Do some sender-side error checking on the input value.
        message.appendUint32(value);
      },
      decode: decoder => {
        const value = decoder.decodeInt32();
        // TODO: validate
        return value;
      },
      arrayElementSize: 4,
      isValidObjectKeyType: true,
    },
  };
};

/**
 * @param {string} name
 * @param {!mojo.internal.MojomType} type
 * @param {*} defaultValue
 * @param {boolean} nullable
 * @return {!mojo.mojom.StructFieldSpec}
 * @export
 */
mojo.mojom.StructField = function(name, type, defaultValue, nullable) {
  return {
    name: name,
    type: type,
    defaultValue: defaultValue,
    nullable: nullable,
  };
};

/**
 * @param {!Object} objectToBlessAsType
 * @param {string} name
 * @param {!Array<!mojo.internal.StructFieldSpec>} fields
 * @export
 */
mojo.mojom.Struct = function(objectToBlessAsType, name, fields) {
  /** @type {!mojo.internal.StructSpec} */
  const structSpec = {
    name: name,
    fields: fields,
  };
  objectToBlessAsType.$ = {
    structSpec: structSpec,
    encode: function(value, message) {
      message.appendStruct(structSpec, value);
    },
    encodeNull: function(message) {
      message.appendNullOffset();
    },
    decode: decoder => decoder.decodeStruct(structSpec),
    arrayElementSize: 8,
    isValidObjectKeyType: false,
  };
};

/**
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.InterfaceProxy = function(type) {
  return {
    $: {
      /**
       * @param {!{proxy: mojo.internal.InterfaceProxyBase}} value
       * @param {!mojo.internal.Message} message
       */
      encode: function(value, message) {
        if (!(value instanceof type))
          throw new Error('Invalid proxy type. Expected ' + type.name);
        if (!value.proxy.handle)
          throw new Error('Unexpected null ' + type.name);

        message.appendHandle(value.proxy.handle);
        message.appendUint32(0);  // TODO: Support versioning
        value.proxy.unbind();
      },
      encodeNull: function(message) {
        message.appendUint32(0xffffffff);
        message.appendUint32(0);
      },
      decode: decoder => decoder.decodeInterfaceProxy(type),
      arrayElementSize: 8,
      isValidObjectKeyType: false,
    },
  };
};

/**
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.InterfaceRequest = function(type) {
  return {
    $: {
      encode: function(value, message) {
        if (!(value instanceof type))
          throw new Error('Invalid request type. Expected ' + type.name);
        if (!value.handle)
          throw new Error('Unexpected null ' + type.name);

        message.appendHandle(value.handle);
      },
      encodeNull: function(message) {
        message.appendUint32(0xffffffff);
      },
      decode: decoder => decoder.decodeInterfaceRequest(type),
      arrayElementSize: 4,
      isValidObjectKeyType: false,
    },
  };
};

/**
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.AssociatedInterfaceProxy = function(type) {
  return {
    $: {
      type: type,
      encode: function(value, message) {
        throw new Error('Associated interfaces not supported yet.');
      },
      decode: decoder => {
        throw new Error('Associated interfaces not supported yet.');
      },
    },
  };
};

/**
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.mojom.AssociatedInterfaceRequest = function(type) {
  return {
    $: {
      type: type,
      encode: function(value, message) {
        throw new Error('Associated interfaces not supported yet.');
      },
      decode: decoder => {
        throw new Error('Associated interfaces not supported yet.');
      },
    },
  };
};
