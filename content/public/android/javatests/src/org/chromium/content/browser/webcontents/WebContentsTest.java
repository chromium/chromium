// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.os.Bundle;
import android.os.Parcel;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ChildProcessLauncherHelperImpl;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_shell.Shell;
import org.chromium.content_shell_apk.ChildProcessLauncherTestUtils;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Test various Java WebContents specific features.
 * TODO(dtrainor): Add more testing for the WebContents methods.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebContentsTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String TEST_URL_1 = "about://blank";
    private static final String TEST_URL_2 = UrlUtils.encodeHtmlDataUri("<html>1</html>");
    private static final String WEB_CONTENTS_KEY = "WEBCONTENTSKEY";
    private static final String PARCEL_STRING_TEST_DATA = "abcdefghijklmnopqrstuvwxyz";

    /**
     * Check that {@link WebContents#isDestroyed()} works as expected.
     * TODO(dtrainor): Test this using {@link WebContents#destroy()} instead once it is possible to
     * build a {@link WebContents} directly in the content/ layer.
     *
     * @throws ExecutionException
     */
    @Test
    @SmallTest
    public void testWebContentsIsDestroyedMethod() throws ExecutionException {
        final ContentShellActivity activity =
                mActivityTestRule.launchContentShellWithUrl(TEST_URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        WebContents webContents = activity.getActiveWebContents();

        Assert.assertFalse(
                "WebContents incorrectly marked as destroyed", isWebContentsDestroyed(webContents));

        // Launch a new shell.
        Shell originalShell = activity.getActiveShell();
        mActivityTestRule.loadNewShell(TEST_URL_1);
        Assert.assertNotSame("New shell not created", activity.getActiveShell(), originalShell);

        Assert.assertTrue("WebContents incorrectly marked as not destroyed",
                isWebContentsDestroyed(webContents));
    }

    /**
     * Check that it is possible to serialize and deserialize a WebContents object through Parcels.
     *
     */
    @Test
    @SmallTest
    public void testWebContentsSerializeDeserializeInParcel() {
        mActivityTestRule.launchContentShellWithUrl(TEST_URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        WebContents webContents = mActivityTestRule.getWebContents();

        Parcel parcel = Parcel.obtain();

        try {
            // Serialize the WebContents.
            parcel.writeParcelable(webContents, 0);

            // Read back the WebContents.
            parcel.setDataPosition(0);
            WebContents deserializedWebContents = parcel.readParcelable(
                    WebContents.class.getClassLoader());

            // Make sure they're equal.
            Assert.assertEquals(
                    "Deserialized object does not match", webContents, deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that it is possible to serialize and deserialize a WebContents object through Bundles.
     */
    @Test
    @SmallTest
    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("ParcelClassLoader")
    public void testWebContentsSerializeDeserializeInBundle() {
        mActivityTestRule.launchContentShellWithUrl(TEST_URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        WebContents webContents = mActivityTestRule.getWebContents();

        // Use a parcel to force the Bundle to actually serialize and deserialize, otherwise it can
        // cache the WebContents object.
        Parcel parcel = Parcel.obtain();

        try {
            // Create a bundle and put the WebContents in it.
            Bundle bundle = new Bundle();
            bundle.putParcelable(WEB_CONTENTS_KEY, webContents);

            // Serialize the Bundle.
            parcel.writeBundle(bundle);

            // Read back the Bundle.
            parcel.setDataPosition(0);
            Bundle deserializedBundle = parcel.readBundle();

            // Read back the WebContents.
            deserializedBundle.setClassLoader(WebContents.class.getClassLoader());
            WebContents deserializedWebContents =
                    deserializedBundle.getParcelable(WEB_CONTENTS_KEY);

            // Make sure they're equal.
            Assert.assertEquals(
                    "Deserialized object does not match", webContents, deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that it is possible to serialize and deserialize a WebContents object through Intents.
     */
    @Test
    @SmallTest
    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("ParcelClassLoader")
    public void testWebContentsSerializeDeserializeInIntent() {
        mActivityTestRule.launchContentShellWithUrl(TEST_URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        WebContents webContents = mActivityTestRule.getWebContents();

        // Use a parcel to force the Intent to actually serialize and deserialize, otherwise it can
        // cache the WebContents object.
        Parcel parcel = Parcel.obtain();

        try {
            // Create an Intent and put the WebContents in it.
            Intent intent = new Intent();
            intent.putExtra(WEB_CONTENTS_KEY, webContents);

            // Serialize the Intent
            parcel.writeParcelable(intent, 0);

            // Read back the Intent.
            parcel.setDataPosition(0);
            Intent deserializedIntent = parcel.readParcelable(null);

            // Read back the WebContents.
            deserializedIntent.setExtrasClassLoader(WebContents.class.getClassLoader());
            WebContents deserializedWebContents =
                    (WebContents) deserializedIntent.getParcelableExtra(WEB_CONTENTS_KEY);

            // Make sure they're equal.
            Assert.assertEquals(
                    "Deserialized object does not match", webContents, deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that attempting to deserialize a WebContents object from a Parcel from another process
     * instance fails.
     */
    @Test
    @SmallTest
    public void testWebContentsFailDeserializationAcrossProcessBoundary() {
        mActivityTestRule.launchContentShellWithUrl(TEST_URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        WebContents webContents = mActivityTestRule.getWebContents();

        Parcel parcel = Parcel.obtain();

        try {
            // Serialize the WebContents.
            parcel.writeParcelable(webContents, 0);

            // Invalidate all serialized WebContents.
            WebContentsImpl.invalidateSerializedWebContentsForTesting();

            // Try to read back the WebContents.
            parcel.setDataPosition(0);
            WebContents deserializedWebContents = parcel.readParcelable(
                    WebContents.class.getClassLoader());

            // Make sure we weren't able to deserialize the WebContents.
            Assert.assertNull("Unexpectedly deserialized a WebContents", deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that serializing a destroyed WebContents always results in a null deserialized
     * WebContents.
     * @throws ExecutionException
     */
    @Test
    @SmallTest
    public void testSerializingADestroyedWebContentsDoesNotDeserialize() throws ExecutionException {
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl(TEST_URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        WebContents webContents = activity.getActiveWebContents();
        mActivityTestRule.loadNewShell(TEST_URL_1);

        Assert.assertTrue("WebContents not destroyed", isWebContentsDestroyed(webContents));

        Parcel parcel = Parcel.obtain();

        try {
            // Serialize the WebContents.
            parcel.writeParcelable(webContents, 0);

            // Try to read back the WebContents.
            parcel.setDataPosition(0);
            WebContents deserializedWebContents = parcel.readParcelable(
                    WebContents.class.getClassLoader());

            // Make sure we weren't able to deserialize the WebContents.
            Assert.assertNull(
                    "Unexpectedly deserialized a destroyed WebContents", deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that destroying a WebContents after serializing it always results in a null
     * deserialized WebContents.
     * @throws ExecutionException
     */
    @Test
    @SmallTest
    public void testDestroyingAWebContentsAfterSerializingDoesNotDeserialize()
            throws ExecutionException {
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl(TEST_URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        WebContents webContents = activity.getActiveWebContents();

        Parcel parcel = Parcel.obtain();

        try {
            // Serialize the WebContents.
            parcel.writeParcelable(webContents, 0);

            // Destroy the WebContents.
            mActivityTestRule.loadNewShell(TEST_URL_1);
            Assert.assertTrue("WebContents not destroyed", isWebContentsDestroyed(webContents));

            // Try to read back the WebContents.
            parcel.setDataPosition(0);
            WebContents deserializedWebContents = parcel.readParcelable(
                    WebContents.class.getClassLoader());

            // Make sure we weren't able to deserialize the WebContents.
            Assert.assertNull(
                    "Unexpectedly deserialized a destroyed WebContents", deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that failing a WebContents deserialization doesn't corrupt subsequent data in the
     * Parcel.
     */
    @Test
    @SmallTest
    public void testFailedDeserializationDoesntCorruptParcel() {
        mActivityTestRule.launchContentShellWithUrl(TEST_URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        WebContents webContents = mActivityTestRule.getWebContents();

        Parcel parcel = Parcel.obtain();

        try {
            // Serialize the WebContents.
            parcel.writeParcelable(webContents, 0);

            // Serialize a String after the WebContents.
            parcel.writeString(PARCEL_STRING_TEST_DATA);

            // Invalidate all serialized WebContents.
            WebContentsImpl.invalidateSerializedWebContentsForTesting();

            // Try to read back the WebContents.
            parcel.setDataPosition(0);
            WebContents deserializedWebContents = parcel.readParcelable(
                    WebContents.class.getClassLoader());

            // Make sure we weren't able to deserialize the WebContents.
            Assert.assertNull("Unexpectedly deserialized a WebContents", deserializedWebContents);

            // Make sure we can properly deserialize the String after the WebContents.
            Assert.assertEquals("Failing to read the WebContents corrupted the parcel",
                    PARCEL_STRING_TEST_DATA, parcel.readString());
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that the main frame associated with the WebContents is not null
     * and corresponds with the test URL.
     *
     */
    @Test
    @SmallTest
    public void testWebContentsMainFrame() {
        final ContentShellActivity activity =
                mActivityTestRule.launchContentShellWithUrl(TEST_URL_2);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        final WebContents webContents = activity.getActiveWebContents();

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                RenderFrameHost frameHost = webContents.getMainFrame();

                Assert.assertNotNull(frameHost);

                Assert.assertEquals("RenderFrameHost has incorrect last committed URL", TEST_URL_2,
                        frameHost.getLastCommittedURL());

                WebContents associatedWebContents =
                        WebContentsStatics.fromRenderFrameHost(frameHost);
                Assert.assertEquals("RenderFrameHost associated with different WebContents",
                        webContents, associatedWebContents);
            }
        });
    }

    private ChildProcessConnection getSandboxedChildProcessConnection() {
        Callable<ChildProcessConnection> getConnectionCallable = () -> {
            for (ChildProcessLauncherHelperImpl process :
                    ChildProcessLauncherHelperImpl.getAllProcessesForTesting().values()) {
                ChildProcessConnection connection = process.getChildProcessConnection();
                if (connection.getServiceName().getClassName().indexOf("Sandbox") != -1) {
                    return connection;
                }
            }
            Assert.assertTrue(false);
            return null;
        };
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(getConnectionCallable);
    }

    @Test
    @SmallTest
    public void testChildProcessImportance() {
        final ContentShellActivity activity =
                mActivityTestRule.launchContentShellWithUrl(TEST_URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        final WebContents webContents = activity.getActiveWebContents();
        // Make sure visibility do not affect bindings.
        TestThreadUtils.runOnUiThreadBlocking(() -> webContents.onHide());

        final ChildProcessConnection connection = getSandboxedChildProcessConnection();
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                () -> Assert.assertFalse(connection.isModerateBindingBound()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> webContents.setImportance(ChildProcessImportance.MODERATE));
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                () -> Assert.assertTrue(connection.isModerateBindingBound()));
    }

    @Test
    @SmallTest
    public void testVisibilityControlsBinding() {
        final ContentShellActivity activity =
                mActivityTestRule.launchContentShellWithUrl(TEST_URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        final WebContents webContents = activity.getActiveWebContents();

        final ChildProcessConnection connection = getSandboxedChildProcessConnection();
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                () -> Assert.assertTrue(connection.isStrongBindingBound()));

        TestThreadUtils.runOnUiThreadBlocking(() -> webContents.onHide());
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                () -> Assert.assertFalse(connection.isStrongBindingBound()));

        TestThreadUtils.runOnUiThreadBlocking(() -> webContents.onShow());
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                () -> Assert.assertTrue(connection.isStrongBindingBound()));
    }

    private boolean isWebContentsDestroyed(final WebContents webContents) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return webContents.isDestroyed();
            }
        });
    }
}
