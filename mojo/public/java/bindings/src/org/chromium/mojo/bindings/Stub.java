// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.mojo.system.Core;

/**
 * Base implementation of Stub. Stubs are message receivers that deserialize the payload and call
 * the appropriate method in the implementation. If the method returns `result`, the stub serializes
 * the response and sends it back.
 *
 * @param <I> the type of the interface to delegate calls to.
 */
@NullMarked
public abstract class Stub<I extends Interface> implements MessageReceiverWithResponder {

    /** The {@link Core} implementation to use. */
    private final Core mCore;

    /** The implementation to delegate calls to. */
    private final I mImpl;

    /** The interface id associated with this stub. */
    private final int mInterfaceId;

    /**
     * Constructor.
     *
     * @param core the {@link Core} implementation to use.
     * @param implementation which will receive requests.
     * @param interfaceId the id that is associated with this stub.
     */
    public Stub(Core core, I impl, int interfaceId) {
        mCore = core;
        mImpl = impl;
        mInterfaceId = interfaceId;
    }

    /** Returns the Core implementation. */
    protected Core getCore() {
        return mCore;
    }

    /** Returns the implementation to delegate calls to. */
    protected I getImpl() {
        return mImpl;
    }

    /** Gets the interface id. */
    public int getInterfaceId() {
        return mInterfaceId;
    }

    @Override
    public void close() {
        getImpl().close();
    }
}
