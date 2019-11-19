// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Handler;
import android.os.Looper;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.blink.mojom.CloneableMessage;
import org.chromium.blink.mojom.NativeFileSystemTransferToken;
import org.chromium.blink.mojom.SerializedArrayBufferContents;
import org.chromium.blink.mojom.SerializedBlob;
import org.chromium.blink.mojom.TransferableMessage;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.mojo.bindings.Connector;
import org.chromium.mojo.bindings.DeserializationException;
import org.chromium.mojo.bindings.Message;
import org.chromium.mojo.bindings.MessageHeader;
import org.chromium.mojo.bindings.MessageReceiver;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.mojo_base.BigBufferUtil;
import org.chromium.skia.mojom.Bitmap;

/**
 * Represents the MessageChannel MessagePort object. Inspired from
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/web-messaging.html#message-channels
 *
 * State management:
 *
 * A message port can be in transferred state while a transfer is pending or complete. An
 * application cannot use a transferred port to post messages. If a transferred port
 * receives messages, they will be queued. This state is not visible to embedder app.
 *
 * A message port should be closed by the app when it is not needed any more. This will free
 * any resources used by it. A closed port cannot receive/send messages and cannot be transferred.
 * close() can be called multiple times. A transferred port cannot be closed by the application,
 * since the ownership is also transferred during the transfer. Closing a transferred port will
 * throw an exception.
 *
 * All methods are called on the UI thread, except for MessageHandler.handleMessage, which is
 * used to dispatch messages on a potentially separate thread.
 *
 * Restrictions:
 * The HTML5 message protocol is very flexible in transferring ports. However, this
 * sometimes leads to surprising behavior. For example, in current version of chrome (m41)
 * the code below
 *  1.  var c1 = new MessageChannel();
 *  2.  var c2 = new MessageChannel();
 *  3.  c1.port2.onmessage = function(e) { console.log("1"); }
 *  4.  c2.port2.onmessage = function(e) {
 *  5.     e.ports[0].onmessage = function(f) {
 *  6.          console.log("3");
 *  7.      }
 *  8.  }
 *  9.  c1.port1.postMessage("test");
 *  10. c2.port1.postMessage("test2",[c1.port2])
 *
 * prints 1 or 3 depending on whether or not line 10 is included in code. Further if
 * it gets executed with a timeout, depending on timeout value, the printout value
 * changes.
 *
 * To prevent such problems, this implementation limits the transfer of ports
 * as below:
 * A port is put to a "started" state if:
 * 1. The port is ever used to post a message, or
 * 2. The port was ever registered a handler to receive a message.
 * A started port cannot be transferred.
 *
 * This restriction should not impact postmessage functionality in a big way,
 * because an app can still create as many channels as it wants to and use it for
 * transferring data. As a return, it simplifies implementation and prevents hard
 * to debug, racy corner cases while receiving/sending data.
 */
@JNINamespace("content")
public class AppWebMessagePort implements MessagePort {
    private static final String TAG = "AppWebMessagePort";

    private static final MessageHeader MESSAGE_HEADER = new MessageHeader(0);

    // Implements the handler to handle messageport messages received from web.
    // These messages are dispatched on the main thread by |mConnector|. Applications
    // can pass a handler to setMessageCallback to have messages dispatched on a different
    // thread.
    private static class MessageHandler extends Handler implements MessageReceiver {
        // The |what| value for handleMessage.
        private static final int MESSAGE_RECEIVED = 1;

        private final MessageCallback mMessageCallback;

        // Type for the |obj| value for handleMessage.
        private static class MessagePortMessage {
            public byte[] encodedMessage;
            public AppWebMessagePort[] ports;
        }

        public MessageHandler(Looper looper, MessageCallback callback) {
            super(looper);
            mMessageCallback = callback;
        }

        @Override
        public void handleMessage(android.os.Message msg) {
            if (msg.what == MESSAGE_RECEIVED) {
                MessagePortMessage message = (MessagePortMessage) msg.obj;
                String decodedMessage =
                        AppWebMessagePortJni.get().decodeStringMessage(message.encodedMessage);
                if (decodedMessage == null) {
                    Log.w(TAG, "Undecodable message received, dropping message");
                    return;
                }
                mMessageCallback.onMessage(decodedMessage, message.ports);
                return;
            }
            throw new IllegalStateException("undefined message");
        }

