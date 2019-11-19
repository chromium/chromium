// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.AccessibilitySnapshotCallback;
import org.chromium.content_public.browser.AccessibilitySnapshotNode;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.UseZoomForDSFPolicy;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.ExecutionException;

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

    private double cssToPixel(double css) {
        double zoomFactor = 0;
        try {
            zoomFactor = TestThreadUtils.runOnUiThreadBlocking(() -> {
                Coordinates coord = Coordinates.createFor(mActivityTestRule.getWebContents());
                return coord.getDeviceScaleFactor();
            });
        } catch (ExecutionException ex) {
            Assert.fail("Unexpected ExecutionException");
        }
        return Math.ceil(zoomFactor * css);
    }

    /**
     * Verifies that AX tree is returned.
     */
    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshot() throws Throwable {
        final String data = "<button>Click</button>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        Assert.assertEquals(1, child.children.size());
        Assert.assertEquals("", child.text);
        AccessibilitySnapshotNode grandChild = child.children.get(0);
        Assert.assertEquals(1, grandChild.children.size());
        Assert.assertEquals("Click", grandChild.text);
    }

    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshotColors() throws Throwable {
        final String data = "<p style=\"color:#123456;background:#abcdef\">color</p>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        Assert.assertEquals("color", child.text);
        Assert.assertTrue(child.hasStyle);
        Assert.assertEquals("ff123456", Integer.toHexString(child.color));
        Assert.assertEquals("ffabcdef", Integer.toHexString(child.bgcolor));
    }

    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshotFontSize() throws Throwable {
        final String data = "<html><head><style> "
                + "    p { font-size:16px; transform: scale(2); }"
                + "    </style></head><body><p>foo</p></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        Assert.assertTrue(child.hasStyle);
        Assert.assertEquals("foo", child.text);

        // The font size should take the scale into account.
        double expected = UseZoomForDSFPolicy.isUseZoomForDSFEnabled() ? cssToPixel(32.0) : 32.0;
        Assert.assertEquals(expected, child.textSize, 1.0);
    }

    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshotStyles() throws Throwable {
        final String data = "<html><head><style> "
                + "    body { font: italic bold 12px Courier; }"
                + "    </style></head><body><p>foo</p></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        Assert.assertEquals("foo", child.text);
        Assert.assertTrue(child.hasStyle);
        Assert.assertTrue(child.bold);
        Assert.assertTrue(child.italic);
        Assert.assertFalse(child.lineThrough);
        Assert.assertFalse(child.underline);
    }

    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshotStrongStyle() throws Throwable {
        final String data = "<html><body><p>foo</p><p><strong>bar</strong></p></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        Assert.assertEquals(2, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child1 = root.children.get(0);
        Assert.assertEquals("foo", child1.text);
        Assert.assertTrue(child1.hasStyle);
        Assert.assertFalse(child1.bold);
        AccessibilitySnapshotNode child2 = root.children.get(1);
        AccessibilitySnapshotNode child2child = child2.children.get(0);
        Assert.assertEquals("bar", child2child.text);
        Assert.assertEquals(child1.textSize, child2child.textSize, ASSERTION_DELTA);
        Assert.assertTrue(child2child.bold);
    }

    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshotItalicStyle() throws Throwable {
        final String data = "<html><body><i>foo</i></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        Assert.assertEquals("foo", grandchild.text);
        Assert.assertTrue(grandchild.hasStyle);
        Assert.assertTrue(grandchild.italic);
    }

    @Test
    @SmallTest
    public void testRequestAccessibilitySnapshotBoldStyle() throws Throwable {
        final String data = "<html><body><b>foo</b></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        Assert.assertEquals(1, root.children.size());
        Assert.assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        Assert.assertEquals("foo", grandchild.text);
        Assert.assertTrue(grandchild.hasStyle);
        Assert.assertTrue(grandchild.bold);
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
