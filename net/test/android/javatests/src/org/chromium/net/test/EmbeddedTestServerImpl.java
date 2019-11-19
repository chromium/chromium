// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.RemoteException;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.UrlUtils;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Java bindings for running a net::test_server::EmbeddedTestServer.
 *
 * This should not be used directly. Use {@link EmbeddedTestServer} instead.
 */
@JNINamespace("net::test_server")
public class EmbeddedTestServerImpl extends IEmbeddedTestServerImpl.Stub {
    private static final String TAG = "TestServer";

    private static AtomicInteger sCount = new AtomicInteger();

    private final Context mContext;
    private Handler mHandler;
    private HandlerThread mHandlerThread;
    private long mNativeEmbeddedTestServer;
    private IConnectionListener mConnectionListener;

    /** Create an uninitialized EmbeddedTestServer. */
    public EmbeddedTestServerImpl(Context context) {
        mContext = context;
    }

    private <V> V runOnHandlerThread(Callable<V> c) {
        FutureTask<V> t = new FutureTask<>(c);
        mHandler.post(t);
        try {
            return t.get();
        } catch (ExecutionException e) {
            Log.e(TAG, "Exception raised from native EmbeddedTestServer", e);
        } catch (InterruptedException e) {
            Log.e(TAG, "Interrupted while waiting for native EmbeddedTestServer", e);
        }
        return null;
    }

    /** Initialize the native EmbeddedTestServer object.
     *
     *  @param https True if the server should use HTTPS, and false otherwise.
     *  @return Whether the native object was successfully initialized.
     */
    @Override
    public boolean initializeNative(final boolean https) {
        // This is necessary as EmbeddedTestServerImpl is in a different process than the tests
        // using it, so it needs to initialize its own application context.
        ContextUtils.initApplicationContext(mContext.getApplicationContext());
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

        mHandlerThread = new HandlerThread("EmbeddedTestServer" + sCount.getAndIncrement());
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());

