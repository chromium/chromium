// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.webkit.JavascriptInterface;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.JavaBridgeActivityTestRule.Controller;

/**
 * Part of the test suite for the Java Bridge. This class tests the general use of arrays.
 *
 * <p>The conversions should follow http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_CONVERSIONS.
 * Places in which the implementation differs from the spec are marked with LIVECONNECT_COMPLIANCE.
 * FIXME: Consider making our implementation more compliant, if it will not break
 * backwards-compatibility. See b/4408210.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(JavaBridgeActivityTestRule.BATCH)
public class JavaBridgeArrayTest {
    @Rule public JavaBridgeActivityTestRule mActivityTestRule = new JavaBridgeActivityTestRule();

    private static class TestObject extends Controller {
        private boolean mBooleanValue;
        private int mIntValue;
        private String mStringValue;

        private int[] mIntArray;
        private int[][] mIntIntArray;

        private boolean mWasArrayMethodCalled;

        @JavascriptInterface
        public synchronized void setBooleanValue(boolean x) {
            mBooleanValue = x;
            notifyResultIsReady();
        }

        @JavascriptInterface
        public synchronized void setIntValue(int x) {
            mIntValue = x;
            notifyResultIsReady();
        }

        @JavascriptInterface
        public synchronized void setStringValue(String x) {
            mStringValue = x;
            notifyResultIsReady();
        }

        public synchronized boolean waitForBooleanValue() {
            waitForResult();
            return mBooleanValue;
        }

        public synchronized int waitForIntValue() {
            waitForResult();
            return mIntValue;
        }

        public synchronized String waitForStringValue() {
            waitForResult();
            return mStringValue;
        }

        @JavascriptInterface
        public synchronized void setIntArray(int[] x) {
            mIntArray = x;
            notifyResultIsReady();
        }

        @JavascriptInterface
        public synchronized void setIntIntArray(int[][] x) {
            mIntIntArray = x;
            notifyResultIsReady();
        }

        public synchronized int[] waitForIntArray() {
            waitForResult();
            return mIntArray;
        }

        public synchronized int[][] waitForIntIntArray() {
            waitForResult();
            return mIntIntArray;
        }

        @JavascriptInterface
        public synchronized int[] arrayMethod() {
            mWasArrayMethodCalled = true;
            return new int[] {42, 43, 44};
        }

        public synchronized boolean wasArrayMethodCalled() {
            return mWasArrayMethodCalled;
        }
    }

    private TestObject mTestObject;

    @Before
    public void setUp() {
        mTestObject = new TestObject();
        mActivityTestRule.injectObjectAndReload(mTestObject, "testObject");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testArrayLength() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setIntArray([42, 43, 44]);");
        int[] result = mTestObject.waitForIntArray();
        Assert.assertEquals(3, result.length);
        Assert.assertEquals(42, result[0]);
        Assert.assertEquals(43, result[1]);
        Assert.assertEquals(44, result[2]);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNull() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setIntArray(null);");
        Assert.assertNull(mTestObject.waitForIntArray());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassUndefined() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setIntArray(undefined);");
        Assert.assertNull(mTestObject.waitForIntArray());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassEmptyArray() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setIntArray([]);");
        Assert.assertEquals(0, mTestObject.waitForIntArray().length);
    }

    // Note that this requires being able to pass a string from JavaScript to
    // Java.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassArrayToStringMethod() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should call toString() on array.
        mActivityTestRule.executeJavaScript("testObject.setStringValue([42, 42, 42]);");
        Assert.assertEquals("undefined", mTestObject.waitForStringValue());
    }

    // Note that this requires being able to pass an integer from JavaScript to
    // Java.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassArrayToNonStringNonArrayMethod() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should raise JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setIntValue([42, 42, 42]);");
        Assert.assertEquals(0, mTestObject.waitForIntValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNonArrayToArrayMethod() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should raise JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setIntArray(42);");
        Assert.assertNull(mTestObject.waitForIntArray());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testObjectWithLengthProperty() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setIntArray({length: 3, 1: 42});");
        int[] result = mTestObject.waitForIntArray();
        Assert.assertEquals(3, result.length);
        Assert.assertEquals(0, result[0]);
        Assert.assertEquals(42, result[1]);
        Assert.assertEquals(0, result[2]);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testNonNumericLengthProperty() throws Throwable {
        // LIVECONNECT_COMPLIANCE: This should not count as an array, so we
        // should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setIntArray({length: \"foo\"});");
        Assert.assertNull(mTestObject.waitForIntArray());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testLengthOutOfBounds() throws Throwable {
        // LIVECONNECT_COMPLIANCE: This should not count as an array, so we
        // should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setIntArray({length: -1});");
        Assert.assertNull(mTestObject.waitForIntArray());

        // LIVECONNECT_COMPLIANCE: This should not count as an array, so we
        // should raise a JavaScript exception.
        long length = Integer.MAX_VALUE + 1L;
        mActivityTestRule.executeJavaScript("testObject.setIntArray({length: " + length + "});");
        Assert.assertNull(mTestObject.waitForIntArray());

        // LIVECONNECT_COMPLIANCE: This should not count as an array, so we
        // should raise a JavaScript exception.
        length = Integer.MAX_VALUE + 1L - Integer.MIN_VALUE + 1L;
        mActivityTestRule.executeJavaScript("testObject.setIntArray({length: " + length + "});");
        Assert.assertNull(mTestObject.waitForIntArray());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testSparseArray() throws Throwable {
        mActivityTestRule.executeJavaScript(
                "var x = [42, 43]; x[3] = 45; testObject.setIntArray(x);");
        int[] result = mTestObject.waitForIntArray();
        Assert.assertEquals(4, result.length);
        Assert.assertEquals(42, result[0]);
        Assert.assertEquals(43, result[1]);
        Assert.assertEquals(0, result[2]);
        Assert.assertEquals(45, result[3]);
    }

    // Note that this requires being able to pass a boolean from JavaScript to
    // Java.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMethodReturningArrayNotCalled() throws Throwable {
        // We don't invoke methods which return arrays, but note that no
        // exception is raised.
        // LIVECONNECT_COMPLIANCE: Should call method and convert result to
        // JavaScript array.
        mActivityTestRule.executeJavaScript(
                "testObject.setBooleanValue(undefined === testObject.arrayMethod())");
        Assert.assertTrue(mTestObject.waitForBooleanValue());
        Assert.assertFalse(mTestObject.wasArrayMethodCalled());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMultiDimensionalArrayMethod() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should handle multi-dimensional arrays.
        mActivityTestRule.executeJavaScript("testObject.setIntIntArray([ [42, 43], [44, 45] ]);");
        Assert.assertNull(mTestObject.waitForIntIntArray());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassMultiDimensionalArray() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should handle multi-dimensional arrays.
        mActivityTestRule.executeJavaScript("testObject.setIntArray([ [42, 43], [44, 45] ]);");
        int[] result = mTestObject.waitForIntArray();
        Assert.assertEquals(2, result.length);
        Assert.assertEquals(0, result[0]);
        Assert.assertEquals(0, result[1]);
    }

    // Verify that ArrayBuffers are not converted into arrays when passed to Java.
    // The LiveConnect spec doesn't mention ArrayBuffers, so it doesn't seem to
    // be a compliance issue.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassArrayBuffer() throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(16);");
        mActivityTestRule.executeJavaScript("testObject.setIntArray(buffer);");
        Assert.assertNull(mTestObject.waitForIntArray());
    }

    // Verify that ArrayBufferViews are not converted into arrays when passed to Java.
    // The LiveConnect spec doesn't mention ArrayBufferViews, so it doesn't seem to
    // be a compliance issue.
    // Here, a DataView is used as an ArrayBufferView instance (since the latter is
    // an interface and can't be instantiated directly). See also JavaBridgeArrayCoercionTest
    // for typed arrays (that also subclass ArrayBufferView) tests.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassDataView() throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(16);");
        mActivityTestRule.executeJavaScript("testObject.setIntArray(new DataView(buffer));");
        Assert.assertNull(mTestObject.waitForIntArray());
    }
}
