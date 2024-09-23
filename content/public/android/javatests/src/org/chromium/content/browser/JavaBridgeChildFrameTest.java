// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.webkit.JavascriptInterface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.JavaBridgeActivityTestRule.Controller;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;

import java.lang.ref.WeakReference;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Part of the test suite for the WebView's Java Bridge.
 *
 * <p>Ensures that injected objects are exposed to child frames as well as the main frame.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(JavaBridgeActivityTestRule.BATCH)
public class JavaBridgeChildFrameTest {
    @Rule public JavaBridgeActivityTestRule mActivityTestRule = new JavaBridgeActivityTestRule();

    private static class TestController extends Controller {
        private String mStringValue;

        @SuppressWarnings("unused") // Called via reflection
        @JavascriptInterface
        public synchronized void setStringValue(String x) {
            mStringValue = x;
            notifyResultIsReady();
        }

        public synchronized String waitForStringValue() {
            waitForResult();
            return mStringValue;
        }
    }

    TestController mTestController;

    @Before
    public void setUp() {
        mTestController = new TestController();
        mActivityTestRule.injectObjectAndReload(mTestController, "testController");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testInjectedObjectPresentInChildFrame() throws Throwable {
        loadDataSync(
                mActivityTestRule.getWebContents().getNavigationController(),
                "<html><body><iframe></iframe></body></html>",
                "text/html",
                false);
        // We are not executing this code as a part of page loading routine to avoid races
        // with internal Blink events that notify Java Bridge about window object updates.
        Assert.assertEquals(
                "\"object\"",
                executeJavaScriptAndGetResult(
                        mActivityTestRule.getWebContents(),
                        "typeof window.frames[0].testController"));
        executeJavaScriptAndGetResult(
                mActivityTestRule.getWebContents(),
                "window.frames[0].testController.setStringValue('PASS')");
        Assert.assertEquals("PASS", mTestController.waitForStringValue());
    }

    // Verify that loading an iframe doesn't ruin JS wrapper of the main page.
    // This is a regression test for the problem described in b/15572824.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMainPageWrapperIsNotBrokenByChildFrame() throws Throwable {
        loadDataSync(
                mActivityTestRule.getWebContents().getNavigationController(),
                "<html><body><iframe></iframe></body></html>",
                "text/html",
                false);
        // In case there is anything wrong with the JS wrapper, an attempt
        // to look up its properties will result in an exception being thrown.
        String script =
                "(function(){ try {"
                        + "  return typeof testController.setStringValue;"
                        + "} catch (e) {"
                        + "  return e.toString();"
                        + "} })()";
        Assert.assertEquals(
                "\"function\"",
                executeJavaScriptAndGetResult(mActivityTestRule.getWebContents(), script));
        // Make sure calling a method also works.
        executeJavaScriptAndGetResult(
                mActivityTestRule.getWebContents(), "testController.setStringValue('PASS');");
        Assert.assertEquals("PASS", mTestController.waitForStringValue());
    }

