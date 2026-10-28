// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;

/**
 * Base class for generated {@link MessageReceiver}s that receive the response to an interface
 * method call, and forward it to the caller's callback.
 */
@NullMarked
public abstract class ForwardToCallback implements MessageReceiver {
    /** The ordinal of the method the response is for. */
    private final int mMethodId;

    /** The expected flags of the response message. */
    private final int mFlags;

    protected ForwardToCallback(int methodId, int flags) {
        mMethodId = methodId;
        mFlags = flags;
    }

    /** Deserializes the response using the given decoder and forwards it to the callback. */
    protected abstract void forwardResponse(Decoder decoder);

    @Override
    public final boolean accept(Message message) throws BadMessageException {
        try {
            ServiceMessage messageWithHeader = message.asServiceMessage();
            MessageHeader header = messageWithHeader.getHeader();
            if (header.getMethodId() != mMethodId) {
                return false;
            }
            if (!header.hasExactFlags(mFlags)) {
                return false;
            }
            forwardResponse(new Decoder(messageWithHeader.getPayload()));
            return true;
        } catch (DeserializationException e) {
            throw new UnmarshalException(e);
        }
    }

    @Override
    public void close() {}
}
