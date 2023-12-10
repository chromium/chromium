// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.content.Context;

import androidx.test.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * Junit4 rule for starting embedded test server when necessary (i.e. when accessed via
 * {@link #getServer()}), and shutting it down when the test finishes.
 */
public class EmbeddedTestServerRule implements TestRule {
    private EmbeddedTestServer mServer;

    // The default value of 0 will result in the same behavior as createAndStartServer
    // (auto-selected port).
    private int mServerPort;

    private boolean mUseHttps;

    @ServerCertificate private int mCertificateType = ServerCertificate.CERT_OK;

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    base.evaluate();
                } finally {
                    if (mServer != null) mServer.stopAndDestroyServer();
                }
            }
        };
    }

    /**
     * Get the test server, creating and starting it if it doesn't exist yet.
     *
     * @return the test server.
     */
    public EmbeddedTestServer getServer() {
        if (mServer == null) {
            Context context = InstrumentationRegistry.getContext();
            // Need to disable ResettersForTesting because it will destroy the server too early in
            // the case where this rule is initialized via @ClassRule and getServer() is not called
            // until one of the tests is executing.
            mServer = new EmbeddedTestServer();
            mServer.mDisableResetterForTesting = true;
            if (mUseHttps) {
                EmbeddedTestServer.initializeAndStartHTTPSServer(
                        mServer, context, mCertificateType, mServerPort);
            } else {
                EmbeddedTestServer.initializeAndStartServer(mServer, context, mServerPort);
            }
        }
        return mServer;
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
        assert mServer == null;
        mServerPort = port;
    }

    /** Sets whether to create an HTTPS (vs HTTP) server. */
    public void setServerUsesHttps(boolean useHttps) {
        assert mServer == null;
        mUseHttps = useHttps;
    }

    /** Sets what type of certificate the server uses when running as an HTTPS server. */
    public void setCertificateType(@ServerCertificate int certificateType) {
        assert mServer == null;
        mCertificateType = certificateType;
    }
}
