// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;

import androidx.annotation.GuardedBy;

import org.junit.Assert;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.net.X509Util;
import org.chromium.net.test.util.CertTestUtil;

import java.io.File;

/**
 * A simple file server for java tests.
 *
 * An example use:
 * <pre>
 * EmbeddedTestServer s = EmbeddedTestServer.createAndStartServer(context);
 *
 * // serve requests...
 * s.getURL("/foo/bar.txt");
 *
 * // Generally safe to omit as ResettersForTesting will call it.
 * s.stopAndDestroyServer();
 * </pre>
 *
 * Note that this runs net::test_server::EmbeddedTestServer in a service in a separate APK.
 */
public class EmbeddedTestServer {
    private static final String TAG = "TestServer";

    private static final String EMBEDDED_TEST_SERVER_SERVICE =
            "org.chromium.net.test.EMBEDDED_TEST_SERVER_SERVICE";
    private static final long SERVICE_CONNECTION_WAIT_INTERVAL_MS = 5000;

    private static boolean sTestRootInitDone;

    @GuardedBy("mImplMonitor")
    private IEmbeddedTestServerImpl mImpl;

    private ServiceConnection mConn =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName name, IBinder service) {
                    synchronized (mImplMonitor) {
                        mImpl = IEmbeddedTestServerImpl.Stub.asInterface(service);
                        mImplMonitor.notify();
                    }
                }

                @Override
                public void onServiceDisconnected(ComponentName name) {
                    synchronized (mImplMonitor) {
                        mImpl = null;
                        mImplMonitor.notify();
                    }
                }
            };

    private Context mContext;
    private final Object mImplMonitor = new Object();
    boolean mDisableResetterForTesting;

    // Whether the server should use HTTP or HTTPS.
    public enum ServerHTTPSSetting {
        USE_HTTP,
        USE_HTTPS,
    }

    /** Exception class raised on failure in the EmbeddedTestServer. */
    public static final class EmbeddedTestServerFailure extends Error {
        public EmbeddedTestServerFailure(String errorDesc) {
            super(errorDesc);
        }

        public EmbeddedTestServerFailure(String errorDesc, Throwable cause) {
            super(errorDesc, cause);
        }
    }

    /**
     * Connection listener class, to be notified of new connections and sockets reads.
     *
     * Notifications are asynchronous and delivered to the UI thread.
     */
    public static class ConnectionListener {
        private final IConnectionListener mListener =
                new IConnectionListener.Stub() {
                    @Override
                    public void acceptedSocket(final long socketId) {
                        ThreadUtils.runOnUiThread(
                                () -> {
                                    ConnectionListener.this.acceptedSocket(socketId);
                                });
                    }

                    @Override
                    public void readFromSocket(final long socketId) {
                        ThreadUtils.runOnUiThread(
                                () -> {
                                    ConnectionListener.this.readFromSocket(socketId);
                                });
                    }
                };

        /**
         * A new socket connection has been opened on the server.
         *
         * @param socketId Socket unique identifier. Unique as long as the socket stays open.
         */
        public void acceptedSocket(long socketId) {}

        /**
         * Data  has been read from a socket.
         *
         * @param socketId Socket unique identifier. Unique as long as the socket stays open.
         */
        public void readFromSocket(long socketId) {}

        private IConnectionListener getListener() {
            return mListener;
        }
    }

    /** Bind the service that will run the native server object.
     *
     *  @param context The context to use to bind the service. This will also be used to unbind
     *          the service at server destruction time.
     *  @param httpsSetting Whether the server should use HTTPS.
     */
    public void initializeNative(Context context, ServerHTTPSSetting httpsSetting) {
        mContext = context;

        Intent intent = new Intent(EMBEDDED_TEST_SERVER_SERVICE);
        setIntentClassName(intent);
        if (!mContext.bindService(intent, mConn, Context.BIND_AUTO_CREATE)) {
            throw new EmbeddedTestServerFailure(
                    "Unable to bind to the EmbeddedTestServer service.");
        }
        synchronized (mImplMonitor) {
            Log.i(TAG, "Waiting for EmbeddedTestServer service connection.");
            while (mImpl == null) {
                try {
                    mImplMonitor.wait(SERVICE_CONNECTION_WAIT_INTERVAL_MS);
                } catch (InterruptedException e) {
                    // Ignore the InterruptedException. Rely on the outer while loop to re-run.
                }
                Log.i(TAG, "Still waiting for EmbeddedTestServer service connection.");
            }
            Log.i(TAG, "EmbeddedTestServer service connected.");
            boolean initialized = false;
            try {
                initialized = mImpl.initializeNative(httpsSetting == ServerHTTPSSetting.USE_HTTPS);
            } catch (RemoteException e) {
                Log.e(TAG, "Failed to initialize native server.", e);
                initialized = false;
            }

            if (!initialized) {
                throw new EmbeddedTestServerFailure("Failed to initialize native server.");
            }
            if (!mDisableResetterForTesting) {
                ResettersForTesting.register(this::stopAndDestroyServer);
            }

            if (httpsSetting == ServerHTTPSSetting.USE_HTTPS) {
                try {
                    String rootCertPemPath = mImpl.getRootCertPemPath();
                    X509Util.addTestRootCertificate(CertTestUtil.pemToDer(rootCertPemPath));
                } catch (Exception e) {
                    throw new EmbeddedTestServerFailure(
                            "Failed to install root certificate from native server.", e);
                }
            }
        }
    }

    /** Set intent package and class name that will pass to the service.
     *
     *  @param intent The intent to use to pass into the service.
     */
    protected void setIntentClassName(Intent intent) {
        intent.setClassName(
                "org.chromium.net.test.support", "org.chromium.net.test.EmbeddedTestServerService");
    }

    /** Add the default handlers and serve files from the provided directory relative to the
     *  external storage directory.
     *
     *  @param directory The directory from which files should be served relative to the external
     *      storage directory.
     */
    public void addDefaultHandlers(File directory) {
        addDefaultHandlers(directory.getPath());
    }

    /** Add the default handlers and serve files from the provided directory relative to the
     *  external storage directory.
     *
     *  @param directoryPath The path of the directory from which files should be served relative
     *      to the external storage directory.
     */
    public void addDefaultHandlers(String directoryPath) {
        try {
            synchronized (mImplMonitor) {
                checkServiceLocked();
                mImpl.addDefaultHandlers(directoryPath);
            }
        } catch (RemoteException e) {
            throw new EmbeddedTestServerFailure(
                    "Failed to add default handlers and start serving files from "
                            + directoryPath
                            + ": "
                            + e.toString());
        }
    }

    /** Configure the server to use a particular type of SSL certificate.
     *
     * @param serverCertificate The type of certificate the server should use.
     */
    public void setSSLConfig(@ServerCertificate int serverCertificate) {
        try {
            synchronized (mImplMonitor) {
                checkServiceLocked();
                mImpl.setSSLConfig(serverCertificate);
            }
        } catch (RemoteException e) {
            throw new EmbeddedTestServerFailure(
                    "Failed to set server certificate: " + e.toString());
        }
    }

    /** Serve files from the provided directory.
     *
     *  @param directory The directory from which files should be served.
     */
    public void serveFilesFromDirectory(File directory) {
        serveFilesFromDirectory(directory.getPath());
    }

    /** Serve files from the provided directory.
     *
     *  @param directoryPath The path of the directory from which files should be served.
     */
    public void serveFilesFromDirectory(String directoryPath) {
        try {
            synchronized (mImplMonitor) {
                checkServiceLocked();
                mImpl.serveFilesFromDirectory(directoryPath);
            }
        } catch (RemoteException e) {
            throw new EmbeddedTestServerFailure(
                    "Failed to start serving files from " + directoryPath + ": " + e.toString());
        }
    }

    /**
     * Sets a connection listener. Must be called after the server has been initialized, but
     * before calling {@link start()}.
     *
     * @param listener The listener to set.
     */
    public void setConnectionListener(ConnectionListener listener) {
        try {
            synchronized (mImplMonitor) {
                checkServiceLocked();
                mImpl.setConnectionListener(listener.getListener());
            }
        } catch (RemoteException e) {
            throw new EmbeddedTestServerFailure("Cannot set the listener");
        }
    }

    @GuardedBy("mImplMonitor")
    private void checkServiceLocked() {
        if (mImpl == null) {
            throw new EmbeddedTestServerFailure("Service disconnected.");
        }
    }

    /** Starts the server with an automatically selected port.
     *
     *  Note that this should be called after handlers are set up, including any relevant calls
     *  serveFilesFromDirectory.
     *
     *  @return Whether the server was successfully initialized.
     */
    public boolean start() {
        return start(0);
    }

    /** Starts the server with the specified port.
     *
     *  Note that this should be called after handlers are set up, including any relevant calls
     *  serveFilesFromDirectory.
     *
     *  @param port The port to use for the server, 0 to auto-select an unused port.
     *
     *  @return Whether the server was successfully initialized.
     */
    public boolean start(int port) {
        try {
            synchronized (mImplMonitor) {
                checkServiceLocked();
                return mImpl.start(port);
            }
        } catch (RemoteException e) {
            throw new EmbeddedTestServerFailure("Failed to start server.", e);
        }
    }

    /** Create and initialize a server with the default handlers.
     *
     *  This handles native object initialization, server configuration, and server initialization.
     *  On returning, the server is ready for use.
     *
     *  @param context The context in which the server will run.
     *  @return The created server.
     */
    public static EmbeddedTestServer createAndStartServer(Context context) {
        return createAndStartServerWithPort(context, 0);
    }

    /** Create and initialize a server with the default handlers and specified port.
     *
     *  This handles native object initialization, server configuration, and server initialization.
     *  On returning, the server is ready for use.
     *
     *  @param context The context in which the server will run.
     *  @param port The port to use for the server, 0 to auto-select an unused port.
     *  @return The created server.
     */
    public static EmbeddedTestServer createAndStartServerWithPort(Context context, int port) {
        Assert.assertNotEquals(
                "EmbeddedTestServer should not be created on UiThread, the instantiation will hang"
                    + " forever waiting for tasks to post to UI thread",
                Looper.getMainLooper(),
                Looper.myLooper());
        EmbeddedTestServer server = new EmbeddedTestServer();
        return initializeAndStartServer(server, context, port);
    }

    /** Create and initialize an HTTPS server with the default handlers.
     *
     *  This handles native object initialization, server configuration, and server initialization.
     *  On returning, the server is ready for use.
     *
     *  @param context The context in which the server will run.
     *  @param serverCertificate The certificate option that the server will use.
     *  @return The created server.
     */
    public static EmbeddedTestServer createAndStartHTTPSServer(
            Context context, @ServerCertificate int serverCertificate) {
        return createAndStartHTTPSServerWithPort(context, serverCertificate, /* port= */ 0);
    }

    /** Create and initialize an HTTPS server with the default handlers and specified port.
     *
     *  This handles native object initialization, server configuration, and server initialization.
     *  On returning, the server is ready for use.
     *
     *  @param context The context in which the server will run.
     *  @param serverCertificate The certificate option that the server will use.
     *  @param port The port to use for the server, 0 to auto-select an unused port.
     *  @return The created server.
     */
    public static EmbeddedTestServer createAndStartHTTPSServerWithPort(
            Context context, @ServerCertificate int serverCertificate, int port) {
        Assert.assertNotEquals(
                "EmbeddedTestServer should not be created on UiThread, "
                        + "the instantiation will hang forever waiting for tasks"
                        + " to post to UI thread",
                Looper.getMainLooper(),
                Looper.myLooper());
        EmbeddedTestServer server = new EmbeddedTestServer();
        return initializeAndStartHTTPSServer(server, context, serverCertificate, port);
    }

    /** Initialize a server with the default handlers.
     *
     *  This handles native object initialization, server configuration, and server initialization.
     *  On returning, the server is ready for use.
     *
     *  @param server The server instance that will be initialized.
     *  @param context The context in which the server will run.
     *  @param port The port to use for the server, 0 to auto-select an unused port.
     *  @return The created server.
     */
    public static <T extends EmbeddedTestServer> T initializeAndStartServer(
            T server, Context context, int port) {
        server.initializeNative(context, ServerHTTPSSetting.USE_HTTP);
        server.addDefaultHandlers("");
        if (!server.start(port)) {
            throw new EmbeddedTestServerFailure("Failed to start serving using default handlers.");
        }
        return server;
    }

    /** Initialize a server with the default handlers that uses HTTPS with the given certificate
     * option.
     *
     *  This handles native object initialization, server configuration, and server initialization.
     *  On returning, the server is ready for use.
     *
     *  @param server The server instance that will be initialized.
     *  @param context The context in which the server will run.
     *  @param serverCertificate The certificate option that the server will use.
     *  @param port The port to use for the server.
     *  @return The created server.
     */
    public static <T extends EmbeddedTestServer> T initializeAndStartHTTPSServer(
            T server, Context context, @ServerCertificate int serverCertificate, int port) {
        server.initializeNative(context, ServerHTTPSSetting.USE_HTTPS);
        server.addDefaultHandlers("");
        server.setSSLConfig(serverCertificate);
        if (!server.start(port)) {
            throw new EmbeddedTestServerFailure("Failed to start serving using default handlers.");
        }
        return server;
    }

    /** Get the full URL for the given relative URL.
     *
     *  @param relativeUrl The relative URL for which a full URL will be obtained.
     *  @return The URL as a String.
     */
    public String getURL(String relativeUrl) {
        try {
            synchronized (mImplMonitor) {
                checkServiceLocked();
                return mImpl.getURL(relativeUrl);
            }
        } catch (RemoteException e) {
            throw new EmbeddedTestServerFailure("Failed to get URL for " + relativeUrl, e);
        }
    }

    /** Get the full URL for the given relative URL. Similar to the above method but uses the given
     *  hostname instead of 127.0.0.1. The hostname should be resolved to 127.0.0.1.
     *
     *  @param hostName The host name which should be used.
     *  @param relativeUrl The relative URL for which a full URL should be returned.
     *  @return The URL as a String.
     */
    public String getURLWithHostName(String hostname, String relativeUrl) {
        try {
            synchronized (mImplMonitor) {
                checkServiceLocked();
                return mImpl.getURLWithHostName(hostname, relativeUrl);
            }
        } catch (RemoteException e) {
            throw new EmbeddedTestServerFailure(
                    "Failed to get URL for " + hostname + " and " + relativeUrl, e);
        }
    }

    /** Get the full URLs for the given relative URLs.
     *
     *  @see #getURL(String)
     *
     *  @param relativeUrls The relative URLs for which full URLs will be obtained.
     *  @return The URLs as a String array.
     */
    public String[] getURLs(String... relativeUrls) {
        String[] absoluteUrls = new String[relativeUrls.length];

        for (int i = 0; i < relativeUrls.length; ++i) absoluteUrls[i] = getURL(relativeUrls[i]);

        return absoluteUrls;
    }

    /**
     * Stop and destroy the server.
     *
     *  This handles stopping the server and destroying the native object.
     */
    public void stopAndDestroyServer() {
        synchronized (mImplMonitor) {
            // ResettersForTesting call can cause this to be called multiple times.
            if (mImpl == null) {
                return;
            }
            try {
                if (!mImpl.shutdownAndWaitUntilComplete()) {
                    throw new EmbeddedTestServerFailure("Failed to stop server.");
                }
                mImpl.destroy();
                mImpl = null;
            } catch (RemoteException e) {
                throw new EmbeddedTestServerFailure("Failed to shut down.", e);
            } finally {
                mContext.unbindService(mConn);
            }
        }
    }

    /** Get the path of the PEM file of the root cert. */
    public String getRootCertPemPath() {
        try {
            synchronized (mImplMonitor) {
                checkServiceLocked();
                return mImpl.getRootCertPemPath();
            }
        } catch (RemoteException e) {
            throw new EmbeddedTestServerFailure("Failed to get root cert's path", e);
        }
    }

    public static void initCerts() {
        if (sTestRootInitDone) {
            return;
        }

        // Always try to add the testing HTTPS root to the cert verifier. We do this here because we
        // need this to happen before the native code loads the user-added roots, and this is the
        // safest place to put it.
        try {
            // Use the same PEM file as net/test/embedded_test_server/embedded_test_server.cc.
            String rootCertPemPath =
                    UrlUtils.getIsolatedTestFilePath("net/data/ssl/certificates/root_ca_cert.pem");
            byte[] rootCertBytesDer = CertTestUtil.pemToDer(rootCertPemPath);
            X509Util.setTestRootCertificateForBuiltin(rootCertBytesDer);
            sTestRootInitDone = true;
        } catch (Exception e) {
            throw new EmbeddedTestServer.EmbeddedTestServerFailure(
                    "Failed to install root certificate.", e);
        }
    }
}
