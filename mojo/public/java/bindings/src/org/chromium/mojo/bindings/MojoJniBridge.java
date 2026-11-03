// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.mojo.bindings.jni.MojoJniTransport;

import java.nio.ByteBuffer;

/**
 * Base class for generated Mojom JNI bridges that call a native {@code mojo::MojoJniBridge}
 * synchronously through {@link MojoJniTransport}.
 */
@NullMarked
public abstract class MojoJniBridge {
    protected final long mNativeBridge;

    protected MojoJniBridge(long nativeBridge) {
        assert nativeBridge != 0;
        mNativeBridge = nativeBridge;
    }

    public long getNativePointer() {
        return mNativeBridge;
    }

    protected Message invoke(int methodId) {
        return invoke(EmptyStruct.INSTANCE, methodId);
    }

    protected Message invoke(Struct params, int methodId) {
        ServiceMessage message =
                params.serializeWithHeader(
                        null,
                        new MessageHeader(
                                MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID,
                                methodId,
                                MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG,
                                0));
        byte[] response = MojoJniTransport.invoke(mNativeBridge, message.getData());
        assert response != null;
        return new Message(ByteBuffer.wrap(response)).asServiceMessage().getPayload();
    }
}
