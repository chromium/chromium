// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function InterfaceEndpointClient(interfaceEndpointHandle, receiver,
      interfaceVersion) {
    this.controller_ = null;
    this.encounteredError_ = false;
    this.handle_ = interfaceEndpointHandle;
    this.incomingReceiver_ = receiver;

    if (interfaceVersion !== undefined) {
      this.controlMessageHandler_ = new internal.ControlMessageHandler(
          interfaceVersion);
    } else {
      this.controlMessageProxy_ = new internal.ControlMessageProxy(this);
    }

    this.nextRequestID_ = 0;
    this.completers_ = new Map();
    this.payloadValidators_ = [];
    this.connectionErrorHandler_ = null;

    if (interfaceEndpointHandle.pendingAssociation()) {
      interfaceEndpointHandle.setAssociationEventHandler(
          this.onAssociationEvent.bind(this));
    } else {
      this.initControllerIfNecessary_();
    }
  }

  InterfaceEndpointClient.prototype.initControllerIfNecessary_ = function() {
    if (!this.handle_) {
      return false;
    }

    if (this.controller_ || this.handle_.pendingAssociation()) {
      return true;
    }

    this.controller_ = this.handle_.groupController().attachEndpointClient(
        this.handle_, this);
    return true;
  };

  InterfaceEndpointClient.prototype.onAssociationEvent = function(
      associationEvent) {
    if (associationEvent === internal.AssociationEvent.ASSOCIATED) {
      this.initControllerIfNecessary_();
    } else if (associationEvent ===
          internal.AssociationEvent.PEER_CLOSED_BEFORE_ASSOCIATION) {
      setTimeout(this.notifyError.bind(this, this.handle_.disconnectReason()),
                 0);
    }
  };

  InterfaceEndpointClient.prototype.passHandle = function() {
    if (!this.handle_.isValid()) {
      return new internal.InterfaceEndpointHandle();
    }

    // Used to clear the previously set callback.
    this.handle_.setAssociationEventHandler(undefined);

    if (this.controller_) {
      this.controller_ = null;
      this.handle_.groupController().detachEndpointClient(this.handle_);
    }
    var handle = this.handle_;
    this.handle_ = null;
    return handle;
  };

  InterfaceEndpointClient.prototype.close = function(reason) {
    var handle = this.passHandle();
    handle.reset(reason);
  };

  InterfaceEndpointClient.prototype.accept = function(message) {
    if (message.associatedEndpointHandles.length > 0) {
      message.serializeAssociatedEndpointHandles(
          this.handle_.groupController());
    }

    if (this.encounteredError_) {
      return false;
    }

    if (!this.initControllerIfNecessary_()) {
      return false;
    }
    return this.controller_.sendMessage(message);
  };

  InterfaceEndpointClient.prototype.acceptAndExpectResponse = function(
      message) {
    if (message.associatedEndpointHandles.length > 0) {
      message.serializeAssociatedEndpointHandles(
          this.handle_.groupController());
    }

    if (this.encounteredError_) {
      return Promise.reject();
    }

    if (!this.initControllerIfNecessary_()) {
      return Promise.reject(Error('Endpoint has been closed'));
    }

    // Reserve 0 in case we want it to convey special meaning in the future.
    var requestID = this.nextRequestID_++;
    if (requestID === 0)
      requestID = this.nextRequestID_++;

    message.setRequestID(requestID);
    var result = this.controller_.sendMessage(message);
    if (!result)
      return Promise.reject(Error("Connection error"));

    var completer = {};
    this.completers_.set(requestID, completer);
    return new Promise(function(resolve, reject) {
      completer.resolve = resolve;
      completer.reject = reject;
    });
  };

  InterfaceEndpointClient.prototype.setPayloadValidators = function(
      payloadValidators) {
    this.payloadValidators_ = payloadValidators;
  };

  InterfaceEndpointClient.prototype.setIncomingReceiver = function(receiver) {
    this.incomingReceiver_ = receiver;
  };

  InterfaceEndpointClient.prototype.setConnectionErrorHandler = function(
      handler) {
    this.connectionErrorHandler_ = handler;
  };

  InterfaceEndpointClient.prototype.handleIncomingMessage = function(message,
      messageValidator) {
    var noError = internal.validationError.NONE;
    var err = noError;
    for (var i = 0; err === noError && i < this.payloadValidators_.length; ++i)
      err = this.payloadValidators_[i](messageValidator);

    if (err == noError) {
      return this.handleValidIncomingMessage_(message);
    } else {
      internal.reportValidationError(err);
      return false;
    }
  };

  InterfaceEndpointClient.prototype.handleValidIncomingMessage_ = function(
      message) {
    if (internal.isTestingMode()) {
      return true;
    }

    if (this.encounteredError_) {
      return false;
    }

    var ok = false;

    if (message.expectsResponse()) {
      if (internal.isInterfaceControlMessage(message) &&
          this.controlMessageHandler_) {
        ok = this.controlMessageHandler_.acceptWithResponder(message, this);
      } else if (this.incomingReceiver_) {
        ok = this.incomingReceiver_.acceptWithResponder(message, this);
      }
    } else if (message.isResponse()) {
      var reader = new internal.MessageReader(message);
      var requestID = reader.requestID;
      var completer = this.completers_.get(requestID);
      if (completer) {
        this.completers_.delete(requestID);
        completer.resolve(message);
        ok = true;
      } else {
        console.log("Unexpected response with request ID: " + requestID);
      }
    } else {
      if (internal.isInterfaceControlMessage(message) &&
          this.controlMessageHandler_) {
        ok = this.controlMessageHandler_.accept(message);
      } else if (this.incomingReceiver_) {
        ok = this.incomingReceiver_.accept(message);
      }
    }
    return ok;
  };

  InterfaceEndpointClient.prototype.notifyError = function(reason) {
    if (this.encounteredError_) {
      return;
    }
    this.encounteredError_ = true;

    this.completers_.forEach(function(value) {
      value.reject();
    });
    this.completers_.clear();  // Drop any responders.

    if (this.connectionErrorHandler_) {
      this.connectionErrorHandler_(reason);
    }
  };

  InterfaceEndpointClient.prototype.queryVersion = function() {
    return this.controlMessageProxy_.queryVersion();
  };

  InterfaceEndpointClient.prototype.requireVersion = function(version) {
    this.controlMessageProxy_.requireVersion(version);
  };

  InterfaceEndpointClient.prototype.getEncounteredError = function() {
    return this.encounteredError_;
  };

  internal.InterfaceEndpointClient = InterfaceEndpointClient;
})();
