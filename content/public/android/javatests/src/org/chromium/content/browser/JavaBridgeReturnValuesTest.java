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
 * Part of the test suite for the Java Bridge. This test checks that we correctly convert Java
 * values to JavaScript values when returning them from the methods of injected Java objects.
 *
 * <p>The conversions should follow http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_CONVERSIONS.
 * Places in which the implementation differs from the spec are marked with LIVECONNECT_COMPLIANCE.
 * FIXME: Consider making our implementation more compliant, if it will not break
 * backwards-compatibility. See b/4408210.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(JavaBridgeActivityTestRule.BATCH)
public class JavaBridgeReturnValuesTest {
    @Rule public JavaBridgeActivityTestRule mActivityTestRule = new JavaBridgeActivityTestRule();

    // An instance of this class is injected into the page to test returning
    // Java values to JavaScript.
    private static class TestObject extends Controller {
        private String mStringResult;
        private boolean mBooleanResult;

        // These four methods are used to control the test.
        @JavascriptInterface
        public synchronized void setStringResult(String x) {
            mStringResult = x;
            notifyResultIsReady();
        }

        public synchronized String waitForStringResult() {
            waitForResult();
            return mStringResult;
        }

        @JavascriptInterface
        public synchronized void setBooleanResult(boolean x) {
            mBooleanResult = x;
            notifyResultIsReady();
        }

        public synchronized boolean waitForBooleanResult() {
            waitForResult();
            return mBooleanResult;
        }

        @JavascriptInterface
        public boolean getBooleanValue() {
            return true;
        }

        @JavascriptInterface
        public byte getByteValue() {
            return 42;
        }

        @JavascriptInterface
        public char getCharValue() {
            return '\u002A';
        }

        @JavascriptInterface
        public short getShortValue() {
            return 42;
        }

        @JavascriptInterface
        public int getIntValue() {
            return 42;
        }

        @JavascriptInterface
        public long getLongValue() {
            return 42L;
        }

        @JavascriptInterface
        public float getFloatValue() {
            return 42.1f;
        }

        @JavascriptInterface
        public float getFloatValueNoDecimal() {
            return 42.0f;
        }

        @JavascriptInterface
        public double getDoubleValue() {
            return 42.1;
        }

        @JavascriptInterface
        public double getDoubleValueNoDecimal() {
            return 42.0;
        }

        @JavascriptInterface
        public String getStringValue() {
            return "foo";
        }

        @JavascriptInterface
        public String getEmptyStringValue() {
            return "";
        }

        @JavascriptInterface
        public String getNullStringValue() {
            return null;
        }

        @JavascriptInterface
        public Object getObjectValue() {
            return new Object();
        }

        @JavascriptInterface
        public Object getNullObjectValue() {
            return null;
        }

        @JavascriptInterface
        public CustomType getCustomTypeValue() {
            return new CustomType();
        }

        @JavascriptInterface
        public void getVoidValue() {}
    }

    // A custom type used when testing passing objects.
    private static class CustomType {}

    TestObject mTestObject;

    @Before
    public void setUp() {
        mTestObject = new TestObject();
        mActivityTestRule.injectObjectAndReload(mTestObject, "testObject");
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    protected String executeJavaScriptAndGetStringResult(String script) throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setStringResult(" + script + ");");
        return mTestObject.waitForStringResult();
    }

