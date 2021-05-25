// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.AccessibilitySnapshotCallback;
import org.chromium.content_public.browser.AccessibilitySnapshotNode;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

/**
 * Accessibility snapshot tests for Assist feature.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AccessibilitySnapshotTest {
    private static final double ASSERTION_DELTA = 0;

    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static class AccessibilityCallbackHelper extends CallbackHelper {
        private AccessibilitySnapshotNode mRoot;

        public void notifyCalled(AccessibilitySnapshotNode root) {
            mRoot = root;
            super.notifyCalled();
        }

        public AccessibilitySnapshotNode getValue() {
            return mRoot;
        }
    }

    private AccessibilitySnapshotNode receiveAccessibilitySnapshot(String data, String js)
            throws Throwable {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        if (js != null) {
            JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    mActivityTestRule.getWebContents(), js);
        }

        final AccessibilityCallbackHelper callbackHelper = new AccessibilityCallbackHelper();
        final AccessibilitySnapshotCallback callback = new AccessibilitySnapshotCallback() {
            @Override
            public void onAccessibilitySnapshot(AccessibilitySnapshotNode root) {
                callbackHelper.notifyCalled(root);
            }
        };
        // read the callbackcount before executing the call on UI thread, since it may
        // synchronously complete.
        final int callbackCount = callbackHelper.getCallCount();
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getWebContents().requestAccessibilitySnapshot(callback);
            }
        });
        callbackHelper.waitForCallback(callbackCount);
        return callbackHelper.getValue();
    }

    private String getSelectionScript(String node1, int start, String node2, int end) {
        return "var element1 = document.getElementById('" + node1 + "');"
                + "var node1 = element1.childNodes.item(0);"
                + "var range=document.createRange();"
                + "range.setStart(node1," + start + ");"
                + "var element2 = document.getElementById('" + node2 + "');"
                + "var node2 = element2.childNodes.item(0);"
                + "range.setEnd(node2," + end + ");"
                + "var selection=window.getSelection();"
                + "selection.removeAllRanges();"
                + "selection.addRange(range);";
    }

    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshotOneCharacterSelection() throws Throwable {
        final String data = "<html><body><b id='node'>foo</b></body></html>";

        AccessibilitySnapshotNode root =
                receiveAccessibilitySnapshot(data, getSelectionScript("node", 0, "node", 1));
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        Assert.assertEquals("foo", grandchild.text);
        Assert.assertEquals(0, grandchild.startSelection);
        Assert.assertEquals(1, grandchild.endSelection);
    }

    @Test

    @SmallTest
    public void testRequestAccessibilitySnapshotOneNodeSelection() throws Throwable {
        final String data = "<html><body><b id='node'>foo</b></body></html>";

        AccessibilitySnapshotNode root =
                receiveAccessibilitySnapshot(data, getSelectionScript("node", 0, "node", 3));
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        Assert.assertEquals("foo", grandchild.text);
        Assert.assertEquals(0, grandchild.startSelection);
        Assert.assertEquals(3, grandchild.endSelection);
    }

    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshotSubsequentNodeSelection() throws Throwable {
        final String data = "<html><body><b id='node1'>foo</b><b id='node2'>bar</b></body></html>";

        AccessibilitySnapshotNode root =
                receiveAccessibilitySnapshot(data, getSelectionScript("node1", 1, "node2", 1));
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        Assert.assertEquals("foo", grandchild.text);
        Assert.assertEquals(1, grandchild.startSelection);
        Assert.assertEquals(3, grandchild.endSelection);
        grandchild = child.children.get(1);
        Assert.assertEquals("bar", grandchild.text);
        Assert.assertEquals(0, grandchild.startSelection);
        Assert.assertEquals(1, grandchild.endSelection);
    }

    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshotMultiNodeSelection() throws Throwable {
        final String data =
                "<html><body><b id='node1'>foo</b><b>middle</b><b id='node2'>bar</b></body></html>";

        AccessibilitySnapshotNode root =
                receiveAccessibilitySnapshot(data, getSelectionScript("node1", 1, "node2", 1));
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        Assert.assertEquals("foo", grandchild.text);
        Assert.assertEquals(1, grandchild.startSelection);
        Assert.assertEquals(3, grandchild.endSelection);
        grandchild = child.children.get(1);
        Assert.assertEquals("middle", grandchild.text);
        Assert.assertEquals(0, grandchild.startSelection);
        Assert.assertEquals(6, grandchild.endSelection);
        grandchild = child.children.get(2);
        Assert.assertEquals("bar", grandchild.text);
        Assert.assertEquals(0, grandchild.startSelection);
        Assert.assertEquals(1, grandchild.endSelection);
    }

    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshotInputSelection() throws Throwable {
        final String data = "<html><body><input id='input' value='Hello, world'></body></html>";
        final String js = "var input = document.getElementById('input');"
                + "input.select();"
                + "input.selectionStart = 0;"
                + "input.selectionEnd = 5;";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, js);
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        Assert.assertEquals("Hello, world", grandchild.text);
        Assert.assertEquals(0, grandchild.startSelection);
        Assert.assertEquals(5, grandchild.endSelection);
    }

    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshotPasswordField() throws Throwable {
        final String data =
                "<html><body><input id='input' type='password' value='foo'></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        Assert.assertEquals("•••", grandchild.text);
    }
}
