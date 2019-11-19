// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function constructRunOrClosePipeMessage(runOrClosePipeInput) {
    var runOrClosePipeMessageParams = new
        mojo.interfaceControl.RunOrClosePipeMessageParams();
    runOrClosePipeMessageParams.input = runOrClosePipeInput;

    var messageName = mojo.interfaceControl.RUN_OR_CLOSE_PIPE_MESSAGE_ID;
    var payloadSize =
        mojo.interfaceControl.RunOrClosePipeMessageParams.encodedSize;
    var builder = new internal.MessageV0Builder(messageName, payloadSize);
    builder.encodeStruct(mojo.interfaceControl.RunOrClosePipeMessageParams,
                         runOrClosePipeMessageParams);
    var message = builder.finish();
    return message;
  }

  function validateControlResponse(message) {
    var messageValidator = new internal.Validator(message);
    var error = messageValidator.validateMessageIsResponse();
    if (error != internal.validationError.NONE) {
      throw error;
    }

    if (message.getName() != mojo.interfaceControl.RUN_MESSAGE_ID) {
      throw new Error("Control message name is not RUN_MESSAGE_ID");
    }

    // Validate payload.
    error = mojo.interfaceControl.RunResponseMessageParams.validate(
        messageValidator, message.getHeaderNumBytes());
    if (error != internal.validationError.NONE) {
      throw error;
    }
  }

  function acceptRunResponse(message) {
    validateControlResponse(message);

    var reader = new internal.MessageReader(message);
    var runResponseMessageParams = reader.decodeStruct(
        mojo.interfaceControl.RunResponseMessageParams);

    return Promise.resolve(runResponseMessageParams);
  }

 /**
  * Sends the given run message through the receiver.
  * Accepts the response message from the receiver and decodes the message
  * struct to RunResponseMessageParams.
  *
  * @param  {Router} receiver
  * @param  {RunMessageParams} runMessageParams to be sent via a message.
  * @return {Promise} that resolves to a RunResponseMessageParams.
  */
  function sendRunMessage(receiver, runMessageParams) {
    var messageName = mojo.interfaceControl.RUN_MESSAGE_ID;
    var payloadSize = mojo.interfaceControl.RunMessageParams.encodedSize;
    // |requestID| is set to 0, but is later properly set by Router.
    var builder = new internal.MessageV1Builder(messageName,
        payloadSize, internal.kMessageExpectsResponse, 0);
    builder.encodeStruct(mojo.interfaceControl.RunMessageParams,
                         runMessageParams);
    var message = builder.finish();

    return receiver.acceptAndExpectResponse(message).then(acceptRunResponse);
  }

  function ControlMessageProxy(receiver) {
    this.receiver_ = receiver;
  }

  ControlMessageProxy.prototype.queryVersion = function() {
    var runMessageParams = new mojo.interfaceControl.RunMessageParams();
    runMessageParams.input = new mojo.interfaceControl.RunInput();
    runMessageParams.input.queryVersion =
        new mojo.interfaceControl.QueryVersion();

    return sendRunMessage(this.receiver_, runMessageParams).then(function(
        runResponseMessageParams) {
      return runResponseMessageParams.output.queryVersionResult.version;
    });
  };

  ControlMessageProxy.prototype.requireVersion = function(version) {
    var runOrClosePipeInput = new mojo.interfaceControl.RunOrClosePipeInput();
    runOrClosePipeInput.requireVersion =
        new mojo.interfaceControl.RequireVersion({'version': version});
    var message = constructRunOrClosePipeMessage(runOrClosePipeInput);
    this.receiver_.accept(message);
  };

  internal.ControlMessageProxy = ControlMessageProxy;
})();
