// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.blink.mojom.MessagePortDescriptor;
import org.chromium.mojo.bindings.ConnectionErrorHandler;
import org.chromium.mojo.bindings.Connector;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.mojo_base.mojom.UnguessableToken;

/**
 * Java wrapper around a native blink::MessagePortDescriptor, and mojom serialized versions of that
 * class (org.chromium.blink.mojom.MessagePortDescriptor).
 *
 * This object is pure implementation detail for AppWebMessagePort. Care must be taken to use this
 * object with something approaching "move" semantics, as under the hood it wraps a Mojo endpoint,
 * which itself has move semantics.
 *
 * The expected usage is as follows:
 *
 *   // Create a pair of descriptors, or rehydrate one from a mojo serialized version.
 *   Pair<AppWebMessagePortDescriptor, AppWebMessagePortDescriptor> pipe =
 *           AppWebMessagePortDescriptor.createPair();
 *   AppWebMessagePortDescriptor port = new AppWebMessagePortDescriptor(
 *           mojomSerializedMessagePortDescriptor);
 *
 *   // Entangle with a connector so the port can be used.
 *   Connector connector = port.entangleWithConnector();
 *
 *   // Do stuff with the connector, passing messages, etc.
 *
 *   // Takes the handle back from the connector that was vended out previously.
 *   port.disentangleFromConnector();
 *
 *   // Close the port when we're done with it. This causes the native
 *   // counterpart to be destroyed.
 *   port.close();
 */
public class AppWebMessagePortDescriptor implements ConnectionErrorHandler {
    private static final long NATIVE_NULL = 0;
    private static final int INVALID_NATIVE_MOJO_HANDLE = 0; // Mirrors CoreImpl.INVALID_HANDLE.
    private static final long INVALID_SEQUENCE_NUMBER = 0;

    /** Handle to the native blink::MessagePortDescriptor associated with this object. */
    private long mNativeMessagePortDescriptor = NATIVE_NULL;
    /**
     * The connector to which the handle has been entangled. This is null if the
     * descriptor owns the handle, otherwise it is set to the Connector.
     */
    private Connector mConnector;
    private boolean mConnectorErrored;

    /**
     * @return a newly created pair of connected AppWebMessagePortDescriptors.
     */
    static Pair<AppWebMessagePortDescriptor, AppWebMessagePortDescriptor> createPair() {
        long[] nativeMessagePortDescriptors = AppWebMessagePortDescriptorJni.get().createPair();
        assert nativeMessagePortDescriptors.length
                == 2 : "native createPair returned an invalid value";
        Pair<AppWebMessagePortDescriptor, AppWebMessagePortDescriptor> pair =
                new Pair<>(new AppWebMessagePortDescriptor(nativeMessagePortDescriptors[0]),
                        new AppWebMessagePortDescriptor(nativeMessagePortDescriptors[1]));
        return pair;
    }

    /**
     * Creates an AppWebMessagePortDescriptor from a pointer to a native object.
     */
    static AppWebMessagePortDescriptor createFromNativeBlinkMessagePortDescriptor(
            long nativeMessagePortDescriptor) {
        return new AppWebMessagePortDescriptor(nativeMessagePortDescriptor);
    }

    /**
     * Creates an instance of a descriptor from a mojo-serialized version.
     */
    AppWebMessagePortDescriptor(MessagePortDescriptor blinkMessagePortDescriptor) {
        // If the descriptor is invalid then immediately go to an invalid state, which also means
        // we're safe to GC.
        if (!isBlinkMessagePortDescriptorValid(blinkMessagePortDescriptor)) {
            reset();
            return;
        }
        mNativeMessagePortDescriptor = AppWebMessagePortDescriptorJni.get().create(
                blinkMessagePortDescriptor.pipeHandle.releaseNativeHandle(),
                blinkMessagePortDescriptor.id.low, blinkMessagePortDescriptor.id.high,
                blinkMessagePortDescriptor.sequenceNumber);
        resetBlinkMessagePortDescriptor(blinkMessagePortDescriptor);
    }

    /**
     * @return true if this descriptor is valid (has an active native counterpart). Safe to call
     *         anytime.
     */
    boolean isValid() {
        return mNativeMessagePortDescriptor != NATIVE_NULL;
    }

