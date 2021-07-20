/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.support.test.rule;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.support.annotation.NonNull;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.Beta;
import android.support.test.internal.util.Checks;
import android.util.Log;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;


/**
 * A JUnit rule that provides a simplified mechanism to start and shutdown your service before
 * and after the duration of your test. It also guarantees that the service is successfully
 * connected when starting (or binding to) a service. The service can be started
 * (or bound) using one of the helper methods. It will automatically be stopped (or unbound) after
 * the test completes and any methods annotated with
 * <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html"><code>After</code></a> are
 * finished.
 * <p>
 * Note: This rule doesn't support {@link android.app.IntentService} because it's automatically
 * destroyed when {@link android.app.IntentService#onHandleIntent(android.content.Intent)} finishes
 * all outstanding commands. So there is no guarantee to establish a successful connection
 * in a timely manner.
 *
 * <p>
 * Usage:
 * <pre>
 * &#064;Rule
 * public final ServiceTestRule mServiceRule = new ServiceTestRule();
 *
 * &#064;Test
 * public void testWithStartedService() {
 *     mServiceRule.startService(
 *         new Intent(InstrumentationRegistry.getTargetContext(), MyService.class));
 *     //do something
 * }
 *
 * &#064;Test
 * public void testWithBoundService() {
 *     IBinder binder = mServiceRule.bindService(
 *         new Intent(InstrumentationRegistry.getTargetContext(), MyService.class));
 *     MyService service = ((MyService.LocalBinder) binder).getService();
 *     assertTrue("True wasn't returned", service.doSomethingToReturnTrue());
 * }
 * </pre>
 * <p>
 */
@Beta
public class ServiceTestRule implements TestRule {

    private static final String TAG = "ServiceTestRule";
    private static final long DEFAULT_TIMEOUT = 5L; //seconds

    private static IBinder mIBinder;
    private Intent mServiceIntent;
    private ServiceConnection mServiceConn;
    private long mTimeout;
    private TimeUnit mTimeUnit;

    boolean mServiceStarted = false;
    boolean mServiceBound = false;

    /**
     * Creates a {@link ServiceTestRule} with a default timeout of 5 seconds
     */
    public ServiceTestRule() {
        this(DEFAULT_TIMEOUT, TimeUnit.SECONDS);
    }

    /**
     * Factory method to create a {@link ServiceTestRule} with a custom timeout
     *
     * @param timeout the amount of time to wait for a service to connect.
     * @param timeUnit the time unit representing how the timeout parameter should be interpreted
     * @return a {@link ServiceTestRule} with the desired timeout
     */
    public static ServiceTestRule withTimeout(long timeout, TimeUnit timeUnit) {
        return new ServiceTestRule(timeout, timeUnit);
    }

    private ServiceTestRule(long timeout, TimeUnit timeUnit) {
        mTimeout = timeout;
        mTimeUnit = timeUnit;
    }


    /**
     * Starts the service under test and blocks until the service is connected, in the same way as
     * if it were started by {@link android.content.Context#startService(Intent)
     * Context.startService(Intent)} with an {@link android.content.Intent} that identifies a
     * service. If you use this method to start the service, it is automatically stopped at the
     * end of the test run. However, it also binds to the service and waits for
     * {@link ServiceConnection#onServiceConnected(android.content.ComponentName,
     * android.os.IBinder)} to be called to ensure successful connection.
     *
     * @param intent An Intent that identifies a service, of the same form as the Intent passed to
     *               {@link android.content.Context#startService(Intent) Context.startService
     *               (Intent)}.
     *
     * @throws SecurityException if you do not have permission to bind to the given service.
     * @throws TimeoutException  if timed out waiting for a successful connection with the service.
     */
    public void startService(@NonNull Intent intent) throws TimeoutException {
        mServiceIntent = Checks.checkNotNull(intent, "intent can't be null");
        InstrumentationRegistry.getTargetContext().startService(mServiceIntent);
        mServiceStarted = true;

        // bind to the started service to guarantee its started and connected before test execution
        mServiceBound = bindServiceAndWait(mServiceIntent, null, Context.BIND_AUTO_CREATE);
    }


    /**
     * Works just like
     * {@link #bindService(android.content.Intent, android.content.ServiceConnection, int)} except
     * uses an internal {@link android.content.ServiceConnection} to guarantee successful bound.
     * The operation option flag defaults to {@link android.content.Context#BIND_AUTO_CREATE}
     *
     * @see #bindService(android.content.Intent, android.content.ServiceConnection, int)
     */
    public IBinder bindService(@NonNull Intent intent) throws TimeoutException {
        // no extras are expected by unbind
        mServiceIntent = Checks.checkNotNull(intent, "intent can't be null").cloneFilter();
        mServiceBound = bindServiceAndWait(intent, null, Context.BIND_AUTO_CREATE);
        return mIBinder;
    }


