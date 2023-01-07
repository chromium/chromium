// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function Connector(handle) {
    if (!(handle instanceof MojoHandle))
      throw new Error("Connector: not a handle " + handle);
    this.handle_ = handle;
    this.dropWrites_ = false;
    this.error_ = false;
    this.incomingReceiver_ = null;
    this.readWatcher_ = null;
    this.errorHandler_ = null;
    this.paused_ = false;

    this.waitToReadMore();
  }

  Connector.prototype.close = function() {
    this.cancelWait();
    if (this.handle_ != null) {
      this.handle_.close();
      this.handle_ = null;
    }
  };

  Connector.prototype.pauseIncomingMethodCallProcessing = function() {
    if (this.paused_) {
      return;
    }
    this.paused_= true;
    this.cancelWait();
  };

  Connector.prototype.resumeIncomingMethodCallProcessing = function() {
    if (!this.paused_) {
      return;
    }
    this.paused_= false;
    this.waitToReadMore();
  };

  Connector.prototype.accept = function(message) {
    if (this.error_)
      return false;

    if (this.dropWrites_)
      return true;

    var result = this.handle_.writeMessage(
        new Uint8Array(message.buffer.arrayBuffer), message.handles);
    switch (result) {
      case Mojo.RESULT_OK:
        // The handles were successfully transferred, so we don't own them
        // anymore.
        message.handles = [];
        break;
      case Mojo.RESULT_FAILED_PRECONDITION:
        // There's no point in continuing to write to this pipe since the other
        // end is gone. Avoid writing any future messages. Hide write failures
        // from the caller since we'd like them to continue consuming any
        // backlog of incoming messages before regarding the message pipe as
        // closed.
        this.dropWrites_ = true;
        break;
      default:
        // This particular write was rejected, presumably because of bad input.
        // The pipe is not necessarily in a bad state.
        return false;
    }
    return true;
  };

  Connector.prototype.setIncomingReceiver = function(receiver) {
    this.incomingReceiver_ = receiver;
  };

  Connector.prototype.setErrorHandler = function(handler) {
    this.errorHandler_ = handler;
  };

  Connector.prototype.readMore_ = function(result) {
    for (;;) {
      if (this.paused_) {
        return;
      }

      var read = this.handle_.readMessage();
      if (this.handle_ == null) // The connector has been closed.
        return;
      if (read.result == Mojo.RESULT_SHOULD_WAIT)
        return;
      if (read.result != Mojo.RESULT_OK) {
        this.handleError(read.result !== Mojo.RESULT_FAILED_PRECONDITION,
            false);
        return;
      }
      var messageBuffer = new internal.Buffer(read.buffer);
      var message = new internal.Message(messageBuffer, read.handles);
      var receiverResult = this.incomingReceiver_ &&
          this.incomingReceiver_.accept(message);

      // Dispatching the message may have closed the connector.
      if (this.handle_ == null)
        return;

      // Handle invalid incoming message.
      if (!internal.isTestingMode() && !receiverResult) {
        // TODO(yzshen): Consider notifying the embedder.
        this.handleError(true, false);
      }
    }
  };

  Connector.prototype.cancelWait = function() {
    if (this.readWatcher_) {
      this.readWatcher_.cancel();
      this.readWatcher_ = null;
    }
  };

  Connector.prototype.waitToReadMore = function() {
    if (this.handle_) {
      this.readWatcher_ = this.handle_.watch({readable: true},
                                             this.readMore_.bind(this));
    }
  };

  Connector.prototype.handleError = function(forcePipeReset,
                                             forceAsyncHandler) {
    if (this.error_ || this.handle_ === null) {
      return;
    }

    if (this.paused_) {
      // Enforce calling the error handler asynchronously if the user has
      // paused receiving messages. We need to wait until the user starts
      // receiving messages again.
      forceAsyncHandler = true;
    }

    if (!forcePipeReset && forceAsyncHandler) {
      forcePipeReset = true;
    }

    this.cancelWait();
    if (forcePipeReset) {
      this.handle_.close();
      var dummyPipe = Mojo.createMessagePipe();
      this.handle_ = dummyPipe.handle0;
    }

    if (forceAsyncHandler) {
      if (!this.paused_) {
        this.waitToReadMore();
      }
    } else {
      this.error_ = true;
      if (this.errorHandler_) {
        this.errorHandler_.onError();
      }
    }
  };

  internal.Connector = Connector;
})();