    /**
     * @return the Connector corresponding to this descriptors mojo endpoint. This must be
     *         returned to the descriptor before it is closed. It is valid to take and return
     *         the handle multiple times. This is only meant to be called by AppWebMessagePort.
     */
    Connector entangleWithConnector() {
        ensureNativeMessagePortDescriptorExists();
        assert mConnector == null : "handle already taken";
        int nativeHandle = AppWebMessagePortDescriptorJni.get().takeHandleToEntangle(
                mNativeMessagePortDescriptor);
        assert nativeHandle
                != INVALID_NATIVE_MOJO_HANDLE : "native object returned an invalid handle";
        MessagePipeHandle handle = wrapNativeHandle(nativeHandle);
        mConnector = new Connector(handle);
        mConnector.setErrorHandler(this);
        return mConnector;
    }

    /**
     * @return true if the port is currently entangled, false otherwise. Safe to call anytime.
     */
    boolean isEntangled() {
        return mNativeMessagePortDescriptor != NATIVE_NULL && mConnector != null;
    }

    /**
     * Returns the mojo endpoint to this descriptor, previously vended out by
     * "entangleWithConnector". If vended out the connector must be returned prior to closing this
     * object. It is valid to entangle and disentangle the descriptor multiple times.
     */
    void disentangleFromConnector() {
        ensureNativeMessagePortDescriptorExists();
        assert mConnector != null : "handle not currently taken";
        disentangleImpl();
    }

    /**
     * Releases the native blink::MessagePortDescriptor object associated with
     * this AppWebMessagePort.
     */
    long releaseNativeMessagePortDescriptor() {
        assert mConnector == null : "releasing a descriptor whose handle is taken";
        long nativeMessagePortDescriptor = mNativeMessagePortDescriptor;
        reset();
        return nativeMessagePortDescriptor;
    }

    /**
     * Closes this descriptor. This will cause the native counterpart to be destroyed, and the
     * descriptor will no longer be valid after this point. It is safe to call this repeatedly. It
     * is illegal to close a descriptor whose handle is presently taken by a corresponding
     * AppWebMessagePort.
     */
    void close() {
        if (mNativeMessagePortDescriptor == NATIVE_NULL) return;
        assert mConnector == null : "closing a descriptor whose handle is taken";
        AppWebMessagePortDescriptorJni.get().closeAndDestroy(mNativeMessagePortDescriptor);
        reset();
    }

    /**
     * Passes ownership of this descriptor into a blink.mojom.MessagePortDescriptor. This allows the
     * port to be transferred as part of a TransferableMessage. Invalidates this object in the
     * process.
     */
    MessagePortDescriptor passAsBlinkMojomMessagePortDescriptor() {
        ensureNativeMessagePortDescriptorExists();
        assert mConnector == null : "passing a descriptor whose handle is taken";

        // This is in the format [nativeHandle, idLow, idHigh, sequenceNumber].
        long[] serialized =
                AppWebMessagePortDescriptorJni.get().passSerialized(mNativeMessagePortDescriptor);
        assert serialized.length == 4 : "native passSerialized returned an invalid value";

        int nativeHandle = (int) serialized[0];
        long idLow = serialized[1];
        long idHigh = serialized[2];
        long sequenceNumber = serialized[3];

        MessagePortDescriptor port = new MessagePortDescriptor();
        port.pipeHandle = wrapNativeHandle(nativeHandle);
        port.id = new UnguessableToken();
        port.id.low = idLow;
        port.id.high = idHigh;
        port.sequenceNumber = sequenceNumber;

        reset();

        return port;
    }

    /**
     * Private constructor used in creating a matching pair of descriptors.
     */
    private AppWebMessagePortDescriptor(long nativeMessagePortDescriptor) {
        assert nativeMessagePortDescriptor != NATIVE_NULL : "invalid nativeMessagePortDescriptor";
        mNativeMessagePortDescriptor = nativeMessagePortDescriptor;
    }

    /**
     * Helper function that throws an exception if the native counterpart does not exist.
     */
    private void ensureNativeMessagePortDescriptorExists() {
        assert mNativeMessagePortDescriptor != NATIVE_NULL : "native descriptor does not exist";
    }

    /**
     * Resets this object. Assumes the native side has already been appropriately torn down.
     */
    private void reset() {
        mNativeMessagePortDescriptor = NATIVE_NULL;
        mConnector = null;
        mConnectorErrored = false;
    }

    /**
     * @return true if the provided blink.mojom.MessagePortDescriptor is valid.
     */
    private static boolean isBlinkMessagePortDescriptorValid(
            MessagePortDescriptor blinkMessagePortDescriptor) {
        if (!blinkMessagePortDescriptor.pipeHandle.isValid()) {
            return false;
        }
        if (blinkMessagePortDescriptor.id == null) {
            return false;
        }
        if (blinkMessagePortDescriptor.id.low == 0 && blinkMessagePortDescriptor.id.high == 0) {
            return false;
        }
        if (blinkMessagePortDescriptor.sequenceNumber == INVALID_SEQUENCE_NUMBER) {
            return false;
        }
        return true;
    }

