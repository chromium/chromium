// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import android.os.ParcelFileDescriptor;

/**
 * Core mojo interface giving access to the base operations. See |src/mojo/public/c/system/core.h|
 * for the underlying api.
 */
public interface Core {

    /** Used to indicate an infinite deadline (timeout). */
    public static final long DEADLINE_INFINITE = -1;

    /** Signals for the wait operations on handles. */
    public static class HandleSignals extends Flags<HandleSignals> {
        /**
         * Constructor.
         *
         * @param signals the serialized signals.
         */
        public HandleSignals(int signals) {
            super(signals);
        }

        private static final int FLAG_NONE = 0;
        private static final int FLAG_READABLE = 1 << 0;
        private static final int FLAG_WRITABLE = 1 << 1;
        private static final int FLAG_PEER_CLOSED = 1 << 2;

        /** Immutable signals. */
        public static final HandleSignals NONE = HandleSignals.none().immutable();

        public static final HandleSignals READABLE =
                HandleSignals.none().setReadable(true).immutable();
        public static final HandleSignals WRITABLE =
                HandleSignals.none().setWritable(true).immutable();

        /**
         * Change the readable bit of this signal.
         *
         * @param readable the new value of the readable bit.
         * @return this.
         */
        public HandleSignals setReadable(boolean readable) {
            return setFlag(FLAG_READABLE, readable);
        }

        /**
         * Change the writable bit of this signal.
         *
         * @param writable the new value of the writable bit.
         * @return this.
         */
        public HandleSignals setWritable(boolean writable) {
            return setFlag(FLAG_WRITABLE, writable);
        }

        /**
         * Change the peer closed bit of this signal.
         *
         * @param peerClosed the new value of the peer closed bit.
         * @return this.
         */
        public HandleSignals setPeerClosed(boolean peerClosed) {
            return setFlag(FLAG_PEER_CLOSED, peerClosed);
        }

        /** Returns a signal with no bit set. */
        public static HandleSignals none() {
            return new HandleSignals(FLAG_NONE);
        }
    }

    /**
     * Returns a platform-dependent monotonically increasing tick count representing "right now."
     */
    public long getTimeTicksNow();

    /** Returned by wait functions to indicate the signaling state of handles. */
    public static class HandleSignalsState {
        /** Signals that were satisfied at some time // before the call returned. */
        private final HandleSignals mSatisfiedSignals;

        /**
         * Signals that are possible to satisfy. For example, if the return value was
         * |MOJO_RESULT_FAILED_PRECONDITION|, you can use this field to determine which, if any, of
         * the signals can still be satisfied.
         */
        private final HandleSignals mSatisfiableSignals;

        /** Constructor. */
        public HandleSignalsState(
                HandleSignals satisfiedSignals, HandleSignals satisfiableSignals) {
            mSatisfiedSignals = satisfiedSignals;
            mSatisfiableSignals = satisfiableSignals;
        }

        /** Returns the satisfiedSignals. */
        public HandleSignals getSatisfiedSignals() {
            return mSatisfiedSignals;
        }

        /** Returns the satisfiableSignals. */
        public HandleSignals getSatisfiableSignals() {
            return mSatisfiableSignals;
        }
    }

    /**
     * Creates a message pipe, which is a bidirectional communication channel for framed data (i.e.,
     * messages), with the given options. Messages can contain plain data and/or Mojo handles.
     *
     * @return the set of handles for the two endpoints (ports) of the message pipe.
     */
    public Pair<MessagePipeHandle, MessagePipeHandle> createMessagePipe(
            MessagePipeHandle.CreateOptions options);

    /**
     * Creates a data pipe, which is a unidirectional communication channel for unframed data, with
     * the given options. Data is unframed, but must come as (multiples of) discrete elements, of
     * the size given in |options|. See |DataPipe.CreateOptions| for a description of the different
     * options available for data pipes. |options| may be set to null for a data pipe with the
     * default options (which will have an element size of one byte and have some system-dependent
     * capacity).
     *
     * @return the set of handles for the two endpoints of the data pipe.
     */
    public Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> createDataPipe(
            DataPipe.CreateOptions options);

    /**
     * Creates a buffer that can be shared between applications (by duplicating the handle -- see
     * |SharedBufferHandle.duplicate()| -- and passing it over a message pipe). To access the
     * buffer, one must call |SharedBufferHandle.map|.
     *
     * @return the new |SharedBufferHandle|.
     */
    public SharedBufferHandle createSharedBuffer(
            SharedBufferHandle.CreateOptions options, long numBytes);

    /**
     * Acquires a handle from the native side. The handle will be owned by the returned object and
     * must not be closed outside of it.
     *
     * @return a new {@link UntypedHandle} representing the native handle.
     */
    public UntypedHandle acquireNativeHandle(long handle);

    /**
     * Creates and acquires a handle from the native side. The handle will be owned by the returned
     * object and must not be closed outside of it.
     *
     * @param fd Java file descriptor to be wrapped as a native platform handle.
     * @return a new {@link UntypedHandle} representing the native handle.
     */
    public UntypedHandle wrapFileDescriptor(ParcelFileDescriptor fd);

    /** Returns an implementation of {@link Watcher}. */
    public Watcher getWatcher();

    /** Returns a new run loop. */
    public RunLoop createDefaultRunLoop();

    /** Returns the current run loop if it exists. */
    public RunLoop getCurrentRunLoop();
}
