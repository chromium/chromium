// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.SmallTest;

import dalvik.system.DexClassLoader;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.JavaBridgeActivityTestRule.Controller;

import java.io.File;

/**
 * Part of the test suite for the Java Bridge. This class tests that
 * we correctly convert JavaScript values to Java values when passing them to
 * the methods of injected Java objects.
 *
 * The conversions should follow
 * http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_CONVERSIONS. Places in
 * which the implementation differs from the spec are marked with
 * LIVECONNECT_COMPLIANCE.
 * FIXME: Consider making our implementation more compliant, if it will not
 * break backwards-compatibility. See b/4408210.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class JavaBridgeCoercionTest {
    private static final double ASSERTION_DELTA = 0;

    @Rule
    public JavaBridgeActivityTestRule mActivityTestRule =
            new JavaBridgeActivityTestRule().shouldSetUp(true);

    private static class TestObject extends Controller {
        private Object mObjectInstance;
        private CustomType mCustomTypeInstance;
        private CustomType2 mCustomType2Instance;

        private boolean mBooleanValue;
        private byte mByteValue;
        private char mCharValue;
        private short mShortValue;
        private int mIntValue;
        private long mLongValue;
        private float mFloatValue;
        private double mDoubleValue;
        private String mStringValue;
        private Object mObjectValue;
        private CustomType mCustomTypeValue;

        public TestObject() {
            mObjectInstance = new Object();
            mCustomTypeInstance = new CustomType();
            mCustomType2Instance = new CustomType2();
        }

        public Object getObjectInstance() {
            return mObjectInstance;
        }
        public CustomType getCustomTypeInstance() {
            return mCustomTypeInstance;
        }
        public CustomType2 getCustomType2Instance() {
            return mCustomType2Instance;
        }

        public synchronized void setBooleanValue(boolean x) {
            mBooleanValue = x;
            notifyResultIsReady();
        }
        public synchronized void setByteValue(byte x) {
            mByteValue = x;
            notifyResultIsReady();
        }
        public synchronized void setCharValue(char x) {
            mCharValue = x;
            notifyResultIsReady();
        }
        public synchronized void setShortValue(short x) {
            mShortValue = x;
            notifyResultIsReady();
        }
        public synchronized void setIntValue(int x) {
            mIntValue = x;
            notifyResultIsReady();
        }
        public synchronized void setLongValue(long x) {
            mLongValue = x;
            notifyResultIsReady();
        }
        public synchronized void setFloatValue(float x) {
            mFloatValue = x;
            notifyResultIsReady();
        }
        public synchronized void setDoubleValue(double x) {
            mDoubleValue = x;
            notifyResultIsReady();
        }
        public synchronized void setStringValue(String x) {
            mStringValue = x;
            notifyResultIsReady();
        }
        public synchronized void setObjectValue(Object x) {
            mObjectValue = x;
            notifyResultIsReady();
        }
        public synchronized void setCustomTypeValue(CustomType x) {
            mCustomTypeValue = x;
            notifyResultIsReady();
        }

        public synchronized boolean waitForBooleanValue() {
            waitForResult();
            return mBooleanValue;
        }
        public synchronized byte waitForByteValue() {
            waitForResult();
            return mByteValue;
        }
        public synchronized char waitForCharValue() {
            waitForResult();
            return mCharValue;
        }
        public synchronized short waitForShortValue() {
            waitForResult();
            return mShortValue;
        }
        public synchronized int waitForIntValue() {
            waitForResult();
            return mIntValue;
        }
        public synchronized long waitForLongValue() {
            waitForResult();
            return mLongValue;
        }
        public synchronized float waitForFloatValue() {
            waitForResult();
            return mFloatValue;
        }
        public synchronized double waitForDoubleValue() {
            waitForResult();
            return mDoubleValue;
        }
        public synchronized String waitForStringValue() {
            waitForResult();
            return mStringValue;
        }
        public synchronized Object waitForObjectValue() {
            waitForResult();
            return mObjectValue;
        }
        public synchronized CustomType waitForCustomTypeValue() {
            waitForResult();
            return mCustomTypeValue;
        }
    }

    // Two custom types used when testing passing objects.
    private static class CustomType {
    }
    private static class CustomType2 {
    }

    private TestObject mTestObject;

    private static class TestController extends Controller {
        private boolean mBooleanValue;

        public synchronized void setBooleanValue(boolean x) {
            mBooleanValue = x;
            notifyResultIsReady();
        }
        public synchronized boolean waitForBooleanValue() {
            waitForResult();
            return mBooleanValue;
        }
    }

    TestController mTestController;

    // Note that this requires that we can pass a JavaScript boolean to Java.
    private void assertRaisesException(String script) throws Throwable {
        mActivityTestRule.executeJavaScript("try {" + script + ";"
                + "  testController.setBooleanValue(false);"
                + "} catch (exception) {"
                + "  testController.setBooleanValue(true);"
                + "}");
        Assert.assertTrue(mTestController.waitForBooleanValue());
    }

    @Before
    public void setUp() {
        mTestObject = new TestObject();
        mTestController = new TestController();
        mActivityTestRule.injectObjectsAndReload(
                mTestObject, "testObject", mTestController, "testController", null);
    }

    // Test passing a 32-bit integer JavaScript number to a method of an
    // injected object. Note that JavaScript may choose to represent these
    // values as either 32-bit integers or doubles, though this should not
    // affect the result.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNumberInt32() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setByteValue(42);");
        Assert.assertEquals(42, mTestObject.waitForByteValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setByteValue(" + Byte.MAX_VALUE + " + 42);");
        Assert.assertEquals(Byte.MIN_VALUE + 42 - 1, mTestObject.waitForByteValue());

        mActivityTestRule.executeJavaScript("testObject.setCharValue(42);");
        Assert.assertEquals(42, mTestObject.waitForCharValue());

        mActivityTestRule.executeJavaScript("testObject.setShortValue(42);");
        Assert.assertEquals(42, mTestObject.waitForShortValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setShortValue(" + Short.MAX_VALUE + " + 42);");
        Assert.assertEquals(Short.MIN_VALUE + 42 - 1, mTestObject.waitForShortValue());

        mActivityTestRule.executeJavaScript("testObject.setIntValue(42);");
        Assert.assertEquals(42, mTestObject.waitForIntValue());

        mActivityTestRule.executeJavaScript("testObject.setLongValue(42);");
        Assert.assertEquals(42L, mTestObject.waitForLongValue());

        mActivityTestRule.executeJavaScript("testObject.setFloatValue(42);");
        Assert.assertEquals(42.0f, mTestObject.waitForFloatValue(), ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleValue(42);");
        Assert.assertEquals(42.0, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should create an instance of java.lang.Number.
        mActivityTestRule.executeJavaScript("testObject.setObjectValue(42);");
        Assert.assertNull(mTestObject.waitForObjectValue());

        // The spec allows the JS engine flexibility in how to format the number.
        mActivityTestRule.executeJavaScript("testObject.setStringValue(42);");
        String str = mTestObject.waitForStringValue();
        Assert.assertTrue("42".equals(str) || "42.0".equals(str));

        mActivityTestRule.executeJavaScript("testObject.setBooleanValue(0);");
        Assert.assertFalse(mTestObject.waitForBooleanValue());
        // LIVECONNECT_COMPLIANCE: Should be true;
        mActivityTestRule.executeJavaScript("testObject.setBooleanValue(42);");
        Assert.assertFalse(mTestObject.waitForBooleanValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeValue(42);");
        Assert.assertNull(mTestObject.waitForCustomTypeValue());
    }

    // Test passing a floating-point JavaScript number to a method of an
    // injected object. JavaScript represents these values as doubles.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNumberDouble() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setByteValue(42.1);");
        Assert.assertEquals(42, mTestObject.waitForByteValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setByteValue(" + Byte.MAX_VALUE + " + 42.1);");
        Assert.assertEquals(Byte.MIN_VALUE + 42 - 1, mTestObject.waitForByteValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setByteValue(" + Byte.MIN_VALUE + " - 42.1);");
        Assert.assertEquals(Byte.MAX_VALUE - 42 + 1, mTestObject.waitForByteValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setByteValue(" + Integer.MAX_VALUE + " + 42.1);");
        Assert.assertEquals(-1, mTestObject.waitForByteValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setByteValue(" + Integer.MIN_VALUE + " - 42.1);");
        Assert.assertEquals(0, mTestObject.waitForByteValue());

        // LIVECONNECT_COMPLIANCE: Should convert to numeric char value.
        mActivityTestRule.executeJavaScript("testObject.setCharValue(42.1);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharValue());

        mActivityTestRule.executeJavaScript("testObject.setShortValue(42.1);");
        Assert.assertEquals(42, mTestObject.waitForShortValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setShortValue(" + Short.MAX_VALUE + " + 42.1);");
        Assert.assertEquals(Short.MIN_VALUE + 42 - 1, mTestObject.waitForShortValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setShortValue(" + Short.MIN_VALUE + " - 42.1);");
        Assert.assertEquals(Short.MAX_VALUE - 42 + 1, mTestObject.waitForShortValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setShortValue(" + Integer.MAX_VALUE + " + 42.1);");
        Assert.assertEquals(-1, mTestObject.waitForShortValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setShortValue(" + Integer.MIN_VALUE + " - 42.1);");
        Assert.assertEquals(0, mTestObject.waitForShortValue());

        mActivityTestRule.executeJavaScript("testObject.setIntValue(42.1);");
        Assert.assertEquals(42, mTestObject.waitForIntValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setIntValue(" + Integer.MAX_VALUE + " + 42.1);");
        Assert.assertEquals(Integer.MAX_VALUE, mTestObject.waitForIntValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setIntValue(" + Integer.MIN_VALUE + " - 42.1);");
        Assert.assertEquals(Integer.MIN_VALUE, mTestObject.waitForIntValue());

        mActivityTestRule.executeJavaScript("testObject.setLongValue(42.1);");
        Assert.assertEquals(42L, mTestObject.waitForLongValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setLongValue(" + Long.MAX_VALUE + " + 42.1);");
        Assert.assertEquals(Long.MAX_VALUE, mTestObject.waitForLongValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setLongValue(" + Long.MIN_VALUE + " - 42.1);");
        Assert.assertEquals(Long.MIN_VALUE, mTestObject.waitForLongValue());

        mActivityTestRule.executeJavaScript("testObject.setFloatValue(42.1);");
        Assert.assertEquals(42.1f, mTestObject.waitForFloatValue(), ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleValue(42.1);");
        Assert.assertEquals(42.1, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should create an instance of java.lang.Number.
        mActivityTestRule.executeJavaScript("testObject.setObjectValue(42.1);");
        Assert.assertNull(mTestObject.waitForObjectValue());

        mActivityTestRule.executeJavaScript("testObject.setStringValue(42.1);");
        Assert.assertEquals("42.1", mTestObject.waitForStringValue());

        mActivityTestRule.executeJavaScript("testObject.setBooleanValue(0.0);");
        Assert.assertFalse(mTestObject.waitForBooleanValue());
        // LIVECONNECT_COMPLIANCE: Should be true.
        mActivityTestRule.executeJavaScript("testObject.setBooleanValue(42.1);");
        Assert.assertFalse(mTestObject.waitForBooleanValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeValue(42.1);");
        Assert.assertNull(mTestObject.waitForCustomTypeValue());
    }

    // Test passing JavaScript NaN to a method of an injected object.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNumberNaN() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setByteValue(Number.NaN);");
        Assert.assertEquals(0, mTestObject.waitForByteValue());

        mActivityTestRule.executeJavaScript("testObject.setCharValue(Number.NaN);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharValue());

        mActivityTestRule.executeJavaScript("testObject.setShortValue(Number.NaN);");
        Assert.assertEquals(0, mTestObject.waitForShortValue());

        mActivityTestRule.executeJavaScript("testObject.setIntValue(Number.NaN);");
        Assert.assertEquals(0, mTestObject.waitForIntValue());

        mActivityTestRule.executeJavaScript("testObject.setLongValue(Number.NaN);");
        Assert.assertEquals(0L, mTestObject.waitForLongValue());

        mActivityTestRule.executeJavaScript("testObject.setFloatValue(Number.NaN);");
        Assert.assertEquals(Float.NaN, mTestObject.waitForFloatValue(), ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleValue(Number.NaN);");
        Assert.assertEquals(Double.NaN, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should create an instance of java.lang.Number.
        mActivityTestRule.executeJavaScript("testObject.setObjectValue(Number.NaN);");
        Assert.assertNull(mTestObject.waitForObjectValue());

        mActivityTestRule.executeJavaScript("testObject.setStringValue(Number.NaN);");
        Assert.assertTrue("nan".equalsIgnoreCase(mTestObject.waitForStringValue()));

        mActivityTestRule.executeJavaScript("testObject.setBooleanValue(Number.NaN);");
        Assert.assertFalse(mTestObject.waitForBooleanValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeValue(Number.NaN);");
        Assert.assertNull(mTestObject.waitForCustomTypeValue());
    }

    // Test passing JavaScript infinity to a method of an injected object.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNumberInfinity() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setByteValue(Infinity);");
        Assert.assertEquals(-1, mTestObject.waitForByteValue());

        // LIVECONNECT_COMPLIANCE: Should convert to maximum numeric char value.
        mActivityTestRule.executeJavaScript("testObject.setCharValue(Infinity);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharValue());

        mActivityTestRule.executeJavaScript("testObject.setShortValue(Infinity);");
        Assert.assertEquals(-1, mTestObject.waitForShortValue());

        mActivityTestRule.executeJavaScript("testObject.setIntValue(Infinity);");
        Assert.assertEquals(Integer.MAX_VALUE, mTestObject.waitForIntValue());

        mActivityTestRule.executeJavaScript("testObject.setLongValue(Infinity);");
        Assert.assertEquals(Long.MAX_VALUE, mTestObject.waitForLongValue());

        mActivityTestRule.executeJavaScript("testObject.setFloatValue(Infinity);");
        Assert.assertEquals(
                Float.POSITIVE_INFINITY, mTestObject.waitForFloatValue(), ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleValue(Infinity);");
        Assert.assertEquals(
                Double.POSITIVE_INFINITY, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should create an instance of java.lang.Number.
        mActivityTestRule.executeJavaScript("testObject.setObjectValue(Infinity);");
        Assert.assertNull(mTestObject.waitForObjectValue());

        mActivityTestRule.executeJavaScript("testObject.setStringValue(Infinity);");
        Assert.assertTrue("inf".equalsIgnoreCase(mTestObject.waitForStringValue()));

        mActivityTestRule.executeJavaScript("testObject.setBooleanValue(Infinity);");
        Assert.assertFalse(mTestObject.waitForBooleanValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeValue(Infinity);");
        Assert.assertNull(mTestObject.waitForCustomTypeValue());
    }

    // Test passing a JavaScript boolean to a method of an injected object.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassBoolean() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setBooleanValue(true);");
        Assert.assertTrue(mTestObject.waitForBooleanValue());
        mActivityTestRule.executeJavaScript("testObject.setBooleanValue(false);");
        Assert.assertFalse(mTestObject.waitForBooleanValue());

        // LIVECONNECT_COMPLIANCE: Should create an instance of java.lang.Boolean.
        mActivityTestRule.executeJavaScript("testObject.setObjectValue(true);");
        Assert.assertNull(mTestObject.waitForObjectValue());

        mActivityTestRule.executeJavaScript("testObject.setStringValue(false);");
        Assert.assertEquals("false", mTestObject.waitForStringValue());
        mActivityTestRule.executeJavaScript("testObject.setStringValue(true);");
        Assert.assertEquals("true", mTestObject.waitForStringValue());

        // LIVECONNECT_COMPLIANCE: Should be 1.
        mActivityTestRule.executeJavaScript("testObject.setByteValue(true);");
        Assert.assertEquals(0, mTestObject.waitForByteValue());
        mActivityTestRule.executeJavaScript("testObject.setByteValue(false);");
        Assert.assertEquals(0, mTestObject.waitForByteValue());

        // LIVECONNECT_COMPLIANCE: Should convert to numeric char value 1.
        mActivityTestRule.executeJavaScript("testObject.setCharValue(true);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharValue());
        mActivityTestRule.executeJavaScript("testObject.setCharValue(false);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharValue());

        // LIVECONNECT_COMPLIANCE: Should be 1.
        mActivityTestRule.executeJavaScript("testObject.setShortValue(true);");
        Assert.assertEquals(0, mTestObject.waitForShortValue());
        mActivityTestRule.executeJavaScript("testObject.setShortValue(false);");
        Assert.assertEquals(0, mTestObject.waitForShortValue());

        // LIVECONNECT_COMPLIANCE: Should be 1.
        mActivityTestRule.executeJavaScript("testObject.setIntValue(true);");
        Assert.assertEquals(0, mTestObject.waitForIntValue());
        mActivityTestRule.executeJavaScript("testObject.setIntValue(false);");
        Assert.assertEquals(0, mTestObject.waitForIntValue());

        // LIVECONNECT_COMPLIANCE: Should be 1.
        mActivityTestRule.executeJavaScript("testObject.setLongValue(true);");
        Assert.assertEquals(0L, mTestObject.waitForLongValue());
        mActivityTestRule.executeJavaScript("testObject.setLongValue(false);");
        Assert.assertEquals(0L, mTestObject.waitForLongValue());

        // LIVECONNECT_COMPLIANCE: Should be 1.0.
        mActivityTestRule.executeJavaScript("testObject.setFloatValue(true);");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatValue(), ASSERTION_DELTA);
        mActivityTestRule.executeJavaScript("testObject.setFloatValue(false);");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should be 1.0.
        mActivityTestRule.executeJavaScript("testObject.setDoubleValue(true);");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);
        mActivityTestRule.executeJavaScript("testObject.setDoubleValue(false);");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeValue(true);");
        Assert.assertNull(mTestObject.waitForCustomTypeValue());
    }

    // Test passing a JavaScript string to a method of an injected object.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassString() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setStringValue(\"+042.10\");");
        Assert.assertEquals("+042.10", mTestObject.waitForStringValue());

        // Make sure that we distinguish between the empty string and NULL.
        mActivityTestRule.executeJavaScript("testObject.setStringValue(\"\");");
        Assert.assertEquals("", mTestObject.waitForStringValue());

        // LIVECONNECT_COMPLIANCE: Should create an instance of java.lang.String.
        mActivityTestRule.executeJavaScript("testObject.setObjectValue(\"+042.10\");");
        Assert.assertNull(mTestObject.waitForObjectValue());

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setByteValue(\"+042.10\");");
        Assert.assertEquals(0, mTestObject.waitForByteValue());

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setShortValue(\"+042.10\");");
        Assert.assertEquals(0, mTestObject.waitForShortValue());

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setIntValue(\"+042.10\");");
        Assert.assertEquals(0, mTestObject.waitForIntValue());

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setLongValue(\"+042.10\");");
        Assert.assertEquals(0L, mTestObject.waitForLongValue());

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setFloatValue(\"+042.10\");");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setDoubleValue(\"+042.10\");");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should decode and convert to numeric char value.
        mActivityTestRule.executeJavaScript("testObject.setCharValue(\"+042.10\");");
        Assert.assertEquals('\u0000', mTestObject.waitForCharValue());

        // LIVECONNECT_COMPLIANCE: Non-empty string should convert to true.
        mActivityTestRule.executeJavaScript("testObject.setBooleanValue(\"+042.10\");");
        Assert.assertFalse(mTestObject.waitForBooleanValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeValue(\"+042.10\");");
        Assert.assertNull(mTestObject.waitForCustomTypeValue());
    }

    // Test passing a JavaScript object to a method of an injected object.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassJavaScriptObject() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setObjectValue({foo: 42});");
        Assert.assertNull(mTestObject.waitForObjectValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeValue({foo: 42});");
        Assert.assertNull(mTestObject.waitForCustomTypeValue());

        // LIVECONNECT_COMPLIANCE: Should call toString() on object.
        mActivityTestRule.executeJavaScript("testObject.setStringValue({foo: 42});");
        Assert.assertEquals("undefined", mTestObject.waitForStringValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setByteValue({foo: 42});");
        Assert.assertEquals(0, mTestObject.waitForByteValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCharValue({foo: 42});");
        Assert.assertEquals('\u0000', mTestObject.waitForCharValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setShortValue({foo: 42});");
        Assert.assertEquals(0, mTestObject.waitForShortValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setIntValue({foo: 42});");
        Assert.assertEquals(0, mTestObject.waitForIntValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setLongValue({foo: 42});");
        Assert.assertEquals(0L, mTestObject.waitForLongValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setFloatValue({foo: 42});");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setDoubleValue({foo: 42});");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setBooleanValue({foo: 42});");
        Assert.assertFalse(mTestObject.waitForBooleanValue());
    }

    // Test passing a Java object to a method of an injected object. Note that
    // this test requires being able to return objects from the methods of
    // injected objects. This is tested elsewhere.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassJavaObject() throws Throwable {
        mActivityTestRule.executeJavaScript(
                "testObject.setObjectValue(testObject.getObjectInstance());");
        Assert.assertTrue(mTestObject.getObjectInstance() == mTestObject.waitForObjectValue());
        mActivityTestRule.executeJavaScript(
                "testObject.setObjectValue(testObject.getCustomTypeInstance());");
        Assert.assertTrue(mTestObject.getCustomTypeInstance() == mTestObject.waitForObjectValue());

        assertRaisesException("testObject.setCustomTypeValue(testObject.getObjectInstance());");
        mActivityTestRule.executeJavaScript(
                "testObject.setCustomTypeValue(testObject.getCustomTypeInstance());");
        Assert.assertTrue(
                mTestObject.getCustomTypeInstance() == mTestObject.waitForCustomTypeValue());
        assertRaisesException(
                "testObject.setCustomTypeValue(testObject.getCustomType2Instance());");

        // LIVECONNECT_COMPLIANCE: Should call toString() on object.
        mActivityTestRule.executeJavaScript(
                "testObject.setStringValue(testObject.getObjectInstance());");
        Assert.assertEquals("undefined", mTestObject.waitForStringValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setByteValue(testObject.getObjectInstance());");
        Assert.assertEquals(0, mTestObject.waitForByteValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setCharValue(testObject.getObjectInstance());");
        Assert.assertEquals('\u0000', mTestObject.waitForCharValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setShortValue(testObject.getObjectInstance());");
        Assert.assertEquals(0, mTestObject.waitForShortValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setIntValue(testObject.getObjectInstance());");
        Assert.assertEquals(0, mTestObject.waitForIntValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setLongValue(testObject.getObjectInstance());");
        Assert.assertEquals(0L, mTestObject.waitForLongValue());

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setFloatValue(testObject.getObjectInstance());");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setDoubleValue(testObject.getObjectInstance());");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setBooleanValue(testObject.getObjectInstance());");
        Assert.assertFalse(mTestObject.waitForBooleanValue());
    }

    static void assertFileIsReadable(String filePath) {
        File file = new File(filePath);
        try {
            Assert.assertTrue("Test file \"" + filePath + "\" is not readable.", file.canRead());
        } catch (SecurityException e) {
            Assert.fail("Got a SecurityException for \"" + filePath + "\": " + e.toString());
        }
    }

    // Verifies that classes obtained via custom class loaders can be
    // passed in and out to injected methods. In real life WebView scenarios
    // WebView and the app use different class loaders, thus we need to make
    // sure that WebView code doesn't attempt to find an app's class using
    // its own class loader. See crbug.com/491800.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassJavaObjectFromCustomClassLoader() throws Throwable {
        // Compiled bytecode (dex) for the following class:
        //
        // package org.example;
        // public class SelfConsumingObject {
        //   public SelfConsumingObject getSelf() {
        //     return this;
        //   }
        //   public boolean verifySelf(SelfConsumingObject self) {
        //     return this == self;
        //   }
        // }
        final String dexFileName = "content/test/data/android/SelfConsumingObject.dex";
        assertFileIsReadable(UrlUtils.getIsolatedTestFilePath(dexFileName));
        final File optimizedDir = File.createTempFile("optimized", "");
        Assert.assertTrue(optimizedDir.delete());
        Assert.assertTrue(optimizedDir.mkdirs());
        DexClassLoader loader = new DexClassLoader(UrlUtils.getIsolatedTestFilePath(dexFileName),
                optimizedDir.getAbsolutePath(), null, ClassLoader.getSystemClassLoader());
        final Object selfConsuming = loader.loadClass(
                "org.example.SelfConsumingObject").newInstance();
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getJavascriptInjector().addPossiblyUnsafeInterface(
                        selfConsuming, "selfConsuming", null);
            }
        });
        mActivityTestRule.synchronousPageReload();
        mActivityTestRule.executeJavaScript("testObject.setBooleanValue("
                + "selfConsuming.verifySelf(selfConsuming.getSelf()));");
        Assert.assertTrue(mTestObject.waitForBooleanValue());
    }

    // Test passing JavaScript null to a method of an injected object.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNull() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setObjectValue(null);");
        Assert.assertNull(mTestObject.waitForObjectValue());

        mActivityTestRule.executeJavaScript("testObject.setCustomTypeValue(null);");
        Assert.assertNull(mTestObject.waitForCustomTypeValue());

        mActivityTestRule.executeJavaScript("testObject.setStringValue(null);");
        Assert.assertNull(mTestObject.waitForStringValue());

        mActivityTestRule.executeJavaScript("testObject.setByteValue(null);");
        Assert.assertEquals(0, mTestObject.waitForByteValue());

        mActivityTestRule.executeJavaScript("testObject.setCharValue(null);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharValue());

        mActivityTestRule.executeJavaScript("testObject.setShortValue(null);");
        Assert.assertEquals(0, mTestObject.waitForShortValue());

        mActivityTestRule.executeJavaScript("testObject.setIntValue(null);");
        Assert.assertEquals(0, mTestObject.waitForIntValue());

        mActivityTestRule.executeJavaScript("testObject.setLongValue(null);");
        Assert.assertEquals(0L, mTestObject.waitForLongValue());

        mActivityTestRule.executeJavaScript("testObject.setFloatValue(null);");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatValue(), ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleValue(null);");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setBooleanValue(null);");
        Assert.assertFalse(mTestObject.waitForBooleanValue());
    }

    // Test passing JavaScript undefined to a method of an injected object.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassUndefined() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setObjectValue(undefined);");
        Assert.assertNull(mTestObject.waitForObjectValue());

        mActivityTestRule.executeJavaScript("testObject.setCustomTypeValue(undefined);");
        Assert.assertNull(mTestObject.waitForCustomTypeValue());

        // LIVECONNECT_COMPLIANCE: Should be NULL.
        mActivityTestRule.executeJavaScript("testObject.setStringValue(undefined);");
        Assert.assertEquals("undefined", mTestObject.waitForStringValue());

        mActivityTestRule.executeJavaScript("testObject.setByteValue(undefined);");
        Assert.assertEquals(0, mTestObject.waitForByteValue());

        mActivityTestRule.executeJavaScript("testObject.setCharValue(undefined);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharValue());

        mActivityTestRule.executeJavaScript("testObject.setShortValue(undefined);");
        Assert.assertEquals(0, mTestObject.waitForShortValue());

        mActivityTestRule.executeJavaScript("testObject.setIntValue(undefined);");
        Assert.assertEquals(0, mTestObject.waitForIntValue());

        mActivityTestRule.executeJavaScript("testObject.setLongValue(undefined);");
        Assert.assertEquals(0L, mTestObject.waitForLongValue());

        mActivityTestRule.executeJavaScript("testObject.setFloatValue(undefined);");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatValue(), ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleValue(undefined);");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setBooleanValue(undefined);");
        Assert.assertFalse(mTestObject.waitForBooleanValue());
    }

    // Verify that ArrayBuffers are not converted into objects or strings when passed
    // to Java. Basically, ArrayBuffers are treated as generic JavaScript objects.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassArrayBuffer() throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(16);");

        mActivityTestRule.executeJavaScript("testObject.setObjectValue(buffer);");
        Assert.assertNull(mTestObject.waitForObjectValue());

        mActivityTestRule.executeJavaScript("testObject.setStringValue(buffer);");
        Assert.assertEquals("undefined", mTestObject.waitForStringValue());
    }

    // Verify that ArrayBufferViewss are not converted into objects or strings when passed
    // to Java. Basically, ArrayBufferViews are treated as generic JavaScript objects.
    // Here, a DataView is used as an ArrayBufferView instance (since the latter is
    // an interface and can't be instantiated directly).
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassDataView() throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(16);");

        mActivityTestRule.executeJavaScript("testObject.setObjectValue(new DataView(buffer));");
        Assert.assertNull(mTestObject.waitForObjectValue());

        mActivityTestRule.executeJavaScript("testObject.setStringValue(new DataView(buffer));");
        Assert.assertEquals("undefined", mTestObject.waitForStringValue());
    }

    // Verify that Date objects are not converted into double values, strings or objects.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassDateObject() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setDoubleValue(new Date(2000, 0, 1));");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleValue(), ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setStringValue(new Date(2000, 0, 1));");
        Assert.assertEquals("undefined", mTestObject.waitForStringValue());

        mActivityTestRule.executeJavaScript("testObject.setObjectValue(new Date(2000, 0, 1));");
        Assert.assertNull(mTestObject.waitForObjectValue());
    }

    // Verify that RegExp objects are not converted into strings or objects.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassRegExpObject() throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setStringValue(/abc/);");
        Assert.assertEquals("undefined", mTestObject.waitForStringValue());

        mActivityTestRule.executeJavaScript("testObject.setObjectValue(/abc/);");
        Assert.assertNull(mTestObject.waitForObjectValue());
    }

    // Verify that Function objects are not converted into strings or objects.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassFunctionObject() throws Throwable {
        mActivityTestRule.executeJavaScript("func = new Function('a', 'b', 'return a + b');");

        mActivityTestRule.executeJavaScript("testObject.setStringValue(func);");
        Assert.assertEquals("undefined", mTestObject.waitForStringValue());

        mActivityTestRule.executeJavaScript("testObject.setObjectValue(func);");
        Assert.assertNull(mTestObject.waitForObjectValue());
    }
}
