// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;

/** Test of BrowserStartupController */
@RunWith(BaseJUnit4ClassRunner.class)
public class BrowserStartupControllerTest {
    private TestBrowserStartupController mController;

    private static class TestBrowserStartupController extends BrowserStartupControllerImpl {
        private int mStartupResult;
        private boolean mLibraryLoadSucceeds;
        private int mMinimalBrowserLaunchCounter;
        private int mFullBrowserLaunchCounter;
        private boolean mMinimalBrowserStarted;

        @Override
        void prepareToStartBrowserProcess(boolean singleProcess, final Runnable deferrableTask) {
            if (!mLibraryLoadSucceeds) {
                throw new ProcessInitException(LoaderErrors.NATIVE_LIBRARY_LOAD_FAILED);
            }
            if (deferrableTask != null) {
                deferrableTask.run();
            }
        }

        private TestBrowserStartupController() {}

        @Override
        void recordStartupUma() {}

        @Override
        int contentMainStart(boolean startMinimalBrowser) {
            if (startMinimalBrowser) {
                mMinimalBrowserLaunchCounter++;
            } else {
                mFullBrowserLaunchCounter++;
            }
            return kickOffStartup(startMinimalBrowser);
        }

        @Override
        void flushStartupTasks() {
            assert mFullBrowserLaunchCounter > 0;
            BrowserStartupControllerImpl.browserStartupComplete(mStartupResult);
        }

