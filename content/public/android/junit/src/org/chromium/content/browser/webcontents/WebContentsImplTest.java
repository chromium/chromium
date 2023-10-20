// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.ui.base.EventForwarder;

/** Unit tests for {@link WebContentsImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebContentsImplTest {
    @Mock private NavigationController mNavigationController;
    @Mock private WebContentsImpl.Natives mWebContentsJniMock;
    @Rule public JniMocker mJniMocker = new JniMocker();

    private WebContentsImpl mWebContentsImpl;
    private final long mNativeWebContentsAndroid = 1;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebContentsImplJni.TEST_HOOKS, mWebContentsJniMock);
        mWebContentsImpl = WebContentsImpl.create(mNativeWebContentsAndroid, mNavigationController);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testSetAndGetStylusWritingHandler() {
        mWebContentsImpl.setStylusWritingHandler(mock(StylusWritingHandler.class));
        verify(mWebContentsJniMock).setStylusHandwritingEnabled(mNativeWebContentsAndroid, true);
        assertNotNull(mWebContentsImpl.getStylusWritingHandler());

        mWebContentsImpl.setStylusWritingHandler(null);
        verify(mWebContentsJniMock).setStylusHandwritingEnabled(mNativeWebContentsAndroid, false);
        assertNull(mWebContentsImpl.getStylusWritingHandler());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testSetAndGetStylusWritingHandler_nativeCleared() {
        mWebContentsImpl.clearNativePtr();
        mWebContentsImpl.setStylusWritingHandler(mock(StylusWritingHandler.class));
        verify(mWebContentsJniMock, never())
                .setStylusHandwritingEnabled(mNativeWebContentsAndroid, true);
        assertNotNull(mWebContentsImpl.getStylusWritingHandler());

        mWebContentsImpl.setStylusWritingHandler(null);
        verify(mWebContentsJniMock, never())
                .setStylusHandwritingEnabled(mNativeWebContentsAndroid, false);
        assertNull(mWebContentsImpl.getStylusWritingHandler());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testSetStylusWritingDelegate() {
        EventForwarder eventForwarder = mock(EventForwarder.class);
        doReturn(eventForwarder)
                .when(mWebContentsJniMock)
                .getOrCreateEventForwarder(mNativeWebContentsAndroid);
        assertEquals(eventForwarder, mWebContentsImpl.getEventForwarder());
        verify(eventForwarder).setStylusWritingDelegate(any());
    }
}
