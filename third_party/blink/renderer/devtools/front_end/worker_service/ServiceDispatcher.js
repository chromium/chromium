// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @interface
 */
function Service() {
}

Service.prototype = {
  /**
   * @return {!Promise}
   */
  dispose() {},

  /**
   * @return {function(string)}
   */
  setNotify(notify) {}
};

/**
 * @unrestricted
 */
ServiceDispatcher = class {
  /**
   * @param {!ServicePort} port
   */
  constructor(port) {
    /** @type {!Map<string, !Object>} */
    this._objects = new Map();
    this._lastObjectId = 1;
    this._port = port;
    this._port.setHandlers(this._dispatchMessageWrapped.bind(this), this._connectionClosed.bind(this));
  }

  /**
   * @param {string} data
   */
  _dispatchMessageWrapped(data) {
    const message = JSON.parse(data);
    try {
      if (!(message instanceof Object)) {
        this._sendErrorResponse(message['id'], 'Malformed message');
        return;
      }
      this._dispatchMessage(message);
    } catch (e) {
      this._sendErrorResponse(message['id'], e.toString() + ' ' + e.stack);
    }
  }

  /**
   * @param {!Object} message
   */
  _dispatchMessage(message) {
    const domainAndMethod = message['method'].split('.');
    const serviceName = domainAndMethod[0];
    const method = domainAndMethod[1];

    if (method === 'create') {
      const extensions =
          self.runtime.extensions(Service).filter(extension => extension.descriptor()['name'] === serviceName);
      if (!extensions.length) {
        this._sendErrorResponse(message['id'], 'Could not resolve service \'' + serviceName + '\'');
        return;
      }
      extensions[0].instance().then(object => {
        const id = String(this._lastObjectId++);
        object.setNotify(this._notify.bind(this, id, serviceName));
        this._objects.set(id, object);
        this._sendResponse(message['id'], {id: id});
      });
    } else if (method === 'dispose') {
      const object = this._objects.get(message['params']['id']);
      if (!object) {
        console.error('Could not look up object with id for ' + JSON.stringify(message));
        return;
      }
      this._objects.delete(message['params']['id']);
      object.dispose().then(() => this._sendResponse(message['id'], {}));
    } else {
      if (!message['params']) {
        console.error('No params in the message: ' + JSON.stringify(message));
        return;
      }
      const object = this._objects.get(message['params']['id']);
      if (!object) {
        console.error('Could not look up object with id for ' + JSON.stringify(message));
        return;
      }
      const handler = object[method];
      if (!(handler instanceof Function)) {
        console.error('Handler for \'' + method + '\' is missing.');
        return;
      }
      object[method](message['params']).then(result => this._sendResponse(message['id'], result));
    }
  }

  _connectionClosed() {
    for (const object of this._objects.values())
      object.dispose();
    this._objects.clear();
  }

  /**
   * @param {string} objectId
   * @param {string} serviceName
   * @param {string} method
   * @param {!Object} params
   */
  _notify(objectId, serviceName, method, params) {
    params['id'] = objectId;
    const message = {method: serviceName + '.' + method, params: params};
    this._port.send(JSON.stringify(message));
  }

  /**
   * @param {string} messageId
   * @param {!Object} result
   */
  _sendResponse(messageId, result) {
    const message = {id: messageId, result: result};
    this._port.send(JSON.stringify(message));
  }

  /**
   * @param {string} messageId
   * @param {string} error
   */
  _sendErrorResponse(messageId, error) {
    const message = {id: messageId, error: error};
    this._port.send(JSON.stringify(message));
  }
};

/**
 * @implements {ServicePort}
 * @unrestricted
 */
WorkerServicePort = class {
  /**
   * @param {!Port|!Worker} port
   */
  constructor(port) {
    this._port = port;
    this._port.onmessage = this._onMessage.bind(this);
    this._port.onerror = console.error;
  }

  /**
   * @override
   * @param {function(string)} messageHandler
   * @param {function(string)} closeHandler
   */
  setHandlers(messageHandler, closeHandler) {
    this._messageHandler = messageHandler;
    this._closeHandler = closeHandler;
  }

  /**
   * @override
   * @param {string} data
   * @return {!Promise}
   */
  send(data) {
    this._port.postMessage(data);
    return Promise.resolve();
  }

  /**
   * @override
   * @return {!Promise}
   */
  close() {
    return Promise.resolve();
  }

  /**
   * @param {!MessageEvent} event
   */
  _onMessage(event) {
    this._messageHandler(event.data);
  }
};

const dispatchers = [];

const worker = /** @type {!Object} */ (self);
const servicePort = new WorkerServicePort(/** @type {!Worker} */ (worker));
dispatchers.push(new ServiceDispatcher(servicePort));
