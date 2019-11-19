// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.content.Context;
import android.os.Bundle;
import android.os.IBinder;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.process_launcher.ChildConnectionAllocator;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_shell_apk.ChildProcessLauncherTestUtils;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;

/**
 * Integration test that starts the full shell and load pages to test ChildProcessLauncher
 * and related code.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ChildProcessLauncherIntegrationTest {
    @Rule
    public final ContentShellActivityTestRule mActivityTestRule =
            new ContentShellActivityTestRule();

    private static class TestChildProcessConnectionFactory
            implements ChildConnectionAllocator.ConnectionFactory {
        private final List<TestChildProcessConnection> mConnections = new ArrayList<>();

        @Override
        public ChildProcessConnection createConnection(Context context, ComponentName serviceName,
                boolean bindToCaller, boolean bindAsExternalService, Bundle serviceBundle,
                String instanceName) {
            TestChildProcessConnection connection = new TestChildProcessConnection(
                    context, serviceName, bindToCaller, bindAsExternalService, serviceBundle);
            mConnections.add(connection);
            return connection;
        }

        public List<TestChildProcessConnection> getConnections() {
            return mConnections;
        }
    }

    private static class TestChildProcessConnection extends ChildProcessConnection {
        private RuntimeException mRemovedBothModerateAndStrongBinding;

        public TestChildProcessConnection(Context context, ComponentName serviceName,
                boolean bindToCaller, boolean bindAsExternalService,
                Bundle childProcessCommonParameters) {
            super(context, serviceName, bindToCaller, bindAsExternalService,
                    childProcessCommonParameters, null /* instanceName */);
        }

        @Override
        protected void unbind() {
            super.unbind();
            if (mRemovedBothModerateAndStrongBinding == null) {
                mRemovedBothModerateAndStrongBinding = new RuntimeException("unbind");
            }
        }

        @Override
        public void removeModerateBinding() {
            super.removeModerateBinding();
            if (mRemovedBothModerateAndStrongBinding == null && !isStrongBindingBound()) {
                mRemovedBothModerateAndStrongBinding =
                        new RuntimeException("removeModerateBinding");
            }
        }

        @Override
        public void removeStrongBinding() {
            super.removeStrongBinding();
            if (mRemovedBothModerateAndStrongBinding == null && !isModerateBindingBound()) {
                mRemovedBothModerateAndStrongBinding = new RuntimeException("removeStrongBinding");
            }
        }

        public void throwIfDroppedBothModerateAndStrongBinding() {
            if (mRemovedBothModerateAndStrongBinding != null) {
                throw new RuntimeException(mRemovedBothModerateAndStrongBinding);
            }
        }
    }

    @Test
    @MediumTest
    public void testCrossDomainNavigationDoNotLoseImportance() throws Throwable {
        final TestChildProcessConnectionFactory factory = new TestChildProcessConnectionFactory();
        final List<TestChildProcessConnection> connections = factory.getConnections();
        ChildProcessLauncherHelperImpl.setSandboxServicesSettingsForTesting(factory,
                10 /* arbitrary number, only realy need 2 */, null /* use default service name */);

        // TODO(boliu,nasko): Ensure navigation is actually successful
        // before proceeding.
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrlSync(
                "content/test/data/android/title1.html");
        NavigationController navigationController =
                mActivityTestRule.getWebContents().getNavigationController();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(activity.getActiveWebContents());

        mActivityTestRule.loadUrl(navigationController, testCallbackHelperContainer,
                new LoadUrlParams(UrlUtils.getIsolatedTestFileUrl(
                        "content/test/data/android/geolocation.html")));
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(1, connections.size());
                connections.get(0).throwIfDroppedBothModerateAndStrongBinding();
            }
        });

        mActivityTestRule.loadUrl(
                navigationController, testCallbackHelperContainer, new LoadUrlParams("data:,foo"));
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(2, connections.size());
                // connections.get(0).didDropBothInitialAndImportantBindings();
                connections.get(1).throwIfDroppedBothModerateAndStrongBinding();
            }
        });
    }

    @Test
    @MediumTest
    public void testIntentionalKillToFreeServiceSlot() throws Throwable {
        final TestChildProcessConnectionFactory factory = new TestChildProcessConnectionFactory();
        final List<TestChildProcessConnection> connections = factory.getConnections();
        ChildProcessLauncherHelperImpl.setSandboxServicesSettingsForTesting(
                factory, 1, null /* use default service name */);
        // Doing a cross-domain navigation would need to kill the first process in order to create
        // the second process.

        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrlSync(
                "content/test/data/android/vsync.html");
        NavigationController navigationController =
                mActivityTestRule.getWebContents().getNavigationController();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(activity.getActiveWebContents());

        mActivityTestRule.loadUrl(navigationController, testCallbackHelperContainer,
                new LoadUrlParams(UrlUtils.getIsolatedTestFileUrl(
                        "content/test/data/android/geolocation.html")));
        mActivityTestRule.loadUrl(
                navigationController, testCallbackHelperContainer, new LoadUrlParams("data:,foo"));

        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(2, connections.size());
                Assert.assertTrue(connections.get(0).isKilledByUs());
            }
        });
    }

    private static class CrashOnLaunchChildProcessConnection extends TestChildProcessConnection {
        private boolean mCrashServiceCalled;
        private final CountDownLatch mDisconnectedLatch = new CountDownLatch(1);
        // Arguments to setupConnection
        private Bundle mConnectionBundle;
        private List<IBinder> mClientInterfaces;
        private ConnectionCallback mConnectionCallback;

        public CrashOnLaunchChildProcessConnection(Context context, ComponentName serviceName,
                boolean bindToCaller, boolean bindAsExternalService,
                Bundle childProcessCommonParameters) {
            super(context, serviceName, bindToCaller, bindAsExternalService,
                    childProcessCommonParameters);
        }

        @Override
        protected void onServiceConnectedOnLauncherThread(IBinder service) {
            super.onServiceConnectedOnLauncherThread(service);
            crashServiceForTesting();
            mCrashServiceCalled = true;
            if (mConnectionBundle != null) {
                super.setupConnection(mConnectionBundle, mClientInterfaces, mConnectionCallback);
                mConnectionBundle = null;
                mClientInterfaces = null;
                mConnectionCallback = null;
            }
        }

        @Override
        protected void onServiceDisconnectedOnLauncherThread() {
            super.onServiceDisconnectedOnLauncherThread();
            mDisconnectedLatch.countDown();
        }

        @Override
        public void setupConnection(Bundle connectionBundle, List<IBinder> clientInterfaces,
                ConnectionCallback connectionCallback) {
            // Make sure setupConnection is called after crashServiceForTesting so that
            // setupConnection is guaranteed to fail.
            if (mCrashServiceCalled) {
                super.setupConnection(connectionBundle, clientInterfaces, connectionCallback);
                return;
            }
            mConnectionBundle = connectionBundle;
            mClientInterfaces = clientInterfaces;
            mConnectionCallback = connectionCallback;
        }

        public void waitForDisconnect() throws InterruptedException {
            mDisconnectedLatch.await();
        }
    }

    private static class CrashOnLaunchChildProcessConnectionFactory
            extends TestChildProcessConnectionFactory {
        // Only create one CrashOnLaunchChildProcessConnection.
        private CrashOnLaunchChildProcessConnection mCrashConnection;

        @Override
        public ChildProcessConnection createConnection(Context context, ComponentName serviceName,
                boolean bindToCaller, boolean bindAsExternalService, Bundle serviceBundle,
                String instanceName) {
            if (mCrashConnection == null) {
                mCrashConnection = new CrashOnLaunchChildProcessConnection(
                        context, serviceName, bindToCaller, bindAsExternalService, serviceBundle);
                return mCrashConnection;
            }
            return super.createConnection(context, serviceName, bindToCaller, bindAsExternalService,
                    serviceBundle, instanceName);
        }

        public CrashOnLaunchChildProcessConnection getCrashConnection() {
            return mCrashConnection;
        }
    }

    @Test
    @MediumTest
    public void testCrashOnLaunch() throws Throwable {
        final CrashOnLaunchChildProcessConnectionFactory factory =
                new CrashOnLaunchChildProcessConnectionFactory();
        ChildProcessLauncherHelperImpl.setSandboxServicesSettingsForTesting(
                factory, 1, null /* use default service name */);

        // Load url which should fail.
        String url = UrlUtils.getIsolatedTestFileUrl("content/test/data/android/title1.html");
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl(url);

        // Poll until connection is allocated, then wait until connection is disconnected.
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("The connection wasn't established.") {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                                () -> factory.getCrashConnection() != null);
                    }
                });
        CrashOnLaunchChildProcessConnection crashConnection =
                ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                        () -> factory.getCrashConnection());
        crashConnection.waitForDisconnect();

        // Load a new URL and make sure everything is ok.
        NavigationController navigationController =
                mActivityTestRule.getWebContents().getNavigationController();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(activity.getActiveWebContents());
        mActivityTestRule.loadUrl(navigationController, testCallbackHelperContainer,
                new LoadUrlParams(UrlUtils.getIsolatedTestFileUrl(
                        "content/test/data/android/geolocation.html")));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        Assert.assertTrue(factory.getConnections().size() > 0);
    }
}
