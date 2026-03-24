// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContentsObserver;

/** Unit tests for {@link WebContentsObserverProxy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebContentsObserverProxyTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private WebContentsObserver mWebContentsObserver;
    @Mock private WebContentsObserver mWebContentsObserver2;
    @Mock private WebContentsObserverProxy.Natives mWebContentsObserverProxyJni;

    private WebContentsImpl mWebContentsImpl;
    private static final long M_NATIVE_WEB_CONTENTS_ANDROID = 1;

    @Before
    public void setUp() {
        WebContentsObserverProxyJni.setInstanceForTesting(mWebContentsObserverProxyJni);
        when(mWebContentsObserverProxyJni.init(any())).thenReturn(1L);
    }

    @Test
    public void testChainingEvents() {
        InOrder inOrder = Mockito.inOrder(mWebContentsObserver, mWebContentsObserver2);
        final WebContentsObserverProxy proxy = new WebContentsObserverProxy(null);
        proxy.addObserver(mWebContentsObserver);
        proxy.addObserver(mWebContentsObserver2);
        Mockito.doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                proxy.navigationEntriesChanged();
                                return null;
                            }
                        })
                .when(mWebContentsObserver)
                .navigationEntriesDeleted();
        proxy.navigationEntriesDeleted();
        inOrder.verify(mWebContentsObserver).navigationEntriesDeleted();
        inOrder.verify(mWebContentsObserver).navigationEntriesChanged();
        inOrder.verify(mWebContentsObserver2).navigationEntriesChanged();
        inOrder.verify(mWebContentsObserver2).navigationEntriesDeleted();
        inOrder.verifyNoMoreInteractions();
    }
}
