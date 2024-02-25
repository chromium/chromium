// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_base;

import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.SharedBufferHandle;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.mojo_base.mojom.BigBuffer;
import org.chromium.mojo_base.mojom.BigBufferSharedMemoryRegion;

import java.nio.ByteBuffer;

/** Static helper methods for working with the mojom BigBuffer type. */
public final class BigBufferUtil {
    public static final int MAX_INLINE_ARRAY_SIZE = 64 * 1024;

    /**
     * A mapping to a BigBuffer.
     *
     * If it is backed by shared memory, this will be a direct mapping which must be closed and will
     * be invalid thereafter. The simplest way to do this is by using try-with-resources.
     */
    public static class Mapping implements AutoCloseable {
        private final SharedBufferHandle mHandle;
        private final ByteBuffer mBuffer;

        Mapping(SharedBufferHandle handle, ByteBuffer buffer) {
            mHandle = handle;
            mBuffer = buffer;
        }

        public ByteBuffer getBuffer() {
            return mBuffer;
        }

        @Override
        public void close() {
            if (mHandle != null) {
                mHandle.unmap(mBuffer);
            }
        }
    }

    // Retrives a copy of the buffer's contents regardless of what type was backing it (i.e. array
    // or shared memory).
    public static byte[] getBytesFromBigBuffer(BigBuffer buffer) {
        if (buffer.which() == BigBuffer.Tag.Bytes) {
            return buffer.getBytes();
        } else {
            BigBufferSharedMemoryRegion region = buffer.getSharedMemory();
            ByteBuffer byteBuffer =
                    region.bufferHandle.map(0, region.size, SharedBufferHandle.MapFlags.NONE);
            byte[] bytes = new byte[region.size];
            byteBuffer.get(bytes);
            region.bufferHandle.unmap(byteBuffer);
            return bytes;
        }
    }

    // Opens a mapping to an existing buffer for direct reading, without a copy.
    // This must be used with a try-with-resources so that close is called to prevent a leak.
    // The direct buffer must not be used after close is called.
    public static Mapping map(BigBuffer buffer) {
        if (buffer.which() == BigBuffer.Tag.Bytes) {
            return new Mapping(null, ByteBuffer.wrap(buffer.getBytes()));
        } else {
            BigBufferSharedMemoryRegion region = buffer.getSharedMemory();
            ByteBuffer byteBuffer =
                    region.bufferHandle.map(0, region.size, SharedBufferHandle.MapFlags.NONE);
            return new Mapping(region.bufferHandle, byteBuffer);
        }
    }

    // Creates a new mojom.BigBuffer for IPC from a set of bytes. If the byte array is larger than
    // MAX_INLINE_ARRAY_SIZE, shared memory will be used instead of an inline array.
    public static BigBuffer createBigBufferFromBytes(byte[] bytes) {
        BigBuffer buffer = new BigBuffer();
        if (bytes.length <= MAX_INLINE_ARRAY_SIZE) {
            buffer.setBytes(bytes);
            return buffer;
        }
        Core core = CoreImpl.getInstance();
        BigBufferSharedMemoryRegion region = new BigBufferSharedMemoryRegion();
        region.bufferHandle =
                core.createSharedBuffer(new SharedBufferHandle.CreateOptions(), bytes.length);
        region.size = bytes.length;
        ByteBuffer mappedRegion =
                region.bufferHandle.map(0, bytes.length, SharedBufferHandle.MapFlags.NONE);
        mappedRegion.put(bytes);
        region.bufferHandle.unmap(mappedRegion);
        buffer.setSharedMemory(region);
        return buffer;
    }
}
