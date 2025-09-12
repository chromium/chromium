// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;
import org.chromium.content_public.browser.BrowserStartupController.StartupMetrics;

/** Test of BrowserStartupController */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class BrowserStartupControllerTest {
    private TestBrowserStartupController mController;

    private static class TestBrowserStartupController extends BrowserStartupControllerImpl {
        private int mStartupResult;
        private boolean mLibraryLoadSucceeds;
        private int mMinimalBrowserLaunchCounter;
        private int mFullBrowserLaunchCounter;
        private boolean mMinimalBrowserStarted;
        private boolean mFlushStartupTasksCalled;
        private boolean mContentStartInClientCall;
        private boolean mStartupTasksInClientCall;

        @Override
        void prepareToStartBrowserProcess(boolean singleProcess, final Runnable deferrableTask) {
            if (!mLibraryLoadSucceeds) {
                throw new ProcessInitException(LoaderErrors.NATIVE_LIBRARY_LOAD_FAILED);
            }
            if (deferrableTask != null) {
                // Post to the UI thread to emulate what would happen in a real scenario.
                PostTask.postTask(TaskTraits.UI_STARTUP, deferrableTask);
            }
        }

        private TestBrowserStartupController() {}

        @Override
        void recordStartupUma() {}

        @Override
        int contentMainStart(boolean startMinimalBrowser) {
            mContentStartInClientCall = mIsInClientCall;
            if (startMinimalBrowser) {
                mMinimalBrowserLaunchCounter++;
            } else {
                mFullBrowserLaunchCounter++;
            }
            return kickOffStartup(startMinimalBrowser);
        }

        @Override
        void flushStartupTasks() {
            mStartupTasksInClientCall = mIsInClientCall;
            assert mFullBrowserLaunchCounter > 0;
            mFlushStartupTasksCalled = true;
            BrowserStartupControllerImpl.browserStartupComplete(
                    mStartupResult,
                    /* longestDurationOfPostedStartupTasksMs= */ 0,
                    /* totalDurationOfPostedStartupTasksMs= */ 0);
        }

        private int kickOffStartup(boolean startMinimalBrowser) {
            if (!mMinimalBrowserStarted) {
                BrowserStartupControllerImpl.minimalBrowserStartupComplete();
                mMinimalBrowserStarted = true;
            }
            if (!startMinimalBrowser) {
                BrowserStartupControllerImpl.browserStartupComplete(
                        mStartupResult,
                        /* longestDurationOfPostedStartupTasksMs= */ 0,
                        /* totalDurationOfPostedStartupTasksMs= */ 0);
            }
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
        public void onSuccess(StartupMetrics metrics) {
            assert !mHasStartupResult;
            mWasSuccess = true;
            mHasStartupResult = true;
        }

        @Override
        public void onFailure() {
            assertThat(mHasStartupResult).isFalse();
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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());
        Assert.assertFalse(
                "contentStart should have been posted.", mController.mContentStartInClientCall);

        Assert.assertFalse(
                "flushStartupTasks should not have been called.",
                mController.mFlushStartupTasksCalled);
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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback2);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback3);
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        Assert.assertFalse(
                "contentStart should have been posted.", mController.mContentStartInClientCall);
        Assert.assertFalse(
                "flushStartupTasks should not have been called.",
                mController.mFlushStartupTasksCalled);

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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback2);
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback3);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback4);
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertFalse(
                "contentStart should have been posted.", mController.mContentStartInClientCall);
        Assert.assertFalse(
                "flushStartupTasks should not have been called.",
                mController.mFlushStartupTasksCalled);

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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should have been lauched once.",
                1,
                mController.fullBrowserLaunchCounter());

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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback2);
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback3);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback4);
                });

        // Wait for posted tasks to complete.
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

        Assert.assertTrue(
                "contentStart should have been run synchronously.",
                mController.mContentStartInClientCall);
        Assert.assertTrue(
                "flushStartupTasks should have been run synchronously.",
                mController.mStartupTasksInClientCall);
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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback);
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

        // Wait for any posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        Assert.assertTrue(
                "contentStart should have been run synchronously.",
                mController.mContentStartInClientCall);
        Assert.assertTrue(
                "flushStartupTasks should have been run synchronously.",
                mController.mStartupTasksInClientCall);

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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should not have been launched a second time.",
                1,
                mController.fullBrowserLaunchCounter());

        Assert.assertTrue(
                "contentStart should have been run synchronously.",
                mController.mContentStartInClientCall);
        Assert.assertTrue(
                "flushStartupTasks should have been run synchronously.",
                mController.mStartupTasksInClientCall);

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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback);
                        Assert.fail("Browser should not have started successfully");
                    } catch (Exception e) {
                        // Exception expected, ignore.
                    }
                });

        Assert.assertEquals(
                "The browser process should not have been launched.",
                0,
                mController.fullBrowserLaunchCounter());

        // Wait for posted tasks to complete.
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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                true,
                                false,
                                false,
                                callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Callback2 will only be run when full browser is started.
                    mController.addStartupCompletedObserver(callback2);
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The minimal browser should have been launched once.",
                1,
                mController.minimalBrowserLaunchCounter());

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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback3);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.addStartupCompletedObserver(callback4);
                });

        // Wait for posted tasks to complete.
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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                true,
                                false,
                                false,
                                callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                true,
                                false,
                                false,
                                callback2);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Callback3 will only be run when full browser is started.
                    mController.addStartupCompletedObserver(callback3);
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The minimal browser should have been launched once.",
                1,
                mController.minimalBrowserLaunchCounter());

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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                true,
                                false,
                                false,
                                callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                true,
                                false,
                                false,
                                callback2);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Callback3 will only be run when full browser is started.
                    mController.addStartupCompletedObserver(callback3);
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The minimal browser should have been launched once.",
                1,
                mController.minimalBrowserLaunchCounter());

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
    @Ignore("https://crbug.com/425929053")
    // The code does not do what this test expects. The test setup was incorrect that's why it
    // wasn't caught. When a full browser startup is triggered as below, the minimal browser
    // contentStart has not started yet because the posted task has not run, leading to the full
    // browser not starting too. The request is silently discarded.
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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                true,
                                false,
                                false,
                                callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                false,
                                callback2);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Callback3 will only be run when full browser is started.
                    mController.addStartupCompletedObserver(callback3);
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The minimal browser should have been launched once.",
                1,
                mController.minimalBrowserLaunchCounter());
        // Wait for posted tasks to complete.
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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                true,
                                false,
                                false,
                                callback1);
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
        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The minimal browser should have been launched once.",
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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                true,
                                false,
                                false,
                                callback1);
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

        Assert.assertEquals(
                "The minimal browser should have been launched once.",
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
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                true,
                                false,
                                false,
                                callback1);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The minimal browser should not have been launched.",
                0,
                mController.minimalBrowserLaunchCounter());

        Assert.assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        Assert.assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        Assert.assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        Assert.assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);
    }

    @Test
    @SmallTest
    public void testAsynchronousStartupRequestWithFlushStartupTasks() {
        mController.mStartupResult = BrowserStartupControllerImpl.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback = new TestStartupCallback();

        // Kick off the asynchronous startup request.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        mController.startBrowserProcessesAsync(
                                LibraryProcessType.PROCESS_BROWSER,
                                true,
                                false,
                                false,
                                true,
                                callback);
                    } catch (Exception e) {
                        throw new AssertionError("Browser should have started successfully", e);
                    }
                });

        // Wait for posted tasks to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "The browser process should have been launched once.",
                1,
                mController.fullBrowserLaunchCounter());

        Assert.assertFalse(
                "contentStart should have been posted", mController.mContentStartInClientCall);
        Assert.assertTrue(
                "flushStartupTasks should have been called.", mController.mFlushStartupTasksCalled);
        Assert.assertFalse(
                "flushStartupTasks should have been posted.",
                mController.mStartupTasksInClientCall);

        Assert.assertTrue("Callback should have been executed.", callback.mHasStartupResult);
        Assert.assertTrue("Callback should have been a success.", callback.mWasSuccess);
    }
}