        runOnHandlerThread(new Callable<Void>() {
            @Override
            public Void call() {
                if (mNativeEmbeddedTestServer == 0) {
                    nativeInit(UrlUtils.getIsolatedTestRoot(), https);
                }
                assert mNativeEmbeddedTestServer != 0;
                return null;
            }
        });
        return true;
    }

    /** Starts the server.
     *
     *  Note that this should be called after handlers are set up, including any relevant calls
     *  serveFilesFromDirectory.
     *
     *  @param port The port to use for the server, 0 to auto-select an unused port.
     *
     *  @return Whether the server was successfully started.
     */
    @Override
    public boolean start(int port) {
        return runOnHandlerThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return nativeStart(mNativeEmbeddedTestServer, port);
            }
        });
    }

    /** Returns the path to a PEM file containing the server's root certificate.
     *
     *  @return The path to a PEM file containing the server's root certificate.
     */
    @Override
    public String getRootCertPemPath() {
        return runOnHandlerThread(new Callable<String>() {
            @Override
            public String call() {
                return nativeGetRootCertPemPath(mNativeEmbeddedTestServer);
            }
        });
    }

    /** Add the default handlers and serve files from the provided directory relative to the
     *  external storage directory.
     *
     *  @param directoryPath The path of the directory from which files should be served, relative
     *      to the external storage directory.
     */
    @Override
    public void addDefaultHandlers(final String directoryPath) {
        runOnHandlerThread(new Callable<Void>() {
            @Override
            public Void call() {
                nativeAddDefaultHandlers(mNativeEmbeddedTestServer, directoryPath);
                return null;
            }
        });
    }

    /** Configure the server to use a particular type of SSL certificate.
     *
     * @param serverCertificate The type of certificate the server should use.
     */
    @Override
    public void setSSLConfig(final int serverCertificate) {
        runOnHandlerThread(new Callable<Void>() {
            @Override
            public Void call() {
                nativeSetSSLConfig(mNativeEmbeddedTestServer, serverCertificate);
                return null;
            }
        });
    }

    /** Register multiple request handlers.
     *  Handlers must be registered before starting the server.
     *
     *  @param handler The pointer of handler to be registered.
     */
    public void registerRequestHandler(final long handler) {
        runOnHandlerThread(new Callable<Void>() {
            @Override
            public Void call() {
                nativeRegisterRequestHandler(mNativeEmbeddedTestServer, handler);
                return null;
            }
        });
    }

    /** Serve files from the provided directory.
     *
     *  @param directoryPath The path of the directory from which files should be served.
     */
    @Override
    public void serveFilesFromDirectory(final String directoryPath) {
        runOnHandlerThread(new Callable<Void>() {
            @Override
            public Void call() {
                nativeServeFilesFromDirectory(mNativeEmbeddedTestServer, directoryPath);
                return null;
            }
        });
    }

    /** Sets a connection listener to be notified of new connections and socket reads.
     *
     * Must be done before starting the server. Setting a new one erases the previous one.
     *
     * @param listener Listener to notify.
     */
    @Override
    public void setConnectionListener(final IConnectionListener listener) {
        runOnHandlerThread(new Callable<Void>() {
            @Override
            public Void call() {
                mConnectionListener = listener;
                return null;
            }
        });
    }

    /** Get the full URL for the given relative URL.
     *
     *  @param relativeUrl The relative URL for which a full URL should be returned.
     *  @return The URL as a String.
     */
    @Override
    public String getURL(final String relativeUrl) {
        return runOnHandlerThread(new Callable<String>() {
            @Override
            public String call() {
                return nativeGetURL(mNativeEmbeddedTestServer, relativeUrl);
            }
        });
    }

    /** Get the full URL for the given relative URL. Similar to the above method but uses the given
     *  hostname instead of 127.0.0.1. The hostname should be resolved to 127.0.0.1.
     *
     *  @param hostName The host name which should be used.
     *  @param relativeUrl The relative URL for which a full URL should be returned.
     *  @return The URL as a String.
     */
    @Override
    public String getURLWithHostName(final String hostName, final String relativeUrl) {
        return runOnHandlerThread(new Callable<String>() {
            @Override
            public String call() {
                return nativeGetURLWithHostName(mNativeEmbeddedTestServer, hostName, relativeUrl);
            }
        });
    }

    /** Shut down the server.
     *
     *  @return Whether the server was successfully shut down.
     */
    @Override
    public boolean shutdownAndWaitUntilComplete() {
        return runOnHandlerThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return nativeShutdownAndWaitUntilComplete(mNativeEmbeddedTestServer);
            }
        });
    }

    /** Destroy the native EmbeddedTestServer object. */
    @Override
    public void destroy() {
        runOnHandlerThread(new Callable<Void>() {
            @Override
            public Void call() {
                assert mNativeEmbeddedTestServer != 0;
                nativeDestroy(mNativeEmbeddedTestServer);
                assert mNativeEmbeddedTestServer == 0;
                return null;
            }
        });

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            mHandlerThread.quitSafely();
        } else {
            runOnHandlerThread(new Callable<Void>() {
                @Override
                public Void call() {
                    mHandlerThread.quit();
                    return null;
                }
            });
        }

        try {
            mHandlerThread.join();
        } catch (InterruptedException e) {
        }
    }

    @CalledByNative
    private void acceptedSocket(long socketId) {
        if (mConnectionListener == null) return;
        try {
            mConnectionListener.acceptedSocket(socketId);
        } catch (RemoteException e) {
            // Callback, ignore exception.
        }
    }

    @CalledByNative
    private void readFromSocket(long socketId) {
        if (mConnectionListener == null) return;
        try {
            mConnectionListener.readFromSocket(socketId);
        } catch (RemoteException e) {
            // Callback, ignore exception.
        }
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        assert mNativeEmbeddedTestServer == 0;
        mNativeEmbeddedTestServer = nativePtr;
    }

    @CalledByNative
    private void clearNativePtr() {
        assert mNativeEmbeddedTestServer != 0;
        mNativeEmbeddedTestServer = 0;
    }

    private native void nativeInit(String testDataDir, boolean https);
    private native void nativeDestroy(long nativeEmbeddedTestServerAndroid);
    private native boolean nativeStart(long nativeEmbeddedTestServerAndroid, int port);
    private native String nativeGetRootCertPemPath(long nativeEmbeddedTestServerAndroid);
    private native boolean nativeShutdownAndWaitUntilComplete(long nativeEmbeddedTestServerAndroid);
    private native void nativeAddDefaultHandlers(
            long nativeEmbeddedTestServerAndroid, String directoryPath);
    private native void nativeSetSSLConfig(
            long nativeEmbeddedTestServerAndroid, int serverCertificate);
    private native void nativeRegisterRequestHandler(
            long nativeEmbeddedTestServerAndroid, long handler);
    private native String nativeGetURL(long nativeEmbeddedTestServerAndroid, String relativeUrl);
    private native String nativeGetURLWithHostName(
            long nativeEmbeddedTestServerAndroid, String hostName, String relativeUrl);
    private native void nativeServeFilesFromDirectory(
            long nativeEmbeddedTestServerAndroid, String directoryPath);
}
