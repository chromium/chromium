// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.bindings.Interface.Manager;
import org.chromium.mojo.bindings.Interface.Proxy;
import org.chromium.mojo.bindings.interfacecontrol.InterfaceControlMessagesConstants;
import org.chromium.mojo.bindings.interfacecontrol.QueryVersionResult;
import org.chromium.mojo.bindings.interfacecontrol.RunInput;
import org.chromium.mojo.bindings.interfacecontrol.RunMessageParams;
import org.chromium.mojo.bindings.interfacecontrol.RunOrClosePipeInput;
import org.chromium.mojo.bindings.interfacecontrol.RunOrClosePipeMessageParams;
import org.chromium.mojo.bindings.interfacecontrol.RunOutput;
import org.chromium.mojo.bindings.interfacecontrol.RunResponseMessageParams;
import org.chromium.mojo.system.Core;

/**
 * Helper class to handle interface control messages. See
 * mojo/public/interfaces/bindings/interface_control_messages.mojom.
 */
public class InterfaceControlMessagesHelper {
    /**
     * Callback interface for the async response to {@link
     * InterfaceControlMessagesHelper#sendRunMessage}.
     */
    interface SendRunMessageCallback {
        public void call(RunResponseMessageParams params);
    }

    /**
     * MessageReceiver that forwards a message containing a {@link RunResponseMessageParams} to a
     * callback.
     */
    private static class RunResponseForwardToCallback extends SideEffectFreeCloseable
            implements MessageReceiver {
        private final SendRunMessageCallback mCallback;

        RunResponseForwardToCallback(SendRunMessageCallback callback) {
            mCallback = callback;
        }

        /**
         * @see MessageReceiver#accept(Message)
         */
        @Override
        public boolean accept(Message message) {
            RunResponseMessageParams response =
                    RunResponseMessageParams.deserialize(message.asServiceMessage().getPayload());
            mCallback.call(response);
            return true;
        }
    }

    /** Sends the given run message through the receiver, registering the callback. */
    public static void sendRunMessage(
            Core core,
            MessageReceiverWithResponder receiver,
            RunMessageParams params,
            SendRunMessageCallback callback) {
        Message message =
                params.serializeWithHeader(
                        core,
                        new MessageHeader(
                                InterfaceControlMessagesConstants.RUN_MESSAGE_ID,
                                MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG,
                                0));
        receiver.acceptWithResponder(message, new RunResponseForwardToCallback(callback));
    }

    /** Sends the given run or close pipe message through the receiver. */
    public static void sendRunOrClosePipeMessage(
            Core core, MessageReceiverWithResponder receiver, RunOrClosePipeMessageParams params) {
        Message message =
                params.serializeWithHeader(
                        core,
                        new MessageHeader(
                                InterfaceControlMessagesConstants.RUN_OR_CLOSE_PIPE_MESSAGE_ID));
        receiver.accept(message);
    }

    /** Handles a received run message. */
    public static <I extends Interface, P extends Proxy> boolean handleRun(
            Core core, Manager<I, P> manager, ServiceMessage message, MessageReceiver responder) {
        Message payload = message.getPayload();
        RunMessageParams query = RunMessageParams.deserialize(payload);
        RunResponseMessageParams response = new RunResponseMessageParams();
        response.output = new RunOutput();
        if (query.input.which() == RunInput.Tag.QueryVersion) {
            response.output.setQueryVersionResult(new QueryVersionResult());
            response.output.getQueryVersionResult().version = manager.getVersion();
        } else {
            response.output = null;
        }

        return responder.accept(
                response.serializeWithHeader(
                        core,
                        new MessageHeader(
                                InterfaceControlMessagesConstants.RUN_MESSAGE_ID,
                                MessageHeader.MESSAGE_IS_RESPONSE_FLAG,
                                message.getHeader().getRequestId())));
    }

    /**
     * Handles a received run or close pipe message. Closing the pipe is handled by returning
     * |false|.
     */
    public static <I extends Interface, P extends Proxy> boolean handleRunOrClosePipe(
            Manager<I, P> manager, ServiceMessage message) {
        Message payload = message.getPayload();
        RunOrClosePipeMessageParams query = RunOrClosePipeMessageParams.deserialize(payload);
        if (query.input.which() == RunOrClosePipeInput.Tag.RequireVersion) {
            return query.input.getRequireVersion().version <= manager.getVersion();
        }
        return false;
    }
}
