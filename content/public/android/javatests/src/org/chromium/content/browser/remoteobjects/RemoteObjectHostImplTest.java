// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.remoteobjects;

import static org.hamcrest.Matchers.isIn;
import static org.hamcrest.Matchers.not;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.blink.mojom.RemoteObject;
import org.chromium.mojo.MojoTestRule;
import org.chromium.mojo.bindings.ConnectionErrorHandler;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.HashSet;
import java.util.Set;

/**
 * Tests the Mojo interface which vends RemoteObject interface handles.
 * Ensures that the provided handles are properly bound.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public final class RemoteObjectHostImplTest {
    @Rule
    public MojoTestRule mMojoTestRule = new MojoTestRule(MojoTestRule.MojoCore.INITIALIZE);

    private final Set<RemoteObjectRegistry> mRetainingSet = new HashSet<>();
    private final RemoteObjectRegistry mRegistry = new RemoteObjectRegistry(mRetainingSet);

    /**
     * Annotation which can be used in the way that {@link android.webkit.JavascriptInterface}
     * would.
     */
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD})
    private @interface TestJavascriptInterface {}

    /**
     * {@link ConnectionErrorHandler} that records any error it received.
     */
    private static class CapturingErrorHandler implements ConnectionErrorHandler {
        private MojoException mLastMojoException;

        /**
         * @see ConnectionErrorHandler#onConnectionError(MojoException)
         */
        @Override
        public void onConnectionError(MojoException e) {
            mLastMojoException = e;
        }

        /**
         * Returns the last recorded exception.
         */
        public MojoException getLastMojoException() {
            return mLastMojoException;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testClosesPipeIfObjectDoesNotExist() {
        RemoteObjectHostImpl host = new RemoteObjectHostImpl(
                TestJavascriptInterface.class, /* auditor */ null, mRegistry);

        Pair<RemoteObject.Proxy, InterfaceRequest<RemoteObject>> result =
                RemoteObject.MANAGER.getInterfaceRequest(CoreImpl.getInstance());
        CapturingErrorHandler errorHandler = new CapturingErrorHandler();
        result.first.getProxyHandler().setErrorHandler(errorHandler);
        host.getObject(123, result.second);

        mMojoTestRule.runLoopUntilIdle();
        Assert.assertNotNull(errorHandler.getLastMojoException());
    }

    /**
     * Tiny utility to capture the result of using RemoteObject.
     *
     * This verifies that it is working correctly.
     */
    private static class HasMethodCapture implements RemoteObject.HasMethodResponse {
        public Boolean methodExists;

        @Override
        public void call(Boolean result) {
            methodExists = result;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testBindsPipeIfObjectExists() {
        Object o = new Object() {
            @TestJavascriptInterface
            public void frobnicate() {}
        };
        int id = mRegistry.getObjectId(o);

        RemoteObjectHostImpl host = new RemoteObjectHostImpl(
                TestJavascriptInterface.class, /* auditor */ null, mRegistry);

        Pair<RemoteObject.Proxy, InterfaceRequest<RemoteObject>> result =
                RemoteObject.MANAGER.getInterfaceRequest(CoreImpl.getInstance());
        RemoteObject.Proxy remoteObject = result.first;
        CapturingErrorHandler errorHandler = new CapturingErrorHandler();
        remoteObject.getProxyHandler().setErrorHandler(errorHandler);
        host.getObject(id, result.second);

        HasMethodCapture frobnicate = new HasMethodCapture();
        remoteObject.hasMethod("frobnicate", frobnicate);
        HasMethodCapture nonExistentMethod = new HasMethodCapture();
        remoteObject.hasMethod("nonExistentMethod", nonExistentMethod);

        mMojoTestRule.runLoopUntilIdle();
        Assert.assertNull(errorHandler.getLastMojoException());
        Assert.assertEquals(true, frobnicate.methodExists);
        Assert.assertEquals(false, nonExistentMethod.methodExists);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testRelease() {
        Object o = new Object();
        int id = mRegistry.getObjectId(o);

        RemoteObjectHostImpl host = new RemoteObjectHostImpl(
                TestJavascriptInterface.class, /* auditor */ null, mRegistry);

        Assert.assertSame(o, mRegistry.getObjectById(id));
        host.releaseObject(id);
        Assert.assertNull(mRegistry.getObjectById(id));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testClose() {
        RemoteObjectHostImpl host = new RemoteObjectHostImpl(
                TestJavascriptInterface.class, /* auditor */ null, mRegistry);
        Assert.assertThat(mRegistry, isIn(mRetainingSet));
        host.close();
        Assert.assertThat(mRegistry, not(isIn(mRetainingSet)));
    }
}