    /**
     * Resets the provided blink.mojom.MessagePortDescriptor.
     */
    private static void resetBlinkMessagePortDescriptor(
            MessagePortDescriptor blinkMessagePortDescriptor) {
        blinkMessagePortDescriptor.pipeHandle.close();
        if (blinkMessagePortDescriptor.id != null) {
            blinkMessagePortDescriptor.id.low = 0;
            blinkMessagePortDescriptor.id.high = 0;
        }
        blinkMessagePortDescriptor.sequenceNumber = 0;
    }

    /**
     * Wraps the provided native handle as MessagePipeHandle.
     */
    MessagePipeHandle wrapNativeHandle(int nativeHandle) {
        return CoreImpl.getInstance().acquireNativeHandle(nativeHandle).toMessagePipeHandle();
    }

    /**
     * Returns the mojo.Core object.
     */
    Core getCore() {
        return CoreImpl.getInstance();
    }

    /**
     * The error handler for the connector. This lets the instrumentation be aware of the message
     * pipe closing itself due to error, which happens unconditionally. Note that a subsequent call
     * to Connector#passHandle should always return an invalid handle at that point.
     * TODO(chrisha): Make this an immediate notification that the channel has been torn down
     * rather than waiting for the owning MessagePort to be cleaned up.
     *
     * @see org.chromium.mojo.bindings.ConnectionErrorHandler#onConnectionError.
     * @see org.chromium.mojo.bindings.Connector#setErrorHandler.
     */
    @Override
    public void onConnectionError(MojoException e) {
        mConnectorErrored = true;
    }

    private void disentangleImpl() {
        MessagePipeHandle handle = mConnector.passHandle();
        int nativeHandle = handle.releaseNativeHandle();
        // An invalid handle should be returned iff the connection errored.
        if (mConnectorErrored) {
            assert nativeHandle
                    == INVALID_NATIVE_MOJO_HANDLE : "errored connector returned a valid handle";
            AppWebMessagePortDescriptorJni.get().onConnectionError(mNativeMessagePortDescriptor);
        } else {
            assert nativeHandle
                    != INVALID_NATIVE_MOJO_HANDLE : "connector returned an invalid handle";
            AppWebMessagePortDescriptorJni.get().giveDisentangledHandle(
                    mNativeMessagePortDescriptor, nativeHandle);
        }
        mConnector = null;
    }

    /**
     * A finalizer is required to ensure that the native object associated with
     * this descriptor gets torn down, otherwise there would be a memory leak.
     *
     * This is safe because it makes a simple call into C++ code that is both
     * thread-safe and very fast.
     *
     * TODO(chrisha): Chase down the existing offenders that don't call close,
     * and flip this to use LifetimeAssert.
     *
     * @see java.lang.Object#finalize()
     */
    @Override
    protected final void finalize() throws Throwable {
        try {
            if (mNativeMessagePortDescriptor != NATIVE_NULL) {
                AppWebMessagePortDescriptorJni.get().disentangleCloseAndDestroy(
                        mNativeMessagePortDescriptor);
            }
        } finally {
            super.finalize();
        }
    }

    @NativeMethods
    interface Native {
        long[] createPair();
        long create(int nativeHandle, long idLow, long idHigh, long sequenceNumber);
        // Deliberately do not use nativeObjectType naming to avoid automatic typecasting and
        // member function dispatch; these need to be routed to static functions.

        // Takes the handle from the native object for entangling with a mojo.Connector.
        int takeHandleToEntangle(long blinkMessagePortDescriptor);
        // Returns the handle previously taken via "takeHandleToEntangle".
        void giveDisentangledHandle(long blinkMessagePortDescriptor, int nativeHandle);
        // Indicates that the entangled error experienced a connection error. The descriptor must be
        // entangled at this point.
        void onConnectionError(long blinkMessagePortDescriptor);
        // Frees the native object, returning its full state in serialized form to Java. The
        // descriptor must not be entangled.
        long[] passSerialized(long blinkMessagePortDescriptor);
        // Closes the open handle, and frees the native object. The descriptor must not be
        // entangled.
        void closeAndDestroy(long blinkMessagePortDescriptor);
        // Fully tears down the native object, disentangling if necessary, closing the handle and
        // freeing the object.
        void disentangleCloseAndDestroy(long blinkMessagePortDescriptor);
    }
}