    /**
     * Starts the service under test, in the same way as if it were started by
     * {@link android.content.Context#bindService(Intent, ServiceConnection, int)
     * Context.bindService(Intent, ServiceConnection, flags)} with an
     * {@link android.content.Intent} that identifies a service. However, it waits for
     * {@link ServiceConnection#onServiceConnected(android.content.ComponentName,
     * android.os.IBinder)} to be called before returning.
     *
     * @param intent     Identifies the service to connect to.  The Intent may
     *                   specify either an explicit component name, or a logical
     *                   description (action, category, etc) to match an
     *                   {@link android.content.IntentFilter} published by a service.
     * @param connection Receives information as the service is started and stopped.
     *                   This must be a valid ServiceConnection object; it must not be null.
     * @param flags      Operation options for the binding.  May be 0,
     *                   {@link android.content.Context#BIND_AUTO_CREATE},
     *                   {@link android.content.Context#BIND_DEBUG_UNBIND},
     *                   {@link android.content.Context#BIND_NOT_FOREGROUND},
     *                   {@link android.content.Context#BIND_ABOVE_CLIENT},
     *                   {@link android.content.Context#BIND_ALLOW_OOM_MANAGEMENT}, or
     *                   {@link android.content.Context#BIND_WAIVE_PRIORITY}.
     * @return An object whose type is a subclass of IBinder, for making further calls into
     * the service.
     * @throws SecurityException if the called doesn't have permission to bind to the given service.
     * @throws TimeoutException  if timed out waiting for a successful connection with the service.
     * @see android.content.Context#BIND_AUTO_CREATE
     * @see android.content.Context#BIND_DEBUG_UNBIND
     * @see android.content.Context#BIND_NOT_FOREGROUND
     */
    public IBinder bindService(@NonNull Intent intent, @NonNull ServiceConnection connection,
                               int flags) throws TimeoutException {
        // no extras are expected by unbind
        mServiceIntent = Checks.checkNotNull(intent, "intent can't be null").cloneFilter();
        ServiceConnection c = Checks.checkNotNull(connection, "connection can't be null");
        mServiceBound = bindServiceAndWait(mServiceIntent, c, flags);

        return mIBinder;
    }

    // Visible for testing
    boolean bindServiceAndWait(Intent intent, final ServiceConnection conn,
                               int flags) throws TimeoutException {

        mServiceConn = new ProxyServiceConnection(conn);

        boolean isBound = InstrumentationRegistry.getTargetContext().bindService(intent,
                mServiceConn, flags);

        if (isBound) {
            // block until service connection is established
            waitOnLatch(ProxyServiceConnection.mConnectedLatch, "connected");
        } else {
            Log.e(TAG, "Failed to bind to service");
        }

        return isBound;
    }

    /**
     * This class is used to wait until a successful connection to the service was established. It
     * then serves as a proxy to original {@link android.content.ServiceConnection} passed by
     * the caller.
     */
     static class ProxyServiceConnection implements ServiceConnection {
        private ServiceConnection mCallerConnection;
        public static CountDownLatch mConnectedLatch = new CountDownLatch(1);

        private ProxyServiceConnection(ServiceConnection connection) {
            mCallerConnection = connection;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            // store the service binder to return to the caller
            mIBinder = service;
            if (mCallerConnection != null) {
                // pass through everything to the callers ServiceConnection
                mCallerConnection.onServiceConnected(name, service);
            }
            mConnectedLatch.countDown();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            //The process hosting the service has crashed or been killed.
            Log.e(TAG, "Connection to the Service has been lost!");
            mIBinder = null;
            if (mCallerConnection != null) {
                // pass through everything to the callers ServiceConnection
                mCallerConnection.onServiceDisconnected(name);
            }
        }
    }

    /**
     * Helper method to block on a given latch for the duration of the set timeout
     */
    // Visible for testing
    void waitOnLatch(CountDownLatch latch, String actionName) throws TimeoutException {
        try {
            if (!latch.await(mTimeout, mTimeUnit)) {
                throw new TimeoutException("Waited for " + mTimeout + " " + mTimeUnit.name() + "," +
                        " but service was never " + actionName);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new RuntimeException("Interrupted while waiting for service to be " +
                    actionName, e);
        }
    }

    /**
     * Makes the necessary calls to stop (or unbind) the service under test. This method is called
     * automatically called after test execution. This is not a blocking call since there is no
     * reliable way to guarantee successful disconnect without access to service lifecycle.
     */
    // Visible for testing
    void shutdownService() throws TimeoutException {
        if (mServiceStarted) {
            InstrumentationRegistry.getTargetContext().stopService(mServiceIntent);
            mServiceStarted = false;
        }
        if (mServiceBound) {
            InstrumentationRegistry.getTargetContext().unbindService(mServiceConn);
            mIBinder = null;
            mServiceBound = false;
        }
    }


    /**
     * Override this method to do your own service specific initialization before starting or
     * binding to the service. The method is called before each test method is executed including
     * any method annotated with
     * <a href="http://junit.sourceforge.net/javadoc/org/junit/Before.html"><code>Before</code></a>.
     * Do not start or bind to a service from here!
     */
    protected void beforeService() {
        // empty by default
    }

    /**
     * Override this method to do your own service specific clean up after the service is shutdown.
     * The method is called after each test method is executed including any method annotated with
     * <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html"><code>After</code></a>
     * and after necessary calls to stop (or unbind) the service under test were called.
     */
    protected void afterService() {
        // empty by default
    }


    @Override
    public Statement apply(final Statement base, Description description) {
        return new ServiceStatement(base);
    }

    /**
     * {@link Statement} that executes the service lifecycle methods before and after the execution
     * of the test.
     */
    private class ServiceStatement extends Statement {
        private final Statement mBase;

        public ServiceStatement(Statement base) {
            mBase = base;
        }

        @Override
        public void evaluate() throws Throwable {
            try {
                beforeService();
                mBase.evaluate();
            } finally {
                shutdownService();
                afterService();
            }
        }
    }
}
