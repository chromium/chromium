// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings.jni;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.nio.ByteBuffer;

/**
 * The single JNI boundary used by all mojom-generated JNI bindings.
 *
 * <p>Calls carry a serialized Mojo {@code ServiceMessage} (including its {@code MessageHeader} and
 * payload), so generated code never declares JNI methods of its own.
 *
 * <p>The {@code bridge} handles are pointers to a native {@code mojo::internal::MojoJniBridgeBase}.
 * Misuse (null bridge, malformed messages, unknown ordinals) crashes rather than failing softly.
 */
@NullMarked
@JNINamespace("mojo")
public final class MojoJniTransport {
    private MojoJniTransport() {}

    /**
     * Invokes a method on a native bridge.
     *
     * @param bridge The native {@code mojo::internal::MojoJniBridgeBase} pointer.
     * @param message A direct {@link ByteBuffer} containing a serialized Mojo {@code
     *     ServiceMessage} (header + payload).
     * @return The serialized response {@code ServiceMessage}, or null if the method has no
     *     response.
     */
    public static byte @Nullable [] invoke(long bridge, ByteBuffer message) {
        assert bridge != 0;
        // JNI's GetDirectBufferAddress / GetDirectBufferCapacity ignore position() and limit(),
        // whereas slice() produces a buffer whose address and capacity match [position, limit).
        return MojoJniTransportJni.get().invoke(bridge, message.slice());
    }

    @NativeMethods
    interface Natives {
        byte @Nullable [] invoke(long bridge, ByteBuffer message);
    }
}
