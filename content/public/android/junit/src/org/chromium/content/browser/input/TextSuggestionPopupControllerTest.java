// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
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
        when(mRenderCoordinates.getContentOffsetTopPix()).thenReturn(50.0f);
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
        when(mWebContents.getContext()).thenReturn(mContext);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
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

    @Test
    public void testCreateAndShowWithNullContext() {
        when(mWebContents.getContext()).thenReturn(null);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(null);

        TextSuggestionPopupController controller = new TextSuggestionPopupController(mWebContents);
        doReturn(controller).when(mWebContents).getOrSetUserData(any(), any());
        controller.onAttachedToWindow();

        JniRawPtr<TextSuggestionHost.NativeTextSuggestionHostAndroid> nativePtr =
                JniRawPtr.createForTesting(555L);
        TextSuggestionHost host =
                TextSuggestionPopupController.createTextSuggestionHost(mWebContents, nativePtr);
        assertNotNull(host);

        TextSuggestionPopupController.showSpellCheckSuggestionMenu(
                mWebContents, host, 10.0, 20.0, "teh", new String[] {"the"});
        TextSuggestionPopupController.showTextSuggestionMenu(
                mWebContents,
                host,
                10.0,
                20.0,
                "teh",
                new SuggestionInfo[] {new SuggestionInfo(1, 0, "", "the", "")});

        assertNull(controller.getSpellCheckPopupWindowForTesting());
        assertNull(controller.getTextSuggestionsPopupWindowForTesting());
        verify(mTextSuggestionHostJniMock, times(2)).onSuggestionMenuClosed(eq(nativePtr));
        controller.destroy();
    }

    @Test
    public void testOnWindowAndroidChangedHidesPopupAndUpdatesContext() {
        Context initialContext = new ContextThemeWrapper(mContext, 0);
        Context updatedContext = new ContextThemeWrapper(mContext, 0);
        WindowAndroid newWindowAndroid = mock(WindowAndroid.class);
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(null));
        when(newWindowAndroid.getActivity()).thenReturn(new WeakReference<>(null));
        when(mWebContents.getContext()).thenReturn(initialContext);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);

        TextSuggestionPopupController controller = new TextSuggestionPopupController(mWebContents);
        doReturn(controller).when(mWebContents).getOrSetUserData(any(), any());
        controller.onAttachedToWindow();

        JniRawPtr<TextSuggestionHost.NativeTextSuggestionHostAndroid> nativePtr =
                JniRawPtr.createForTesting(666L);
        TextSuggestionHost host =
                TextSuggestionPopupController.createTextSuggestionHost(mWebContents, nativePtr);
        assertNotNull(host);

        TextSuggestionPopupController.showSpellCheckSuggestionMenu(
                mWebContents, host, 10.0, 20.0, "teh", new String[] {"the"});
        SuggestionsPopupWindow firstPopup = controller.getSpellCheckPopupWindowForTesting();
        assertNotNull(firstPopup);
        assertSame(initialContext, firstPopup.getContentViewForTesting().getContext());

        controller.onWindowAndroidChanged(null);
        assertNull(controller.getSpellCheckPopupWindowForTesting());

        when(mWebContents.getContext()).thenReturn(updatedContext);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(newWindowAndroid);
        controller.onWindowAndroidChanged(newWindowAndroid);

        TextSuggestionPopupController.showSpellCheckSuggestionMenu(
                mWebContents, host, 10.0, 20.0, "teh", new String[] {"the"});
        SuggestionsPopupWindow secondPopup = controller.getSpellCheckPopupWindowForTesting();
        assertNotNull(secondPopup);
        assertSame(updatedContext, secondPopup.getContentViewForTesting().getContext());

        TextSuggestionPopupController.showTextSuggestionMenu(
                mWebContents,
                host,
                10.0,
                20.0,
                "teh",
                new SuggestionInfo[] {new SuggestionInfo(1, 0, "", "the", "")});
        SuggestionsPopupWindow textSuggestionsPopup =
                controller.getTextSuggestionsPopupWindowForTesting();
        assertNotNull(textSuggestionsPopup);
        assertSame(updatedContext, textSuggestionsPopup.getContentViewForTesting().getContext());

        controller.destroy();
    }
}
