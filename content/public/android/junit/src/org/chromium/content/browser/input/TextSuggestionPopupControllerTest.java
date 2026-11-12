// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.jni_zero.JniRawPtr;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.Set;

/** Unit tests for {@link TextSuggestionPopupController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TextSuggestionPopupControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private ViewGroup mContainerView;
    private TextSuggestionPopupController mController;

    @Mock private WebContentsImpl mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private RenderCoordinatesImpl mRenderCoordinates;
    @Mock private TextSuggestionHost.Natives mTextSuggestionHostJniMock;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContainerView = new FrameLayout(mContext);
        TextSuggestionHostJni.setInstanceForTesting(mTextSuggestionHostJniMock);

        ViewAndroidDelegate viewDelegate = ViewAndroidDelegate.createBasicDelegate(mContainerView);
        when(mWebContents.getContext()).thenReturn(mContext);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(viewDelegate);

        when(mWebContents.getOrSetUserData(any(), any()))
                .thenAnswer(
                        invocation -> {
                            UserDataFactory factory = invocation.getArgument(1);
                            return factory.create(mWebContents);
                        });

        mController = new TextSuggestionPopupController(mWebContents);
    }

    @Test
    public void testCalculateYOffset() {
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mRenderCoordinates.getContentOffsetYPix()).thenReturn(50.0f);
        mContainerView.layout(0, 0, 100, 1000);

        assertEquals(200.0, mController.calculateYOffset(150.0), 0.001);
        assertEquals(50.0, mController.calculateYOffset(-100.0), 0.001);
        assertEquals(50.0, mController.calculateYOffset(Double.NEGATIVE_INFINITY), 0.001);
        assertEquals(50.0, mController.calculateYOffset(Double.NaN), 0.001);
        assertEquals(1000.0, mController.calculateYOffset(2000.0), 0.001);
        assertEquals(1000.0, mController.calculateYOffset(Double.POSITIVE_INFINITY), 0.001);
    }

    @Test
    public void testDestroyClearsNativePtrsAndHosts() {
        doReturn(mController).when(mWebContents).getOrSetUserData(any(), any());
        JniRawPtr<TextSuggestionHost.NativeTextSuggestionHostAndroid> nativePtr1 =
                JniRawPtr.createForTesting(111L);
        JniRawPtr<TextSuggestionHost.NativeTextSuggestionHostAndroid> nativePtr2 =
                JniRawPtr.createForTesting(222L);

        TextSuggestionHost host1 =
                TextSuggestionPopupController.createTextSuggestionHost(mWebContents, nativePtr1);
        TextSuggestionHost host2 =
                TextSuggestionPopupController.createTextSuggestionHost(mWebContents, nativePtr2);
        assertNotNull(host1);
        assertNotNull(host2);

        Set<TextSuggestionHost> hosts = mController.getTextSuggestionHostsForTesting();
        mController.setLastShownHostForTesting(host1);

        assertEquals(nativePtr1, host1.getNativePtrForTesting());
        assertEquals(nativePtr2, host2.getNativePtrForTesting());
        assertEquals(2, hosts.size());
        assertEquals(host1, mController.getLastShownHostForTesting());

        mController.destroy();

        assertNull(host1.getNativePtrForTesting());
        assertNull(host2.getNativePtrForTesting());
        assertTrue(hosts.isEmpty());
        assertNull(mController.getLastShownHostForTesting());
    }

    @Test
    public void testStaleDismissDoesNotInvalidateSubsequentPopup() {
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(null));
        doReturn(mController).when(mWebContents).getOrSetUserData(any(), any());
        mController.onAttachedToWindow();

        JniRawPtr<TextSuggestionHost.NativeTextSuggestionHostAndroid> nativePtr =
                JniRawPtr.createForTesting(333L);
        TextSuggestionHost host =
                TextSuggestionPopupController.createTextSuggestionHost(mWebContents, nativePtr);
        assertNotNull(host);

        TextSuggestionPopupController.showSpellCheckSuggestionMenu(
                mWebContents, host, 10.0, 20.0, "teh", new String[] {"the"});
        SuggestionsPopupWindow firstPopup = mController.getSpellCheckPopupWindowForTesting();
        assertNotNull(firstPopup);

        TextSuggestionPopupController.showSpellCheckSuggestionMenu(
                mWebContents, host, 10.0, 20.0, "teh", new String[] {"the"});
        SuggestionsPopupWindow secondPopup = mController.getSpellCheckPopupWindowForTesting();
        assertNotNull(secondPopup);

        // Simulate Android firing the first popup's async dismissal after the second menu opens.
        firstPopup.onDismiss();

        secondPopup.onItemClick(null, null, 0, 0);
        verify(mTextSuggestionHostJniMock).applySpellCheckSuggestion(eq(nativePtr), eq("the"));

        mController.destroy();
    }

    @Test
    public void testCreateTextSuggestionHostWithoutControllerReleasesNativePtr() {
        WebContentsImpl webContentsWithoutController = mock(WebContentsImpl.class);
        when(webContentsWithoutController.getOrSetUserData(any(), any())).thenReturn(null);

        JniRawPtr<TextSuggestionHost.NativeTextSuggestionHostAndroid> nativePtr =
                JniRawPtr.createForTesting(444L);
        assertNull(
                TextSuggestionPopupController.createTextSuggestionHost(
                        webContentsWithoutController, nativePtr));
        LifetimeAssert.assertAllInstancesDestroyedForTesting();
    }
}