        private int kickOffStartup(boolean startMinimalBrowser) {
            // Post to the UI thread to emulate what would happen in a real scenario.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    new Runnable() {
                        @Override
                        public void run() {
                            if (!mMinimalBrowserStarted) {
                                BrowserStartupControllerImpl.minimalBrowserStartupComplete();
                                mMinimalBrowserStarted = true;
                            }
                            if (!startMinimalBrowser) {
                                BrowserStartupControllerImpl.browserStartupComplete(mStartupResult);
                            }
                        }
                    });
            return mStartupResult;
        }

        private int minimalBrowserLaunchCounter() {
            return mMinimalBrowserLaunchCounter;
        }

        private int fullBrowserLaunchCounter() {
            return mFullBrowserLaunchCounter;
        }
    }

    private static class TestStartupCallback implements StartupCallback {
        private boolean mWasSuccess;
        private boolean mWasFailure;
        private boolean mHasStartupResult;

        @Override
        public void onSuccess() {
            assert !mHasStartupResult;
            mWasSuccess = true;
            mHasStartupResult = true;
        }

        @Override
        public void onFailure() {
            assert !mHasStartupResult;
            mWasFailure = true;
            mHasStartupResult = true;
        }
    }

    @Before
    public void setUp() {
        mController = new TestBrowserStartupController();
        // Setting the static singleton instance field enables more correct testing, since it is
        // is possible to call {@link BrowserStartupController#browserStartupComplete(int)} instead
        // of {@link BrowserStartupController#executeEnqueuedCallbacks(int, boolean)} directly.
        BrowserStartupControllerImpl.overrideInstanceForTest(mController);
    }

    @Test
    @SmallTest
    public void testSingleAsynchronousStartupRequest() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback = new TestStartupCallback();

        // Kick off the asynchronous startup request.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertTrue("Callback should have been executed.", callback.mHasStartupResult);
        Assert.assertTrue("Callback should have been a success.", callback.mWasSuccess);
    }

    @Test
    @SmallTest
    public void testMultipleAsynchronousStartupRequests() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();
        final TestStartupCallback callback3 = new TestStartupCallback();

        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback2);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback3);
                });

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        Assert.assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        Assert.assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        Assert.assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);
        Assert.assertTrue("Callback 3 should have been executed.", callback3.mHasStartupResult);
        Assert.assertTrue("Callback 3 should have been a success.", callback3.mWasSuccess);
    }

    @Test
    @SmallTest
    public void testConsecutiveAsynchronousStartupRequests() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();

        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback2);
                });

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        Assert.assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        Assert.assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        Assert.assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);

        final TestStartupCallback callback3 = new TestStartupCallback();
        final TestStartupCallback callback4 = new TestStartupCallback();

        // Kick off more asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback3);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback4);
                });

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertTrue("Callback 3 should have been executed.", callback3.mHasStartupResult);
        Assert.assertTrue("Callback 3 should have been a success.", callback3.mWasSuccess);
        Assert.assertTrue("Callback 4 should have been executed.", callback4.mHasStartupResult);
        Assert.assertTrue("Callback 4 should have been a success.", callback4.mWasSuccess);
    }

    @Test
    @SmallTest
    public void testSingleFailedAsynchronousStartupRequest() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_FAILURE;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback = new TestStartupCallback();

        // Kick off the asynchronous startup request.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });

        Assert.assertEquals(
                "The browser process should have been lauched once.",
                1,
                mController.fullBrowserLaunchCounter());

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertTrue("Callback should have been executed.", callback.mHasStartupResult);
        Assert.assertTrue("Callback should have been a failure.", callback.mWasFailure);
    }

    @Test
    @SmallTest
    public void testConsecutiveFailedAsynchronousStartupRequests() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_FAILURE;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();

        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback2);
                });

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        Assert.assertTrue("Callback 1 should have been a failure.", callback1.mWasFailure);
        Assert.assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        Assert.assertTrue("Callback 2 should have been a failure.", callback2.mWasFailure);

        final TestStartupCallback callback3 = new TestStartupCallback();
        final TestStartupCallback callback4 = new TestStartupCallback();

        // Kick off more asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback3);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback4);
                });

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertTrue("Callback 3 should have been executed.", callback3.mHasStartupResult);
        Assert.assertTrue("Callback 3 should have been a failure.", callback3.mWasFailure);
        Assert.assertTrue("Callback 4 should have been executed.", callback4.mHasStartupResult);
        Assert.assertTrue("Callback 4 should have been a failure.", callback4.mWasFailure);
    }

    @Test
    @SmallTest
    public void testSingleSynchronousRequest() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        // Kick off the synchronous startup.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesSync(
                                LibraryProcessType.PROCESS_BROWSER, false, false);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());
    }

    @Test
    @SmallTest
    public void testAsyncThenSyncRequests() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback = new TestStartupCallback();

        // Kick off the startups.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                    // To ensure that the async startup doesn't complete too soon we have
                    // to do both these in a since Runnable instance. This avoids the
                    // unpredictable race that happens in real situations.
                    try {
                        mController.startBrowserProcessesSync(
                                LibraryProcessType.PROCESS_BROWSER, false, true);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        Assert.assertTrue("Callback should have been executed.", callback.mHasStartupResult);
        Assert.assertTrue("Callback should have been a success.", callback.mWasSuccess);
    }

    @Test
    @SmallTest
    public void testSyncThenAsyncRequests() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback = new TestStartupCallback();

        // Do a synchronous startup first.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesSync(
                                LibraryProcessType.PROCESS_BROWSER, false, true);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        // Kick off the asynchronous startup request. This should just queue the callback.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });

        Assert.assertEquals(
                "The browser process should not have been launched a second time.",
                1,
                mController.fullBrowserLaunchCounter());

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertTrue("Callback should have been executed.", callback.mHasStartupResult);
        Assert.assertTrue("Callback should have been a success.", callback.mWasSuccess);
    }

    @Test
    @SmallTest
    public void testLibraryLoadFails() {
        mController.mLibraryLoadSucceeds = false;
        final TestStartupCallback callback = new TestStartupCallback();

        // Kick off the asynchronous startup request.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback);
                        Assert.fail("Browser should not have started successfully");
                    } catch (Exception e) {
                        // Exception expected, ignore.
                    }
                });

        Assert.assertEquals(
                "The browser process should not have been launched.",
                0,
                mController.fullBrowserLaunchCounter());

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @Test
    @SmallTest
    public void testAsynchronousStartMinimalBrowserThenStartFullBrowser() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();

        // Kick off the asynchronous startup requests to start a minimal browser.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, true, callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Callback2 will only be run when full browser is started.
                    mController.addStartupCompletedObserver(callback2);
                });

        Assert.assertEquals(
                "The service manager should have been launched once.",
                1,
                mController.minimalBrowserLaunchCounter());

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        Assert.assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        Assert.assertFalse("Callback 2 should not be executed.", callback2.mHasStartupResult);

        Assert.assertEquals(
                "The browser process should not have been launched.",
                0,
                mController.fullBrowserLaunchCounter());

        final TestStartupCallback callback3 = new TestStartupCallback();
        final TestStartupCallback callback4 = new TestStartupCallback();

        // Kick off another asynchronous startup requests to start full browser.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback3);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback4);
                });

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        Assert.assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        Assert.assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);
        Assert.assertTrue("Callback 3 should have been executed.", callback3.mHasStartupResult);
        Assert.assertTrue("Callback 3 should have been a success.", callback3.mWasSuccess);
        Assert.assertTrue("Callback 4 should have been executed.", callback4.mHasStartupResult);
        Assert.assertTrue("Callback 4 should have been a success.", callback4.mWasSuccess);
    }

    @Test
    @SmallTest
    public void testMultipleAsynchronousStartMinimalBrowserRequests() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();
        final TestStartupCallback callback3 = new TestStartupCallback();

        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, true, callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, true, callback2);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Callback3 will only be run when full browser is started.
                    mController.addStartupCompletedObserver(callback3);
                });

        Assert.assertEquals(
                "The service manager should have been launched once.",
                1,
                mController.minimalBrowserLaunchCounter());

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should not have been launched.",
                0,
                mController.fullBrowserLaunchCounter());

        Assert.assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        Assert.assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        Assert.assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        Assert.assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);
        Assert.assertFalse("Callback 3 should not be executed.", callback3.mHasStartupResult);
    }

    @Test
    @SmallTest
    public void testConsecutiveAsynchronousStartMinimalBrowserRequests() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();
        final TestStartupCallback callback3 = new TestStartupCallback();

        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, true, callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, true, callback2);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Callback3 will only be run when full browser is started.
                    mController.addStartupCompletedObserver(callback3);
                });

        Assert.assertEquals(
                "The service manager should have been launched once.",
                1,
                mController.minimalBrowserLaunchCounter());

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should not have been launched.",
                0,
                mController.fullBrowserLaunchCounter());

        Assert.assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        Assert.assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        Assert.assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        Assert.assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);
        Assert.assertFalse("Callback 3 should not be executed.", callback3.mHasStartupResult);
    }

    @Test
    @SmallTest
    public void testMultipleAsynchronousStartMinimalBrowserAndFullBrowserRequests() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();
        final TestStartupCallback callback3 = new TestStartupCallback();

        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, true, callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, false, callback2);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Callback3 will only be run when full browser is started.
                    mController.addStartupCompletedObserver(callback3);
                });

        Assert.assertEquals(
                "The service manager should have been launched once.",
                1,
                mController.minimalBrowserLaunchCounter());

        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        Assert.assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        Assert.assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        Assert.assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        Assert.assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);
        Assert.assertTrue("Callback 3 should have been executed.", callback3.mHasStartupResult);
        Assert.assertTrue("Callback 3 should have been a success.", callback3.mWasSuccess);
    }

    @Test
    @SmallTest
    public void testAsynchronousStartMinimalBrowserThenSynchronousStartFullBrowser() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Callback2 will only be run when full browser is started.
                    mController.addStartupCompletedObserver(callback2);
                });

        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, true, callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesSync(
                                LibraryProcessType.PROCESS_BROWSER, false, true);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        // Wait for callbacks to complete.
        Assert.assertEquals(
                "The service manager should have been launched once.",
                1,
                mController.minimalBrowserLaunchCounter());

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        Assert.assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        Assert.assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        Assert.assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        Assert.assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);
    }

    @Test
    @SmallTest
    public void testAsynchronousStartMinimalBrowserAlongWithSynchronousStartFullBrowser() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Callback2 will only be run when full browser is started.
                    mController.addStartupCompletedObserver(callback2);
                });
        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, true, callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }

                    try {
                        mController.startBrowserProcessesSync(
                                LibraryProcessType.PROCESS_BROWSER, false, true);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        // Wait for callbacks to complete.
        Assert.assertEquals(
                "The service manager should have been launched once.",
                1,
                mController.minimalBrowserLaunchCounter());

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        Assert.assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        Assert.assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        Assert.assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        Assert.assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);
    }

    @Test
    @SmallTest
    public void testSynchronousStartFullBrowserThenAsynchronousStartMinimalBrowser() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Callback2 will only be run when full browser is started.
                    mController.addStartupCompletedObserver(callback2);
                });
        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesSync(
                                LibraryProcessType.PROCESS_BROWSER, false, true);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER, true, true, callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        // Wait for callbacks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "The service manager should not have been launched.",
                0,
                mController.minimalBrowserLaunchCounter());

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        Assert.assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        Assert.assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        Assert.assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        Assert.assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);
    }
}
