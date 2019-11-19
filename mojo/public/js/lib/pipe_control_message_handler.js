// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function validateControlRequestWithoutResponse(message) {
    var messageValidator = new internal.Validator(message);
    var error = messageValidator.validateMessageIsRequestWithoutResponse();
    if (error != internal.validationError.NONE) {
      throw error;
    }

    if (message.getName() != mojo.pipeControl.RUN_OR_CLOSE_PIPE_MESSAGE_ID) {
      throw new Error(
        "Control message name is not RUN_OR_CLOSE_PIPE_MESSAGE_ID");
    }

    // Validate payload.
    error = mojo.pipeControl.RunOrClosePipeMessageParams.validate(
        messageValidator, message.getHeaderNumBytes());
    if (error != internal.validationError.NONE) {
      throw error;
    }
  }

  function runOrClosePipe(message, delegate) {
    var reader = new internal.MessageReader(message);
    var runOrClosePipeMessageParams = reader.decodeStruct(
        mojo.pipeControl.RunOrClosePipeMessageParams);
    var event = runOrClosePipeMessageParams.input
        .peerAssociatedEndpointClosedEvent;
    return delegate.onPeerAssociatedEndpointClosed(event.id,
        event.disconnectReason);
  }

  function isPipeControlMessage(message) {
    return !internal.isValidInterfaceId(message.getInterfaceId());
  }

  function PipeControlMessageHandler(delegate) {
    this.delegate_ = delegate;
  }

  PipeControlMessageHandler.prototype.accept = function(message) {
    validateControlRequestWithoutResponse(message);
    return runOrClosePipe(message, this.delegate_);
  };

  internal.PipeControlMessageHandler = PipeControlMessageHandler;
  internal.isPipeControlMessage = isPipeControlMessage;
})();
