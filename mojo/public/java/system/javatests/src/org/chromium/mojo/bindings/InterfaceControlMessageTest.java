// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.mojo.MojoTestRule;
import org.chromium.mojo.bindings.Callbacks.Callback1;
import org.chromium.mojo.bindings.test.mojom.sample.IntegerAccessor;
import org.chromium.mojo.system.MojoException;

import java.io.Closeable;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests for interface control messages. */
@RunWith(BaseJUnit4ClassRunner.class)
public class InterfaceControlMessageTest {
    @Rule public MojoTestRule mTestRule = new MojoTestRule();

    private final List<Closeable> mCloseablesToClose = new ArrayList<Closeable>();

    /** See mojo/public/interfaces/bindings/tests/sample_interfaces.mojom. */
    class IntegerAccessorImpl extends SideEffectFreeCloseable implements IntegerAccessor {
        private long mValue;
        private int mEnum;
        private boolean mEncounteredError;

        /**
         * @see ConnectionErrorHandler#onConnectionError(MojoException)
         */
        @Override
        public void onConnectionError(MojoException e) {
            mEncounteredError = true;
        }

        /**
         * @see IntegerAccessor#getInteger(IntegerAccessor.GetIntegerResponse)
         */
        @Override
        public void getInteger(GetIntegerResponse response) {
            response.call(mValue, mEnum);
        }

        /**
         * @see IntegerAccessor#setInteger(long, int)
         */
        @Override
        public void setInteger(long value, int enumValue) {
            mValue = value;
            mEnum = enumValue;
        }

        public long getValue() {
            return mValue;
        }

        public boolean encounteredError() {
            return mEncounteredError;
        }
    }

    /**
     * @see MojoTestCase#tearDown()
     */
    @After
    public void tearDown() throws Exception {
        // Close the elements in the reverse order they were added. This is needed because it is an
        // error to close the handle of a proxy without closing the proxy first.
        Collections.reverse(mCloseablesToClose);
        for (Closeable c : mCloseablesToClose) {
            c.close();
        }
    }

    @Test
    @SmallTest
    public void testQueryVersion() {
        IntegerAccessor.Proxy p =
                BindingsTestUtils.newProxyOverPipe(
                        IntegerAccessor.MANAGER, new IntegerAccessorImpl(), mCloseablesToClose);
        Assert.assertEquals(0, p.getProxyHandler().getVersion());
        p.getProxyHandler()
                .queryVersion(
                        new Callback1<Integer>() {
                            @Override
                            public void call(Integer version) {
                                Assert.assertEquals(3, version.intValue());
                            }
                        });
        mTestRule.runLoopUntilIdle();
        Assert.assertEquals(3, p.getProxyHandler().getVersion());
    }

    @Test
    @SmallTest
    public void testRequireVersion() {
        IntegerAccessorImpl impl = new IntegerAccessorImpl();
        IntegerAccessor.Proxy p =
                BindingsTestUtils.newProxyOverPipe(
                        IntegerAccessor.MANAGER, impl, mCloseablesToClose);

        Assert.assertEquals(0, p.getProxyHandler().getVersion());

        p.getProxyHandler().requireVersion(1);
        Assert.assertEquals(1, p.getProxyHandler().getVersion());
        p.setInteger(123, Enum.VALUE);
        mTestRule.runLoopUntilIdle();
        Assert.assertFalse(impl.encounteredError());
        Assert.assertEquals(123, impl.getValue());

        p.getProxyHandler().requireVersion(3);
        Assert.assertEquals(3, p.getProxyHandler().getVersion());
        p.setInteger(456, Enum.VALUE);
        mTestRule.runLoopUntilIdle();
        Assert.assertFalse(impl.encounteredError());
        Assert.assertEquals(456, impl.getValue());

        // Require a version that is not supported by the implementation side.
        p.getProxyHandler().requireVersion(4);
        // getVersion() is updated synchronously.
        Assert.assertEquals(4, p.getProxyHandler().getVersion());
        p.setInteger(789, Enum.VALUE);
        mTestRule.runLoopUntilIdle();
        Assert.assertTrue(impl.encounteredError());
        // The call to setInteger() after requireVersion() is ignored.
        Assert.assertEquals(456, impl.getValue());
    }
}
