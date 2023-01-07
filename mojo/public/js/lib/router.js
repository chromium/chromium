// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  /**
   * The state of |endpoint|. If both the endpoint and its peer have been
   * closed, removes it from |endpoints_|.
   * @enum {string}
   */
  var EndpointStateUpdateType = {
    ENDPOINT_CLOSED: 'endpoint_closed',
    PEER_ENDPOINT_CLOSED: 'peer_endpoint_closed'
  };

  function check(condition, output) {
    if (!condition) {
      // testharness.js does not rethrow errors so the error stack needs to be
      // included as a string in the error we throw for debugging layout tests.
      throw new Error((new Error()).stack);
    }
  }

  function InterfaceEndpoint(router, interfaceId) {
    this.router_ = router;
    this.id = interfaceId;
    this.closed = false;
    this.peerClosed = false;
    this.handleCreated = false;
    this.disconnectReason = null;
    this.client = null;
  }

  InterfaceEndpoint.prototype.sendMessage = function(message) {
    message.setInterfaceId(this.id);
    return this.router_.connector_.accept(message);
  };

  function Router(handle, setInterfaceIdNamespaceBit) {
    if (!(handle instanceof MojoHandle)) {
      throw new Error("Router constructor: Not a handle");
    }
    if (setInterfaceIdNamespaceBit === undefined) {
      setInterfaceIdNamespaceBit = false;
    }

    this.connector_ = new internal.Connector(handle);

    this.connector_.setIncomingReceiver({
        accept: this.accept.bind(this),
    });
    this.connector_.setErrorHandler({
        onError: this.onPipeConnectionError.bind(this),
    });

    this.setInterfaceIdNamespaceBit_ = setInterfaceIdNamespaceBit;
    // |cachedMessageData| caches infomation about a message, so it can be
    // processed later if a client is not yet attached to the target endpoint.
    this.cachedMessageData = null;
    this.controlMessageHandler_ = new internal.PipeControlMessageHandler(this);
    this.controlMessageProxy_ =
        new internal.PipeControlMessageProxy(this.connector_);
    this.nextInterfaceIdValue_ = 1;
    this.encounteredError_ = false;
    this.endpoints_ = new Map();
  }

  Router.prototype.associateInterface = function(handleToSend) {
    if (!handleToSend.pendingAssociation()) {
      return internal.kInvalidInterfaceId;
    }

    var id = 0;
    do {
      if (this.nextInterfaceIdValue_ >= internal.kInterfaceIdNamespaceMask) {
        this.nextInterfaceIdValue_ = 1;
      }
      id = this.nextInterfaceIdValue_++;
      if (this.setInterfaceIdNamespaceBit_) {
        id += internal.kInterfaceIdNamespaceMask;
      }
    } while (this.endpoints_.has(id));

    var endpoint = new InterfaceEndpoint(this, id);
    this.endpoints_.set(id, endpoint);
    if (this.encounteredError_) {
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
    }
    endpoint.handleCreated = true;

    if (!handleToSend.notifyAssociation(id, this)) {
      // The peer handle of |handleToSend|, which is supposed to join this
      // associated group, has been closed.
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.ENDPOINT_CLOSED);

      pipeControlMessageproxy.notifyPeerEndpointClosed(id,
          handleToSend.disconnectReason());
    }

    return id;
  };

  Router.prototype.attachEndpointClient = function(
      interfaceEndpointHandle, interfaceEndpointClient) {
    check(internal.isValidInterfaceId(interfaceEndpointHandle.id()));
    check(interfaceEndpointClient);

    var endpoint = this.endpoints_.get(interfaceEndpointHandle.id());
    check(endpoint);
    check(!endpoint.client);
    check(!endpoint.closed);
    endpoint.client = interfaceEndpointClient;

    if (endpoint.peerClosed) {
      setTimeout(endpoint.client.notifyError.bind(endpoint.client), 0);
    }

    if (this.cachedMessageData && interfaceEndpointHandle.id() ===
        this.cachedMessageData.message.getInterfaceId()) {
      setTimeout((function() {
        if (!this.cachedMessageData) {
          return;
        }

        var targetEndpoint = this.endpoints_.get(
            this.cachedMessageData.message.getInterfaceId());
        // Check that the target endpoint's client still exists.
        if (targetEndpoint && targetEndpoint.client) {
          var message = this.cachedMessageData.message;
          var messageValidator = this.cachedMessageData.messageValidator;
          this.cachedMessageData = null;
          this.connector_.resumeIncomingMethodCallProcessing();
          var ok = endpoint.client.handleIncomingMessage(message,
              messageValidator);

          // Handle invalid cached incoming message.
          if (!internal.isTestingMode() && !ok) {
            this.connector_.handleError(true, true);
          }
        }
      }).bind(this), 0);
    }

    return endpoint;
  };

  Router.prototype.detachEndpointClient = function(
      interfaceEndpointHandle) {
    check(internal.isValidInterfaceId(interfaceEndpointHandle.id()));
    var endpoint = this.endpoints_.get(interfaceEndpointHandle.id());
    check(endpoint);
    check(endpoint.client);
    check(!endpoint.closed);

    endpoint.client = null;
  };

  Router.prototype.createLocalEndpointHandle = function(
      interfaceId) {
    if (!internal.isValidInterfaceId(interfaceId)) {
      return new internal.InterfaceEndpointHandle();
    }

    // Unless it is the primary ID, |interfaceId| is from the remote side and
    // therefore its namespace bit is supposed to be different than the value
    // that this router would use.
    if (!internal.isPrimaryInterfaceId(interfaceId) &&
        this.setInterfaceIdNamespaceBit_ ===
            internal.hasInterfaceIdNamespaceBitSet(interfaceId)) {
      return new internal.InterfaceEndpointHandle();
    }

    var endpoint = this.endpoints_.get(interfaceId);

    if (!endpoint) {
      endpoint = new InterfaceEndpoint(this, interfaceId);
      this.endpoints_.set(interfaceId, endpoint);

      check(!endpoint.handleCreated);

      if (this.encounteredError_) {
        this.updateEndpointStateMayRemove(endpoint,
            EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
      }
    } else {
      // If the endpoint already exist, it is because we have received a
      // notification that the peer endpoint has closed.
      check(!endpoint.closed);
      check(endpoint.peerClosed);

      if (endpoint.handleCreated) {
        return new internal.InterfaceEndpointHandle();
      }
    }

    endpoint.handleCreated = true;
    return new internal.InterfaceEndpointHandle(interfaceId, this);
  };

  Router.prototype.accept = function(message) {
    var messageValidator = new internal.Validator(message);
    var err = messageValidator.validateMessageHeader();

    var ok = false;
    if (err !== internal.validationError.NONE) {
      internal.reportValidationError(err);
    } else if (message.deserializeAssociatedEndpointHandles(this)) {
      if (internal.isPipeControlMessage(message)) {
        ok = this.controlMessageHandler_.accept(message);
      } else {
        var interfaceId = message.getInterfaceId();
        var endpoint = this.endpoints_.get(interfaceId);
        if (!endpoint || endpoint.closed) {
          return true;
        }

        if (!endpoint.client) {
          // We need to wait until a client is attached in order to dispatch
          // further messages.
          this.cachedMessageData = {message: message,
              messageValidator: messageValidator};
          this.connector_.pauseIncomingMethodCallProcessing();
          return true;
        }
        ok = endpoint.client.handleIncomingMessage(message, messageValidator);
      }
    }
    return ok;
  };

  Router.prototype.close = function() {
    this.connector_.close();
    // Closing the message pipe won't trigger connection error handler.
    // Explicitly call onPipeConnectionError() so that associated endpoints
    // will get notified.
    this.onPipeConnectionError();
  };

  Router.prototype.onPeerAssociatedEndpointClosed = function(interfaceId,
      reason) {
    var endpoint = this.endpoints_.get(interfaceId);
    if (!endpoint) {
      endpoint = new InterfaceEndpoint(this, interfaceId);
      this.endpoints_.set(interfaceId, endpoint);
    }

    if (reason) {
      endpoint.disconnectReason = reason;
    }

    if (!endpoint.peerClosed) {
      if (endpoint.client) {
        setTimeout(endpoint.client.notifyError.bind(endpoint.client, reason),
                   0);
      }
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
    }
    return true;
  };

  Router.prototype.onPipeConnectionError = function() {
    this.encounteredError_ = true;

    for (var endpoint of this.endpoints_.values()) {
      if (endpoint.client) {
        setTimeout(
            endpoint.client.notifyError.bind(
                endpoint.client, endpoint.disconnectReason),
            0);
      }
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
    }
  };

  Router.prototype.closeEndpointHandle = function(interfaceId, reason) {
    if (!internal.isValidInterfaceId(interfaceId)) {
      return;
    }
    var endpoint = this.endpoints_.get(interfaceId);
    check(endpoint);
    check(!endpoint.client);
    check(!endpoint.closed);

    this.updateEndpointStateMayRemove(endpoint,
        EndpointStateUpdateType.ENDPOINT_CLOSED);

    if (!internal.isPrimaryInterfaceId(interfaceId) || reason) {
      this.controlMessageProxy_.notifyPeerEndpointClosed(interfaceId, reason);
    }

    if (this.cachedMessageData && interfaceId ===
        this.cachedMessageData.message.getInterfaceId()) {
      this.cachedMessageData = null;
      this.connector_.resumeIncomingMethodCallProcessing();
    }
  };

  Router.prototype.updateEndpointStateMayRemove = function(endpoint,
      endpointStateUpdateType) {
    if (endpointStateUpdateType === EndpointStateUpdateType.ENDPOINT_CLOSED) {
      endpoint.closed = true;
    } else {
      endpoint.peerClosed = true;
    }
    if (endpoint.closed && endpoint.peerClosed) {
      this.endpoints_.delete(endpoint.id);
    }
  };

  internal.Router = Router;
})();
