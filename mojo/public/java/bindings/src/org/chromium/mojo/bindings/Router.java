// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.mojo.system.MessagePipeHandle;

/**
 * A {@link Router} will handle mojo message and forward those to a {@link Connector}. It deals with
 * parsing of headers and adding of request ids in order to be able to match a response to a
 * request.
 */
@NullMarked
public interface Router extends MessageReceiverWithResponder, HandleOwner<MessagePipeHandle> {
    public static final int PRIMARY_INTERFACE_ID = 0;

    /** Start listening for incoming messages. */
    void start();

    /**
     * Set the priamry {@link MessageReceiverWithResponder} that will deserialize and use the
     * message received from the pipe. Primary pipes own the state of the pipe, as opposed to
     * associated pipes.
     */
    void setPrimaryStub(Stub primaryMessageReceiver) throws BadMessageException;

    /** Set the handle that will be notified of errors on the message pipe. */
    void setErrorHandler(ConnectionErrorHandler errorHandler);
}
