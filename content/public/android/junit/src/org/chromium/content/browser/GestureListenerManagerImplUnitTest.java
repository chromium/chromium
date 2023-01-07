// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Point;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink.mojom.EventType;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.GestureStateListenerWithScroll;
import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Unit test for {@link GestureListenerManagerImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class GestureListenerManagerImplUnitTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    WebContentsImpl mWebContents;
    @Mock
    ViewGroup mViewGroup;
    @Mock
    GestureListenerManagerImpl.Natives mMockJniGestureListenerManager;
    @Mock
    GestureStateListenerWithScroll mGestureStateListener;
    @Captor
    ArgumentCaptor<Point> mPointCaptor;

    private GestureListenerManagerImpl mGestureManager;

    @Before
    public void setup() {
        mJniMocker.mock(GestureListenerManagerImplJni.TEST_HOOKS, mMockJniGestureListenerManager);
        doReturn(1L).when(mMockJniGestureListenerManager).init(any(), any());

        setupMockWebContents();

        mGestureManager = new GestureListenerManagerImpl(mWebContents);
        mGestureManager.addListener(mGestureStateListener);
    }

    @Test
    public void verticalScrollDirectionChanged() {
        GestureStateListener listener1 = Mockito.mock(GestureStateListener.class);
        GestureStateListener listener2 = Mockito.mock(GestureStateListener.class);

        mGestureManager.addListener(listener1);
        mGestureManager.addListener(listener2);

        // Verify all listener gets the update.
        mGestureManager.onVerticalScrollDirectionChanged(true, 0.1f);
        verify(mGestureStateListener).onVerticalScrollDirectionChanged(true, 0.1f);
        verify(listener1).onVerticalScrollDirectionChanged(true, 0.1f);
        verify(listener2).onVerticalScrollDirectionChanged(true, 0.1f);
    }

    @Test
    public void scrollBegin() {
        // In production the call started from cpp, and passed into Java via JNI. In this test
        // we start the scroll from Java directly.
        mGestureManager.onScrollBegin(true);
        Assert.assertTrue("Scroll should started.", mGestureManager.isScrollInProgress());
        verify(mGestureStateListener).onScrollStarted(anyInt(), anyInt(), eq(true));

        mGestureManager.onEventAck(EventType.GESTURE_SCROLL_UPDATE, /*consumed*/ true,
                /*scrollOffsetX*/ 0.f, /*scrollOffsetY*/ 1.f);
        mGestureManager.onEventAck(EventType.GESTURE_SCROLL_UPDATE, /*consumed*/ true,
                /*scrollOffsetX*/ 1.f, /*scrollOffsetY*/ 0.f);
        verify(mGestureStateListener, times(2))
                .onScrollUpdateGestureConsumed(mPointCaptor.capture());

        Assert.assertEquals(
                "Number of points captured is different.", 2, mPointCaptor.getAllValues().size());
        Assert.assertEquals(
                "Points doesn't match.", new Point(0, 1), mPointCaptor.getAllValues().get(0));
        Assert.assertEquals(
                "Points doesn't match.", new Point(1, 0), mPointCaptor.getAllValues().get(1));

        mGestureManager.onEventAck(EventType.GESTURE_SCROLL_END, /*consumed*/ true,
                /*scrollOffsetX*/ 0.f, /*scrollOffsetY*/ 0.f);
        verify(mGestureStateListener).onScrollEnded(anyInt(), anyInt());
    }

    @Test
    public void scrollBeginThenAbort() {
        // In production the call started from cpp, and passed into Java via JNI. In this test
        // we start the scroll from Java directly.
        mGestureManager.onScrollBegin(true);
        Assert.assertTrue("Scroll should started.", mGestureManager.isScrollInProgress());
        verify(mGestureStateListener).onScrollStarted(anyInt(), anyInt(), eq(true));

        mGestureManager.resetPopupsAndInput(false);
        verify(mGestureStateListener).onScrollEnded(anyInt(), anyInt());
    }

    @Test
    public void flingBegin() {
        // In production the call started from cpp, and passed into Java via JNI. In this test
        // we start the scroll from Java directly.
        mGestureManager.onFlingStart(true);
        Assert.assertTrue("Fling should started.", mGestureManager.hasActiveFlingScroll());
        verify(mGestureStateListener).onFlingStartGesture(anyInt(), anyInt(), eq(true));

        mGestureManager.onFlingEnd();
        verify(mGestureStateListener).onFlingEndGesture(anyInt(), anyInt());
    }

    private void setupMockWebContents() {
        ViewAndroidDelegate viewAndroidDelegate =
                ViewAndroidDelegate.createBasicDelegate(mViewGroup);
        RenderCoordinatesImpl renderCoordinates = new RenderCoordinatesImpl();
        doReturn(viewAndroidDelegate).when(mWebContents).getViewAndroidDelegate();
        doReturn(renderCoordinates).when(mWebContents).getRenderCoordinates();

        // Setup UserData involved in the scrolling process.
        doAnswer(invocation -> {
            if (invocation.getArgument(0) == SelectionPopupControllerImpl.class) {
                return SelectionPopupControllerImpl.createForTesting(mWebContents);
            }
            UserDataFactory factory = invocation.getArgument(1);
            return factory.create(mWebContents);
        })
                .when(mWebContents)
                .getOrSetUserData(/*key=*/any(),
                        /*userDataFactory*/ any());
    }
}
