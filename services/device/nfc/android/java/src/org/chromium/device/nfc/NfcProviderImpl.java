// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import org.chromium.device.mojom.Nfc;
import org.chromium.device.mojom.NfcProvider;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.MojoException;
import org.chromium.services.service_manager.InterfaceFactory;

/** Android implementation of the NfcProvider Mojo interface. */
public class NfcProviderImpl implements NfcProvider {
    private static final String TAG = "NfcProviderImpl";
    private NfcDelegate mDelegate;
    private NfcImpl mNfcImpl;

    public NfcProviderImpl(NfcDelegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void close() {
        // The connection to this object is owned by the browser process, but connections to the
        // NfcImpl are passed directly to a render process. If the connection is closed by the
        // browser process, also close the connection to the render process as this indicates that
        // the render process should no longer have access to the NFC feature.
        if (mNfcImpl != null) {
            mNfcImpl.closeMojoConnection();
            mNfcImpl = null;
        }
    }

    @Override
    public void onConnectionError(MojoException e) {
        // We do nothing here since close() is always called no matter the connection gets closed
        // normally or abnormally.
    }

    @Override
    public void getNfcForHost(int hostId, InterfaceRequest<Nfc> request) {
        // Blink's NfcProxy class makes a single request for the NFC interface per document.
        // If a new request is received, close the old connection. This can happen on navigation
        // when the RenderFrameHost is not swapped out.
        if (mNfcImpl != null) {
            mNfcImpl.closeMojoConnection();
        }
        mNfcImpl = new NfcImpl(hostId, mDelegate, request);
    }

    /** Suspends the NFC usage. Should be called when web page visibility is lost. */
    @Override
    public void suspendNfcOperations() {
        if (mNfcImpl != null) {
            mNfcImpl.suspendNfcOperations();
        }
    }

    /** Resumes the NFC usage. Should be called when web page becomes visible. */
    @Override
    public void resumeNfcOperations() {
        if (mNfcImpl != null) {
            mNfcImpl.resumeNfcOperations();
        }
    }

    /** A factory for implementations of the NfcProvider interface. */
    public static class Factory implements InterfaceFactory<NfcProvider> {
        private NfcDelegate mDelegate;

        public Factory(NfcDelegate delegate) {
            mDelegate = delegate;
        }

        @Override
        public NfcProvider createImpl() {
            return new NfcProviderImpl(mDelegate);
        }
    }
}
