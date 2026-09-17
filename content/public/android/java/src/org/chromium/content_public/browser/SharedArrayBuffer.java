// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.os.SharedMemory;
import android.system.ErrnoException;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * A Java representation of a JavaScript SharedArrayBuffer, meant to be used with {@link
 * MessagePayload}.
 *
 * <p>This class encapsulates an Android {@link SharedMemory} instance to enable zero-copy data
 * exchange between native Java code and JavaScript in a WebView.
 */
@NullMarked
public final class SharedArrayBuffer implements AutoCloseable {

    private final SharedMemory mSharedMemory;
    private @Nullable ByteBuffer mByteBuffer;

    private SharedArrayBuffer(SharedMemory sharedMemory) {
        mSharedMemory = sharedMemory;
    }

    /**
     * Allocates a new {@link SharedArrayBuffer} of the given size.
     *
     * @param name The optional debug/identification name for the shared memory region. The linux
     *     kernel imposes a 255 byte limit but it prepends `memfd:`, so the name should be less than
     *     249 bytes.
     * @param size The size of the buffer in bytes.
     * @return A newly allocated {@link SharedArrayBuffer}.
     * @throws ErrnoException If the system fails to create the shared memory region.
     */
    public static SharedArrayBuffer allocate(@Nullable String name, int size)
            throws ErrnoException {

        SharedMemory memory = SharedMemory.create(name, size);
        return new SharedArrayBuffer(memory);
    }

    /**
     * Gets a copy of a {@link ByteBuffer} mapped to the shared memory region. If the buffer has
     * already been mapped, the existing buffer is returned with a new duplicate.
     *
     * <p>The returned {@link ByteBuffer} is configured with {@link ByteOrder#nativeOrder()} to
     * ensure consistency with JavaScript TypedArray views and native code.
     *
     * <p><b>Note:</b> Any returned buffer becomes invalid once {@link #close()} is called.
     * Accessing it after destruction will cause a segmentation fault (SIGSEGV).
     *
     * @return A read/write direct {@link ByteBuffer} mapped to the shared memory region.
     * @throws ErrnoException If the system fails to map the shared memory region.
     */
    public ByteBuffer mapAndCreateByteBuffer() throws ErrnoException {
        if (mByteBuffer == null) {
            mByteBuffer = mSharedMemory.mapReadWrite().order(ByteOrder.nativeOrder());
        }
        return mByteBuffer.duplicate().order(ByteOrder.nativeOrder());
    }

    public long getSize() {
        return mSharedMemory.getSize();
    }

    @CalledByNative
    private SharedMemory getSharedMemory() {
        return mSharedMemory;
    }

    /**
     * Closes the {@link SharedArrayBuffer} and releases the underlying {@link SharedMemory}
     * instance.
     *
     * <p>This method unmaps any previously mapped {@link ByteBuffer} instances and closes the
     * shared memory region.
     */
    @Override
    public void close() {
        if (mByteBuffer != null) {
            SharedMemory.unmap(mByteBuffer);
            mByteBuffer = null;
        }
        mSharedMemory.close();
    }
}
