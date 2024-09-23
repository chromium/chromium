// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.content.Context;
import android.os.Bundle;
import android.os.IBinder;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.process_launcher.ChildConnectionAllocator;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
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
        public ChildProcessConnection createConnection(
                Context context,
                ComponentName serviceName,
                ComponentName fallbackServiceName,
                boolean bindToCaller,
                boolean bindAsExternalService,
                Bundle serviceBundle,
                String instanceName) {
            TestChildProcessConnection connection =
                    new TestChildProcessConnection(
                            context,
                            serviceName,
                            bindToCaller,
                            bindAsExternalService,
                            serviceBundle);
            mConnections.add(connection);
            return connection;
        }

        public List<TestChildProcessConnection> getConnections() {
            return mConnections;
        }
    }

    private static class TestChildProcessConnection extends ChildProcessConnection {
        private RuntimeException mRemovedBothVisibleAndStrongBinding;

        public TestChildProcessConnection(
                Context context,
                ComponentName serviceName,
                boolean bindToCaller,
                boolean bindAsExternalService,
                Bundle childProcessCommonParameters) {
            super(
                    context,
                    serviceName,
                    /* fallbackServiceName= */ null,
                    bindToCaller,
                    bindAsExternalService,
                    childProcessCommonParameters,
                    /* instanceName= */ null);
        }

        @Override
        protected void unbind() {
            super.unbind();
            if (mRemovedBothVisibleAndStrongBinding == null) {
                mRemovedBothVisibleAndStrongBinding = new RuntimeException("unbind");
            }
        }

        @Override
        public void removeVisibleBinding() {
            super.removeVisibleBinding();
            if (mRemovedBothVisibleAndStrongBinding == null && !isStrongBindingBound()) {
                mRemovedBothVisibleAndStrongBinding = new RuntimeException("removeVisibleBinding");
            }
        }

        @Override
        public void removeStrongBinding() {
            super.removeStrongBinding();
            if (mRemovedBothVisibleAndStrongBinding == null && !isVisibleBindingBound()) {
                mRemovedBothVisibleAndStrongBinding = new RuntimeException("removeStrongBinding");
            }
        }

        public void throwIfDroppedBothVisibleAndStrongBinding() {
            if (mRemovedBothVisibleAndStrongBinding != null) {
                throw new RuntimeException(mRemovedBothVisibleAndStrongBinding);
            }
        }
    }

    @Test
    @MediumTest
    // This test may run with --site-per-process or AndroidWarmUpSpareRendererWithTimeout, which
    // also enables a feature to maintain a spare renderer process.
    // The spare process interferes with assertions on the number of process connections in this
    // test, so disable it.
    @CommandLineFlags.Add({
        "disable-features=SpareRendererForSitePerProcess,AndroidWarmUpSpareRendererWithTimeout"
    })
    public void testCrossDomainNavigationDoNotLoseImportance() throws Throwable {
        final TestChildProcessConnectionFactory factory = new TestChildProcessConnectionFactory();
        final List<TestChildProcessConnection> connections = factory.getConnections();
        ChildProcessLauncherHelperImpl.setSandboxServicesSettingsForTesting(
                factory,
                10 /* arbitrary number, only really need 2 */,
                null /* use default service name */);

        // TODO(boliu,nasko): Ensure navigation is actually successful
        // before proceeding.
        ContentShellActivity activity =
                mActivityTestRule.launchContentShellWithUrlSync(
                        "content/test/data/android/title1.html");
        NavigationController navigationController =
                mActivityTestRule.getWebContents().getNavigationController();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(activity.getActiveWebContents());

        mActivityTestRule.loadUrl(
                navigationController,
                testCallbackHelperContainer,
                new LoadUrlParams(
                        UrlUtils.getIsolatedTestFileUrl(
                                "content/test/data/android/geolocation.html")));
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        Assert.assertEquals(1, connections.size());
                        connections.get(0).throwIfDroppedBothVisibleAndStrongBinding();
                    }
                });

        mActivityTestRule.loadUrl(
                navigationController, testCallbackHelperContainer, new LoadUrlParams("data:,foo"));
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        if (ContentFeatureMap.isEnabled(
                                ContentFeatureList.PROCESS_SHARING_WITH_STRICT_SITE_INSTANCES)) {
                            // If this feature is turned on all the URLs will use the same process.
                            // Verify that the process has not lost its importance now that the
                            // data: URL is also in the same process as the file: URLs.
                            Assert.assertEquals(1, connections.size());
                            connections.get(0).throwIfDroppedBothVisibleAndStrongBinding();
                        } else {
                            Assert.assertEquals(2, connections.size());
                            connections.get(1).throwIfDroppedBothVisibleAndStrongBinding();
                        }
                    }
                });
    }

    @Test
    @MediumTest
    // This test may run with --site-per-process or AndroidWarmUpSpareRendererWithTimeout, which
    // also enables a feature to maintain a spare renderer process.
    // The spare process interferes with assertions on the number of process connections in this
    // test, so disable it.
    @CommandLineFlags.Add({
        "disable-features=SpareRendererForSitePerProcess,AndroidWarmUpSpareRendererWithTimeout"
    })
    public void testIntentionalKillToFreeServiceSlot() throws Throwable {
        final TestChildProcessConnectionFactory factory = new TestChildProcessConnectionFactory();
        final List<TestChildProcessConnection> connections = factory.getConnections();
        ChildProcessLauncherHelperImpl.setSandboxServicesSettingsForTesting(
                factory, 1, null /* use default service name */);
        // Doing a cross-domain navigation would need to kill the first process in order to create
        // the second process.

        ContentShellActivity activity =
                mActivityTestRule.launchContentShellWithUrlSync(
                        "content/test/data/android/vsync.html");
        NavigationController navigationController =
                mActivityTestRule.getWebContents().getNavigationController();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(activity.getActiveWebContents());

        mActivityTestRule.loadUrl(
                navigationController,
                testCallbackHelperContainer,
                new LoadUrlParams(
                        UrlUtils.getIsolatedTestFileUrl(
                                "content/test/data/android/geolocation.html")));
        mActivityTestRule.loadUrl(
                navigationController, testCallbackHelperContainer, new LoadUrlParams("data:,foo"));

        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        if (ContentFeatureMap.isEnabled(
                                ContentFeatureList.PROCESS_SHARING_WITH_STRICT_SITE_INSTANCES)) {
                            // If this feature is turned on all the URLs will use the same process
                            // and this test will not observe any kills.
                            Assert.assertEquals(1, connections.size());
                            Assert.assertFalse(connections.get(0).isKilledByUs());
                        } else {
                            // The file: URLs and data: URL are expected to be in different
                            // processes and the data: URL is expected to kill the process used
                            // for the file:
                            // URLs.
                            // Note: The default SiteInstance process model also follows this path
                            // because
                            // file: URLs are not allowed in the default SiteInstance process while
                            // data:
                            // URLs are.
                            Assert.assertEquals(2, connections.size());
                            Assert.assertTrue(connections.get(0).isKilledByUs());
                        }
                    }
                });
    }

    private static class CrashOnLaunchChildProcessConnection extends TestChildProcessConnection {
        private boolean mCrashServiceCalled;
        private final CountDownLatch mDisconnectedLatch = new CountDownLatch(1);
        // Arguments to setupConnection
        private Bundle mConnectionBundle;
        private List<IBinder> mClientInterfaces;
        private IBinder mBinderBox;
        private ConnectionCallback mConnectionCallback;

        public CrashOnLaunchChildProcessConnection(
                Context context,
                ComponentName serviceName,
                boolean bindToCaller,
                boolean bindAsExternalService,
                Bundle childProcessCommonParameters) {
            super(
                    context,
                    serviceName,
                    bindToCaller,
                    bindAsExternalService,
                    childProcessCommonParameters);
        }

        @Override
        protected void onServiceConnectedOnLauncherThread(IBinder service) {
            super.onServiceConnectedOnLauncherThread(service);
            crashServiceForTesting();
            mCrashServiceCalled = true;
            if (mConnectionBundle != null) {
                super.setupConnection(
                        mConnectionBundle,
                        mClientInterfaces,
                        mBinderBox,
                        mConnectionCallback,
                        null);
                mConnectionBundle = null;
                mClientInterfaces = null;
                mBinderBox = null;
                mConnectionCallback = null;
            }
        }

        @Override
        protected void onServiceDisconnectedOnLauncherThread() {
            super.onServiceDisconnectedOnLauncherThread();
            mDisconnectedLatch.countDown();
        }

        @Override
        public void setupConnection(
                Bundle connectionBundle,
                List<IBinder> clientInterfaces,
                IBinder binderBox,
                ConnectionCallback connectionCallback,
                ZygoteInfoCallback zygoteInfoCallback) {
            // Make sure setupConnection is called after crashServiceForTesting so that
            // setupConnection is guaranteed to fail.
            if (mCrashServiceCalled) {
                super.setupConnection(
                        connectionBundle, clientInterfaces, binderBox, connectionCallback, null);
                return;
            }
            mConnectionBundle = connectionBundle;
            mClientInterfaces = clientInterfaces;
            mBinderBox = binderBox;
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
        public ChildProcessConnection createConnection(
                Context context,
                ComponentName serviceName,
                ComponentName fallbackServiceName,
                boolean bindToCaller,
                boolean bindAsExternalService,
                Bundle serviceBundle,
                String instanceName) {
            if (mCrashConnection == null) {
                mCrashConnection =
                        new CrashOnLaunchChildProcessConnection(
                                context,
                                serviceName,
                                bindToCaller,
                                bindAsExternalService,
                                serviceBundle);
                return mCrashConnection;
            }
            return super.createConnection(
                    context,
                    serviceName,
                    fallbackServiceName,
                    bindToCaller,
                    bindAsExternalService,
                    serviceBundle,
                    instanceName);
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
                factory, 2, null /* use default service name */);

        // Load url which should fail.
        String url = UrlUtils.getIsolatedTestFileUrl("content/test/data/android/title1.html");
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl(url);

        // Poll until connection is allocated, then wait until connection is disconnected.
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                                () -> factory.getCrashConnection() != null),
                "The connection wasn't established.");
        CrashOnLaunchChildProcessConnection crashConnection =
                ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                        () -> factory.getCrashConnection());
        crashConnection.waitForDisconnect();

        // Load a new URL and make sure everything is ok.
        NavigationController navigationController =
                mActivityTestRule.getWebContents().getNavigationController();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(activity.getActiveWebContents());
        mActivityTestRule.loadUrl(
                navigationController,
                testCallbackHelperContainer,
                new LoadUrlParams(
                        UrlUtils.getIsolatedTestFileUrl(
                                "content/test/data/android/geolocation.html")));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        Assert.assertTrue(factory.getConnections().size() > 0);
    }
}
