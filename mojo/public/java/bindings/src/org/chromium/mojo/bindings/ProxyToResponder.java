// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.mojo.system.Core;

/**
 * Base class for the generated callbacks passed to interface implementations, which serialize and
 * send the response to an interface method call.
 */
@NullMarked
public abstract class ProxyToResponder {
    private final Core mCore;
    private final MessageReceiver mMessageReceiver;
    private final long mRequestId;

    protected ProxyToResponder(Core core, MessageReceiver messageReceiver, long requestId) {
        mCore = core;
        mMessageReceiver = messageReceiver;
        mRequestId = requestId;
    }

    /** Serializes and sends the given response. */
    protected final void sendResponse(Struct response, int methodId, int flags) {
        try {
            mMessageReceiver.accept(
                    response.serializeWithHeader(
                            mCore,
                            new MessageHeader(
                                    MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID,
                                    methodId,
                                    flags,
                                    mRequestId)));
        } catch (BadMessageException e) {
            // Should not be possible, because we serialized above.
            throw new IllegalStateException(e);
        }
    }
}
