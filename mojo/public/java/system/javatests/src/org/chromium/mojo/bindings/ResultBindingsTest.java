// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.mojo.MojoTestRule;
import org.chromium.mojo.bindings.test.mojom.result.ResultIface;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.impl.CoreImpl;

/** Tests Java end-to-end serialization and deserialization of result responses. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ResultBindingsTest {
    @Rule public MojoTestRule mTestRule = new MojoTestRule();

    // Simple stub go make it easier to extend the iface in each test case.
    abstract static class TestResultIface implements ResultIface {
        @Override
        public void method2(ResultIface.Method2_Response cb) {
            cb.call(Result.of(true));
        }

        @Override
        public void close() {}

        @Override
        public void onConnectionError(MojoException e) {}
    }

    /** Tests a basic case where the method was successful. */
    @Test
    @MediumTest
    public void testClient_success() throws Exception {
        var pair = ResultIface.MANAGER.getInterfaceRequest(CoreImpl.getInstance());
        var ifaceProxy = pair.first;
        var ifaceRequest = pair.second;
        ResultIface.MANAGER.bind(
                new TestResultIface() {
                    @Override
                    public void method(ResultIface.Method_Response cb) {
                        cb.call(Result.of(true));
                    }
                },
                ifaceRequest);

        var wrapper =
                new Object() {
                    Result mResult;
                };
        ifaceProxy.method(result -> wrapper.mResult = result);
        mTestRule.runLoopUntilIdle();

        Assert.assertTrue((boolean) wrapper.mResult.get());
    }

    /** Tests a basic case where the method was not successful. */
    @Test
    @MediumTest
    public void testClient_error() {
        var pair = ResultIface.MANAGER.getInterfaceRequest(CoreImpl.getInstance());
        var ifaceProxy = pair.first;
        var ifaceRequest = pair.second;
        ResultIface.MANAGER.bind(
                new TestResultIface() {
                    @Override
                    public void method(ResultIface.Method_Response cb) {
                        cb.call(Result.ofError("hihi"));
                    }
                },
                ifaceRequest);

        var wrapper =
                new Object() {
                    Result mResult;
                };
        ifaceProxy.method(result -> wrapper.mResult = result);
        mTestRule.runLoopUntilIdle();

        Assert.assertTrue("hihi".equals(wrapper.mResult.getError()));
    }
}
