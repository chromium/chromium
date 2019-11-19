// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.content.Context;
import android.support.test.InstrumentationRegistry;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import javax.annotation.concurrent.GuardedBy;

/**
 * Junit4 rule for starting embedded test server when necessary (i.e. when accessed via
 * {@link #getServer()}), and shutting it down when the test finishes.
 */
public class EmbeddedTestServerRule extends TestWatcher {
    private final Object mLock = new Object();
    @GuardedBy("mLock")
    private EmbeddedTestServer mServer;

    // The default value of 0 will result in the same behavior as createAndStartServer
    // (auto-selected port).
    @GuardedBy("mLock")
    private int mServerPort;

    @GuardedBy("mLock")
    private boolean mUseHttps;

    @GuardedBy("mLock")
    @ServerCertificate
    private int mCertificateType = ServerCertificate.CERT_OK;

    @Override
    protected void finished(Description description) {
        super.finished(description);
        synchronized (mLock) {
            if (mServer != null) {
                mServer.stopAndDestroyServer();
            }
        }
    }

    /**
     * Get the test server, creating and starting it if it doesn't exist yet.
     *
     * @return the test server.
     */
    public EmbeddedTestServer getServer() {
        synchronized (mLock) {
            if (mServer == null) {
                Context context = InstrumentationRegistry.getContext();
                mServer = mUseHttps
                        ? EmbeddedTestServer.createAndStartHTTPSServerWithPort(
                                context, mCertificateType, mServerPort)
                        : EmbeddedTestServer.createAndStartServerWithPort(context, mServerPort);
            }
            return mServer;
        }
    }

    public String getOrigin() {
        return getServer().getURL("/");
    }

    /**
     * Sets the port that the server will be started with. Must be called before the first
     * {@link #getServer()} call.
     *
     * @param port the port to start the server with, or 0 for an automatically selected one.
     */
    public void setServerPort(int port) {
        synchronized (mLock) {
            assert mServer == null;
            mServerPort = port;
        }
    }

    /** Sets whether to create an HTTPS (vs HTTP) server. */
    public void setServerUsesHttps(boolean useHttps) {
        synchronized (mLock) {
            assert mServer == null;
            mUseHttps = useHttps;
        }
    }

    /** Sets what type of certificate the server uses when running as an HTTPS server. */
    public void setCertificateType(@ServerCertificate int certificateType) {
        synchronized (mLock) {
            assert mServer == null;
            mCertificateType = certificateType;
        }
    }
}