    // Verify that parent page and child frame each has own JS wrapper object.
    // Failing to do so exposes parent's context to the child.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testWrapperIsNotSharedWithChildFrame() throws Throwable {
        // Test by setting a custom property on the parent page's injected
        // object and then checking that child frame doesn't see the property.
        loadDataSync(
                mActivityTestRule.getWebContents().getNavigationController(),
                "<html><head>"
                        + "<script>"
                        + "  window.wProperty = 42;"
                        + "  testController.tcProperty = 42;"
                        + "  function queryProperties(w) {"
                        + "    return w.wProperty + ' / ' + w.testController.tcProperty;"
                        + "  }"
                        + "</script>"
                        + "</head><body><iframe></iframe></body></html>",
                "text/html",
                false);
        Assert.assertEquals(
                "\"42 / 42\"",
                executeJavaScriptAndGetResult(
                        mActivityTestRule.getWebContents(), "queryProperties(window)"));
        Assert.assertEquals(
                "\"undefined / undefined\"",
                executeJavaScriptAndGetResult(
                        mActivityTestRule.getWebContents(), "queryProperties(window.frames[0])"));
    }

    // Regression test for crbug.com/484927 -- make sure that existence of transient
    // objects held by multiple RenderFrames doesn't cause an infinite loop when one
    // of them gets removed.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @DisabledTest(message = "https://crbug.com/677053")
    public void testRemovingTransientObjectHolders() throws Throwable {
        class Test {
            private Object mInner = new Object();
            // Expecting the inner object to be retrieved twice.
            private CountDownLatch mLatch = new CountDownLatch(2);

            @JavascriptInterface
            public Object getInner() {
                mLatch.countDown();
                return mInner;
            }

            public void waitForInjection() throws Throwable {
                if (!mLatch.await(5, TimeUnit.SECONDS)) {
                    throw new TimeoutException();
                }
            }
        }
        final Test testObject = new Test();

        // Due to crbug.com/486262, Java objects are sometimes not injected
        // into newly added frames. To work around this, we load the page first, so
        // all the frames got created, then inject the object.
        // Thus, the script code fails on the first execution (as no Java object is
        // injected yet), but then works just fine after reload.
        loadDataSync(
                mActivityTestRule.getWebContents().getNavigationController(),
                "<html>"
                        + "<head><script>window.inner_ref = test.getInner()</script></head>"
                        + "<body>"
                        + "   <iframe id='frame' "
                        + "       srcdoc='<script>window.inner_ref = test.getInner()</script>'>"
                        + "   </iframe>"
                        + "</body></html>",
                "text/html",
                false);
        mActivityTestRule.injectObjectAndReload(testObject, "test");
        testObject.waitForInjection();
        // Just in case, check that the object wrappers are in place.
        Assert.assertEquals(
                "\"object\"",
                executeJavaScriptAndGetResult(
                        mActivityTestRule.getWebContents(), "typeof inner_ref"));
        Assert.assertEquals(
                "\"object\"",
                executeJavaScriptAndGetResult(
                        mActivityTestRule.getWebContents(), "typeof window.frames[0].inner_ref"));
        // Remove the iframe, this will trigger a removal of RenderFrame, which was causing
        // the bug condition, as the transient object still has a holder -- the main window.
        Assert.assertEquals(
                "\"object\"",
                executeJavaScriptAndGetResult(
                        mActivityTestRule.getWebContents(),
                        "(function(){ "
                                + "var f = document.getElementById('frame');"
                                + "f.parentNode.removeChild(f); return typeof f; })()"));
        // Just in case, check that the remaining wrapper is still accessible.
        Assert.assertEquals(
                "\"object\"",
                executeJavaScriptAndGetResult(
                        mActivityTestRule.getWebContents(), "typeof inner_ref"));
    }

    // Regression test for crbug.com/486245 -- assign ownership of a transient object
    // to one frame with a code running in the second frame. Deletion of the second
    // frame should not affect the injected object.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @CommandLineFlags.Add("js-flags=--expose-gc")
    @DisabledTest(message = "https://crbug.com/646843")
    public void testHolderFrame() throws Throwable {
        class Test {
            WeakReference<Object> mWeakRefForInner;
            private CountDownLatch mLatch = new CountDownLatch(1);

            @JavascriptInterface
            public Object getInner() {
                mLatch.countDown();
                Object inner = new Object();
                mWeakRefForInner = new WeakReference<Object>(inner);
                return inner;
            }

            public void waitForInjection() throws Throwable {
                if (!mLatch.await(5, TimeUnit.SECONDS)) {
                    throw new TimeoutException();
                }
            }
        }
        final Test testObject = new Test();

        Assert.assertEquals(
                "\"function\"",
                executeJavaScriptAndGetResult(mActivityTestRule.getWebContents(), "typeof gc"));
        // The page executes in the second frame code which creates a wrapper for a transient
        // injected object, but makes the first frame the owner of the object.
        loadDataSync(
                mActivityTestRule.getWebContents().getNavigationController(),
                "<html>"
                        + "<head></head>"
                        + "<body>"
                        + "   <iframe id='frame1' "
                        + "       srcdoc='<body>I am the Inner object owner!</body>'>"
                        + "   </iframe>"
                        + "   <iframe id='frame2' "
                        + "       srcdoc='<script>"
                        + "           window.parent.frames[0].inner_ref = test.getInner()"
                        + "       </script>'>"
                        + "   </iframe>"
                        + "</body></html>",
                "text/html",
                false);
        mActivityTestRule.injectObjectAndReload(testObject, "test");
        testObject.waitForInjection();
        // Check that the object wrappers are in place.
        Assert.assertTrue(testObject.mWeakRefForInner.get() != null);
        Assert.assertEquals(
                "\"object\"",
                executeJavaScriptAndGetResult(
                        mActivityTestRule.getWebContents(), "typeof window.frames[0].inner_ref"));
        // Remove the second frame. This must not toggle the deletion of the inner
        // object.
        Assert.assertEquals(
                "\"object\"",
                executeJavaScriptAndGetResult(
                        mActivityTestRule.getWebContents(),
                        "(function(){ "
                                + "var f = document.getElementById('frame2');"
                                + "f.parentNode.removeChild(f); return typeof f; })()"));
        // Perform two major GCs at the end to flush out all wrappers
        // and other Blink (Oilpan) objects.
        executeJavaScriptAndGetResult(
                mActivityTestRule.getWebContents(), "for (var i = 0; i < 2; ++i) gc();");
        // Check that returned Java object is being held by the Java bridge, thus it's not
        // collected.  Note that despite that what JavaDoc says about invoking "gc()", both Dalvik
        // and ART actually run the collector.
        Runtime.getRuntime().gc();
        Assert.assertNotNull(testObject.mWeakRefForInner.get());
        // Now, remove the first frame and GC. As it was the only holder of the
        // inner object's wrapper, the wrapper must be collected. Then, the death
        // of the wrapper must cause removal of the inner object.
        Assert.assertEquals(
                "\"object\"",
                executeJavaScriptAndGetResult(
                        mActivityTestRule.getWebContents(),
                        "(function(){ "
                                + "var f = document.getElementById('frame1');"
                                + "f.parentNode.removeChild(f); return typeof f; })()"));
        executeJavaScriptAndGetResult(
                mActivityTestRule.getWebContents(), "for (var i = 0; i < 2; ++i) gc();");
        Runtime.getRuntime().gc();
        Assert.assertNull(testObject.mWeakRefForInner.get());
    }

    private String executeJavaScriptAndGetResult(
            final WebContents webContents, final String script) {
        final String[] result = new String[1];
        class ResultCallback extends Controller implements JavaScriptCallback {
            @Override
            public void handleJavaScriptResult(String jsonResult) {
                result[0] = jsonResult;
                notifyResultIsReady();
            }
        }
        final ResultCallback resultCallback = new ResultCallback();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                webContents.evaluateJavaScriptForTests(script, resultCallback);
                            }
                        });
        resultCallback.waitForResult();
        return result[0];
    }

    /** Loads data on the UI thread and blocks until onPageFinished is called. */
    private void loadDataSync(
            final NavigationController navigationController,
            final String data,
            final String mimeType,
            final boolean isBase64Encoded)
            throws Throwable {
        mActivityTestRule.loadUrl(
                navigationController,
                mActivityTestRule.getTestCallBackHelperContainer(),
                LoadUrlParams.createLoadDataParams(data, mimeType, isBase64Encoded));
    }
}
