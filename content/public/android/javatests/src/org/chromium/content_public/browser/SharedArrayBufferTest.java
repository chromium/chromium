// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.system.ErrnoException;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/** Tests for {@link SharedArrayBuffer}. */
@Batch(Batch.PER_CLASS)
@RunWith(ContentJUnit4ClassRunner.class)
@SmallTest
public class SharedArrayBufferTest {

    @Test
    public void testAllocateSuccess() throws ErrnoException {
        int size = 1024;
        try (SharedArrayBuffer buffer = SharedArrayBuffer.allocate("", size)) {
            Assert.assertNotNull(
                    "SharedArrayBuffer should not be null on successful allocation", buffer);
            Assert.assertEquals(
                    "Buffer size should match the allocated size", size, buffer.getSize());
        }
    }

    @Test
    public void testAllocateInvalidSizes() {
        Assert.assertThrows(
                IllegalArgumentException.class, () -> SharedArrayBuffer.allocate("", 0));

        Assert.assertThrows(
                IllegalArgumentException.class, () -> SharedArrayBuffer.allocate("", -512));
    }

    @Test
    public void testZeroInitialization() throws ErrnoException {
        int size = 100;
        try (SharedArrayBuffer buffer = SharedArrayBuffer.allocate("", size)) {
            ByteBuffer byteBuffer = buffer.mapAndCreateByteBuffer();

            for (int i = 0; i < size; i++) {
                Assert.assertEquals(
                        "Freshly allocated buffer should be strictly zero-initialized",
                        0,
                        byteBuffer.get(i));
            }
        }
    }

    @Test
    public void testMapAndCreateByteBuffer() throws ErrnoException {
        int size = 512;
        try (SharedArrayBuffer buffer = SharedArrayBuffer.allocate("", size)) {
            ByteBuffer byteBuffer = buffer.mapAndCreateByteBuffer();

            Assert.assertNotNull("Mapped ByteBuffer should not be null", byteBuffer);
            Assert.assertTrue("Mapped ByteBuffer should be direct", byteBuffer.isDirect());
            Assert.assertEquals(
                    "ByteBuffer capacity must match allocated size", size, byteBuffer.capacity());
            Assert.assertEquals(
                    "Byte order must be the native byte order",
                    ByteOrder.nativeOrder(),
                    byteBuffer.order());

            byteBuffer.put(0, (byte) 42);
            Assert.assertEquals(
                    "Should read back the value written to the mapped buffer",
                    (byte) 42,
                    byteBuffer.get(0));
        }
    }

    @Test
    public void testBoundarySizeAllocation() throws ErrnoException {
        try (SharedArrayBuffer buffer = SharedArrayBuffer.allocate("", 1)) {
            ByteBuffer byteBuffer = buffer.mapAndCreateByteBuffer();

            Assert.assertEquals(
                    "Buffer capacity should match the exact requested size (1)",
                    1,
                    byteBuffer.capacity());

            byteBuffer.put(0, (byte) 255);
            Assert.assertEquals((byte) 255, byteBuffer.get(0));

            Assert.assertThrows(
                    IndexOutOfBoundsException.class, () -> byteBuffer.put(1, (byte) 10));
        }
    }

    @Test
    public void testMapAndCreateByteBufferReturnsDuplicateWithIndependentCursor()
            throws ErrnoException {
        try (SharedArrayBuffer buffer = SharedArrayBuffer.allocate("", 128)) {
            ByteBuffer first = buffer.mapAndCreateByteBuffer();
            ByteBuffer second = buffer.mapAndCreateByteBuffer();

            Assert.assertNotNull(first);
            Assert.assertNotNull(second);
            Assert.assertNotSame(
                    "mapAndCreateByteBuffer() called twice should return distinct ByteBuffer"
                            + " instances",
                    first,
                    second);

            first.putInt(0, 1337);
            Assert.assertEquals(
                    "Second mapped buffer should see updates from first mapped buffer",
                    1337,
                    second.getInt(0));

            second.putInt(4, 999);
            Assert.assertEquals(
                    "First mapped buffer should see updates from second mapped buffer",
                    999,
                    first.getInt(4));

            first.position(10);
            Assert.assertEquals(
                    "Modifying position in one buffer should not affect the duplicate",
                    0,
                    second.position());
        }
    }

    @Test
    public void testCloseUnmapsMemoryImmediately() throws ErrnoException, IOException {
        String uniqueTag = java.util.UUID.randomUUID().toString();
        SharedArrayBuffer buffer = SharedArrayBuffer.allocate(uniqueTag, 128);
        buffer.mapAndCreateByteBuffer();
        Assert.assertTrue(
                "SharedArrayBuffer should be mapped after mapAndCreateByteBuffer()",
                isMemoryMapped(uniqueTag));

        buffer.close();
        Assert.assertFalse(
                "SharedArrayBuffer mapping should be unmapped immediately on close()",
                isMemoryMapped(uniqueTag));
    }

    @Test
    public void testCloseIsIdempotent() throws ErrnoException {
        SharedArrayBuffer buffer = SharedArrayBuffer.allocate("", 128);
        buffer.mapAndCreateByteBuffer();
        // Closing multiple times should be safe and not throw
        buffer.close();
        buffer.close();
    }

    @Test
    public void testCloseWithoutMapping() throws ErrnoException, IOException {
        String uniqueTag = java.util.UUID.randomUUID().toString();
        SharedArrayBuffer buffer = SharedArrayBuffer.allocate(uniqueTag, 128);
        Assert.assertFalse(
                "SharedArrayBuffer should not be mapped before mapAndCreateByteBuffer()",
                isMemoryMapped(uniqueTag));
        // Closing a buffer that was never mapped should succeed safely
        buffer.close();
        Assert.assertFalse(isMemoryMapped(uniqueTag));
    }

    /** Checks if a memory mapping containing the given tag exists in /proc/self/maps. */
    private boolean isMemoryMapped(String mappingTag) throws IOException {
        try (BufferedReader reader = new BufferedReader(new FileReader("/proc/self/maps"))) {
            String line;
            while ((line = reader.readLine()) != null) {
                if (line.contains(mappingTag)) {
                    return true;
                }
            }
        }
        return false;
    }
}
