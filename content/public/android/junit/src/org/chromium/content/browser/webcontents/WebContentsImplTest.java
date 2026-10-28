// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.os.Parcel;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventForwarder;

/** Unit tests for {@link WebContentsImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebContentsImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private NavigationController mNavigationController;
    @Mock private WebContentsImpl.Natives mWebContentsJniMock;
    @Mock private EventForwarder mEventForwarder;
    @Mock private StylusWritingHandler mStylusWritingHandler;

    private WebContentsImpl mWebContentsImpl;
    private final long mNativeWebContentsAndroid = 1;

    @Before
    public void setUp() {
        WebContentsImplJni.setInstanceForTesting(mWebContentsJniMock);
        mWebContentsImpl = WebContentsImpl.create(mNativeWebContentsAndroid, mNavigationController);
    }

    @After
    public void tearDown() {
        mWebContentsImpl.destroy();
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testSetAndGetStylusWritingHandler() {
        mWebContentsImpl.setStylusWritingHandler(mStylusWritingHandler);
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
        mWebContentsImpl.setStylusWritingHandler(mStylusWritingHandler);
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
        doReturn(mEventForwarder)
                .when(mWebContentsJniMock)
                .getOrCreateEventForwarder(mNativeWebContentsAndroid);
        assertEquals(mEventForwarder, mWebContentsImpl.getEventForwarder());
        verify(mEventForwarder).setStylusWritingDelegate(any());
    }

    private static Parcel writeToParcel(WebContents webContents) {
        Parcel parcel = Parcel.obtain();
        webContents.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        return parcel;
    }

    @Test
    public void testParcelRoundTrip() {
        Parcel parcel = writeToParcel(mWebContentsImpl);
        assertSame(mWebContentsImpl, WebContentsImpl.CREATOR.createFromParcel(parcel));
        parcel.recycle();
    }

    @Test
    public void testParcelAfterNativeCleared() {
        Parcel parcel = writeToParcel(mWebContentsImpl);
        mWebContentsImpl.clearNativePtr();
        assertNull(WebContentsImpl.CREATOR.createFromParcel(parcel));
        parcel.recycle();
    }

    @Test
    public void testParcelAfterPeerRecreated() {
        Parcel parcel = writeToParcel(mWebContentsImpl);
        mWebContentsImpl.clearNativePtr();
        // A new peer at the same native address must not be returned for the old parcel.
        WebContentsImpl recreated =
                WebContentsImpl.create(mNativeWebContentsAndroid, mNavigationController);
        assertNull(WebContentsImpl.CREATOR.createFromParcel(parcel));
        parcel.recycle();
        recreated.destroy();
    }
}