        @Override
        public boolean accept(Message mojoMessage) {
            try {
                TransferableMessage msg = TransferableMessage.deserialize(
                        mojoMessage.asServiceMessage().getPayload());
                AppWebMessagePort[] ports = new AppWebMessagePort[msg.ports.length];
                for (int i = 0; i < ports.length; ++i) {
                    ports[i] = new AppWebMessagePort(msg.ports[i]);
                }
                MessagePortMessage portMsg = new MessagePortMessage();
                portMsg.encodedMessage =
                        BigBufferUtil.getBytesFromBigBuffer(msg.message.encodedMessage);
                portMsg.ports = ports;
                sendMessage(obtainMessage(MESSAGE_RECEIVED, portMsg));
            } catch (DeserializationException e) {
                Log.w(TAG, "Error deserializing message", e);
                return false;
            }
            return true;
        }

        @Override
        public void close() {}
    }

    private boolean mClosed;
    private boolean mTransferred;
    private boolean mStarted;
    private boolean mWatching;

    private Core mMojoCore;
    private Connector mConnector;

    private AppWebMessagePort(MessagePipeHandle messagePipeHandle) {
        mMojoCore = messagePipeHandle.getCore();
        mConnector = new Connector(messagePipeHandle);
    }

    // Called to create an entangled pair of ports.
    public static AppWebMessagePort[] createPair() {
        Pair<MessagePipeHandle, MessagePipeHandle> handles =
                CoreImpl.getInstance().createMessagePipe(new MessagePipeHandle.CreateOptions());
        AppWebMessagePort[] ports = new AppWebMessagePort[] {
                new AppWebMessagePort(handles.first), new AppWebMessagePort(handles.second)};
        return ports;
    }

    // Called to create a port from handle.
    public static AppWebMessagePort create(MessagePipeHandle handle) {
        return new AppWebMessagePort(handle);
    }

    private MessagePipeHandle passHandle() {
        mTransferred = true;
        MessagePipeHandle handle = mConnector.passHandle();
        mConnector = null;
        return handle;
    }

    @CalledByNative
    private int releaseNativeHandle() {
        return passHandle().releaseNativeHandle();
    }

    @Override
    public void close() {
        if (mTransferred) {
            throw new IllegalStateException("Port is already transferred");
        }
        if (mClosed) return;
        mClosed = true;
        mConnector.close();
        mConnector = null;
    }

    @Override
    public boolean isClosed() {
        return mClosed;
    }

    @Override
    public boolean isTransferred() {
        return mTransferred;
    }

    @Override
    public boolean isStarted() {
        return mStarted;
    }

    // Only called on UI thread
    @Override
    public void setMessageCallback(MessageCallback messageCallback, Handler handler) {
        if (isClosed() || isTransferred()) {
            throw new IllegalStateException("Port is already closed or transferred");
        }
        mStarted = true;
        if (messageCallback == null) {
            mConnector.setIncomingMessageReceiver(null);
        } else {
            mConnector.setIncomingMessageReceiver(new MessageHandler(
                    handler == null ? Looper.getMainLooper() : handler.getLooper(),
                    messageCallback));
        }
        if (!mWatching) {
            mConnector.start();
            mWatching = true;
        }
    }

    @Override
    public void postMessage(String message, MessagePort[] sentPorts) throws IllegalStateException {
        if (isClosed() || isTransferred()) {
            throw new IllegalStateException("Port is already closed or transferred");
        }
        MessagePipeHandle[] ports = new MessagePipeHandle[sentPorts == null ? 0 : sentPorts.length];
        if (sentPorts != null) {
            for (MessagePort port : sentPorts) {
                if (port.equals(this)) {
                    throw new IllegalStateException("Source port cannot be transferred");
                }
                if (port.isClosed() || port.isTransferred()) {
                    throw new IllegalStateException("Port is already closed or transferred");
                }
                if (port.isStarted()) {
                    throw new IllegalStateException("Port is already started");
                }
            }
            for (int i = 0; i < sentPorts.length; ++i) {
                ports[i] = ((AppWebMessagePort) sentPorts[i]).passHandle();
            }
        }
        mStarted = true;

        TransferableMessage msg = new TransferableMessage();
        msg.message = new CloneableMessage();
        msg.message.encodedMessage = BigBufferUtil.createBigBufferFromBytes(
                AppWebMessagePortJni.get().encodeStringMessage(message));
        msg.message.blobs = new SerializedBlob[0];
        msg.message.nativeFileSystemTokens = new NativeFileSystemTransferToken[0];
        msg.message.senderOrigin = null;
        msg.arrayBufferContentsArray = new SerializedArrayBufferContents[0];
        msg.imageBitmapContentsArray = new Bitmap[0];
        msg.ports = ports;
        msg.streamChannels = new MessagePipeHandle[0];
        mConnector.accept(msg.serializeWithHeader(mMojoCore, MESSAGE_HEADER));
    }

    @NativeMethods
    interface Natives {
        String decodeStringMessage(byte[] encodedData);
        byte[] encodeStringMessage(String message);
    }
}