    // Note that this requires that we can pass a JavaScript boolean to Java.
    private boolean executeJavaScriptAndGetBooleanResult(String script) throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setBooleanResult(" + script + ");");
        return mTestObject.waitForBooleanResult();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMethodReturnTypes() throws Throwable {
        Assert.assertEquals(
                "boolean",
                executeJavaScriptAndGetStringResult("typeof testObject.getBooleanValue()"));
        Assert.assertEquals(
                "number", executeJavaScriptAndGetStringResult("typeof testObject.getByteValue()"));
        // char values are returned to JavaScript as numbers.
        Assert.assertEquals(
                "number", executeJavaScriptAndGetStringResult("typeof testObject.getCharValue()"));
        Assert.assertEquals(
                "number", executeJavaScriptAndGetStringResult("typeof testObject.getShortValue()"));
        Assert.assertEquals(
                "number", executeJavaScriptAndGetStringResult("typeof testObject.getIntValue()"));
        Assert.assertEquals(
                "number", executeJavaScriptAndGetStringResult("typeof testObject.getLongValue()"));
        Assert.assertEquals(
                "number", executeJavaScriptAndGetStringResult("typeof testObject.getFloatValue()"));
        Assert.assertEquals(
                "number",
                executeJavaScriptAndGetStringResult("typeof testObject.getFloatValueNoDecimal()"));
        Assert.assertEquals(
                "number",
                executeJavaScriptAndGetStringResult("typeof testObject.getDoubleValue()"));
        Assert.assertEquals(
                "number",
                executeJavaScriptAndGetStringResult("typeof testObject.getDoubleValueNoDecimal()"));
        Assert.assertEquals(
                "string",
                executeJavaScriptAndGetStringResult("typeof testObject.getStringValue()"));
        Assert.assertEquals(
                "string",
                executeJavaScriptAndGetStringResult("typeof testObject.getEmptyStringValue()"));
        // LIVECONNECT_COMPLIANCE: This should have type object.
        Assert.assertEquals(
                "undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.getNullStringValue()"));
        Assert.assertEquals(
                "object",
                executeJavaScriptAndGetStringResult("typeof testObject.getObjectValue()"));
        Assert.assertEquals(
                "object",
                executeJavaScriptAndGetStringResult("typeof testObject.getNullObjectValue()"));
        Assert.assertEquals(
                "object",
                executeJavaScriptAndGetStringResult("typeof testObject.getCustomTypeValue()"));
        Assert.assertEquals(
                "undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.getVoidValue()"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMethodReturnValues() throws Throwable {
        // We do the string comparison in JavaScript, to avoid relying on the
        // coercion algorithm from JavaScript to Java.
        Assert.assertTrue(executeJavaScriptAndGetBooleanResult("testObject.getBooleanValue()"));
        Assert.assertTrue(executeJavaScriptAndGetBooleanResult("42 === testObject.getByteValue()"));
        // char values are returned to JavaScript as numbers.
        Assert.assertTrue(executeJavaScriptAndGetBooleanResult("42 === testObject.getCharValue()"));
        Assert.assertTrue(
                executeJavaScriptAndGetBooleanResult("42 === testObject.getShortValue()"));
        Assert.assertTrue(executeJavaScriptAndGetBooleanResult("42 === testObject.getIntValue()"));
        Assert.assertTrue(executeJavaScriptAndGetBooleanResult("42 === testObject.getLongValue()"));
        Assert.assertTrue(
                executeJavaScriptAndGetBooleanResult(
                        "Math.abs(42.1 - testObject.getFloatValue()) < 0.001"));
        Assert.assertTrue(
                executeJavaScriptAndGetBooleanResult(
                        "42.0 === testObject.getFloatValueNoDecimal()"));
        Assert.assertTrue(
                executeJavaScriptAndGetBooleanResult(
                        "Math.abs(42.1 - testObject.getDoubleValue()) < 0.001"));
        Assert.assertTrue(
                executeJavaScriptAndGetBooleanResult(
                        "42.0 === testObject.getDoubleValueNoDecimal()"));
        Assert.assertEquals(
                "foo", executeJavaScriptAndGetStringResult("testObject.getStringValue()"));
        Assert.assertEquals(
                "", executeJavaScriptAndGetStringResult("testObject.getEmptyStringValue()"));
        Assert.assertTrue(
                executeJavaScriptAndGetBooleanResult("undefined === testObject.getVoidValue()"));
    }
}
