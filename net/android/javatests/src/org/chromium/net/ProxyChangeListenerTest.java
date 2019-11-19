// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Proxy;
import android.os.Build;
import android.os.Looper;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.AdvancedMockContext;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link ProxyChangeListener}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ProxyChangeListenerTest {
    FakeContext mAppContext;
    ProxyChangeListener mListener;
    ProxyChangeListener.Delegate mDelegate;

    private static class FakeContext extends AdvancedMockContext {
        private class RegisteredReceiver {
            final BroadcastReceiver mReceiver;
            final IntentFilter mFilter;

            RegisteredReceiver(BroadcastReceiver receiver, IntentFilter filter) {
                mReceiver = receiver;
                mFilter = filter;
            }

            void filterAndPostBroadcast(final Intent broadcast) {
                if (mFilter != null) {
                    if (mFilter.match(null, broadcast, false, "sendBroadcast") < 0) {
                        return;
                    }
                }
                mReceiver.onReceive(FakeContext.this, broadcast);
            }
        }
        private List<RegisteredReceiver> mReceivers = new ArrayList<>();

        FakeContext() {
            super(InstrumentationRegistry.getInstrumentation()
                            .getTargetContext()
                            .getApplicationContext());
        }

        @Override
        public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
            mReceivers.add(new RegisteredReceiver(receiver, filter));
            return null;
        }

        @Override
        public void unregisterReceiver(BroadcastReceiver receiver) {
            for (RegisteredReceiver r : mReceivers) {
                if (r.mReceiver == receiver) {
                    mReceivers.remove(r);
                    return;
                }
            }
            throw new IllegalStateException("Receiver not found.");
        }

        @Override
        public void sendBroadcast(Intent intent) {
            for (final RegisteredReceiver r : mReceivers) {
                r.filterAndPostBroadcast(intent);
            }
        }

        public List<BroadcastReceiver> getReceivers() {
            ArrayList<BroadcastReceiver> result = new ArrayList<>(mReceivers.size());
            for (final RegisteredReceiver r : mReceivers) {
                result.add(r.mReceiver);
            }
            return result;
        }
    }

    @Before
    public void setUp() {
        mAppContext = Mockito.spy(new FakeContext());
        ContextUtils.initApplicationContextForTests(mAppContext);

        Looper.prepare();

        // Create the listener that's going to be used for tests
        mListener = ProxyChangeListener.create();
        mListener.setDelegateForTesting(
                mDelegate = Mockito.mock(ProxyChangeListener.Delegate.class));
        mListener.start(0);

        Mockito.verify(mAppContext)
                .registerReceiver(Mockito.anyObject(),
                        Mockito.argThat((IntentFilter filter)
                                                -> filter.matchAction(Proxy.PROXY_CHANGE_ACTION)));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Mockito.verify(mAppContext)
                    .registerReceiver(Mockito.anyObject(),
                            Mockito.argThat(
                                    (IntentFilter filter)
                                            -> !filter.matchAction(Proxy.PROXY_CHANGE_ACTION)));
        }
    }

    @After
    public void tearDown() {
        mListener.stop();
        Assert.assertEquals("All receivers should have been unregistered",
                mAppContext.getReceivers().size(), 0);
    }

    @Test
    @SmallTest
    public void testProxyChangeListenerDelegateCalled() {
        Intent intent = new Intent();
        intent.setAction(Proxy.PROXY_CHANGE_ACTION);
        mAppContext.sendBroadcast(intent);

        Mockito.verify(mDelegate, Mockito.times(1)).proxySettingsChanged();
    }

    @Test
    @SmallTest
    public void testProxyChangeListenerDelegateCalledByReflection() throws Exception {
        // Assuming that the reflection to get the LoadedApk list of broadcast receivers worked,
        // this tests that an app can correctly find the ProxyChangeListener (which may be fake),
        // and call its onReceived method, resulting in one delegate invocation.
        for (Object rec : mAppContext.getReceivers()) {
            Class<?> clazz = rec.getClass();
            if (clazz.getName().contains("ProxyChangeListener")) {
                Method onReceiveMethod =
                        clazz.getDeclaredMethod("onReceive", Context.class, Intent.class);
                Intent intent = new Intent(Proxy.PROXY_CHANGE_ACTION);
                onReceiveMethod.invoke(rec, mAppContext, intent);
            }
        }

        Mockito.verify(mDelegate, Mockito.times(1)).proxySettingsChanged();
    }
}
