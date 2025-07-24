// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.InputDevice;
import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImplJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.MotionEventTestUtils;
import org.chromium.ui.util.MotionEventUtils;

/** Unit tests for {@link ContentUiEventHandler} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContentUiEventHandlerTest {
    private static final long NATIVE_WEB_CONTENTS_ANDROID = 1;
    private static final long NATIVE_CONTENT_UI_EVENT_HANDLER = 2;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NavigationController mNavigationController;
    @Mock private WebContentsImpl.Natives mWebContentsJniMock;
    @Mock private ContentUiEventHandler.Natives mContentUiEventHandlerJniMock;

    private ContentUiEventHandler mContentUiEventHandler;

    @Before
    public void setUp() {
        WebContentsImplJni.setInstanceForTesting(mWebContentsJniMock);
        ContentUiEventHandlerJni.setInstanceForTesting(mContentUiEventHandlerJniMock);

        WebContentsImpl webContentsImpl =
                spy(WebContentsImpl.create(NATIVE_WEB_CONTENTS_ANDROID, mNavigationController));
        webContentsImpl.initializeForTesting();

        Gamepad gamepad = mock(Gamepad.class);
        when(gamepad.onGenericMotionEvent(any())).thenReturn(false);
        webContentsImpl.setUserDataForTesting(Gamepad.class, gamepad);

        JoystickHandler joystickHandler = mock(JoystickHandler.class);
        when(joystickHandler.onGenericMotionEvent(any())).thenReturn(false);
        webContentsImpl.setUserDataForTesting(JoystickHandler.class, joystickHandler);

        EventForwarder eventForwarder = mock(EventForwarder.class);
        when(eventForwarder.isTrackpadToMouseEventConversionEnabled()).thenReturn(true);
        when(eventForwarder.createOffsetMotionEventIfNeeded(any()))
                .thenAnswer(
                        (Answer<MotionEvent>)
                                invocation -> {
                                    Object[] args = invocation.getArguments();
                                    return (MotionEvent) args[0];
                                });
        doReturn(eventForwarder).when(webContentsImpl).getEventForwarder();

        mContentUiEventHandler =
                ContentUiEventHandler.createForTesting(
                        webContentsImpl, NATIVE_CONTENT_UI_EVENT_HANDLER);
    }

    @Test
    public void testOnGenericMotionEventSendsTrackpadClicksToNative() {
        MotionEvent trackpadLeftClickEvent = getTrackpadLeftClickEvent();
        mContentUiEventHandler.onGenericMotionEvent(getTrackpadLeftClickEvent());

        MotionEvent trackpadRightClickEvent = getTrackRightClickEvent();
        mContentUiEventHandler.onGenericMotionEvent(getTrackRightClickEvent());

        ArgumentCaptor<MotionEvent> captor = ArgumentCaptor.forClass(MotionEvent.class);
        verify(mContentUiEventHandlerJniMock, times(2))
                .sendMouseEvent(
                        eq(NATIVE_CONTENT_UI_EVENT_HANDLER),
                        captor.capture(),
                        eq(MotionEventUtils.getEventTimeNanos(trackpadLeftClickEvent)),
                        eq(EventForwarder.getMouseEventActionButton(trackpadLeftClickEvent)),
                        eq(MotionEvent.TOOL_TYPE_MOUSE));

        MotionEventTestUtils.assertEquals(captor.getAllValues().get(0), trackpadLeftClickEvent);
        MotionEventTestUtils.assertEquals(captor.getAllValues().get(1), trackpadRightClickEvent);
    }

    private static MotionEvent getTrackpadLeftClickEvent() {
        return getTrackpadEvent(MotionEvent.ACTION_BUTTON_PRESS, MotionEvent.BUTTON_PRIMARY);
    }

    private static MotionEvent getTrackRightClickEvent() {
        return getTrackpadEvent(MotionEvent.ACTION_BUTTON_PRESS, MotionEvent.BUTTON_SECONDARY);
    }

    private static MotionEvent getTrackpadEvent(int action, int buttonState) {
        return MotionEvent.obtain(
                0,
                1,
                action,
                1,
                getToolTypeFingerProperties(),
                getPointerCoords(),
                0,
                buttonState,
                0,
                0,
                0,
                0,
                getTrackpadSource(),
                0);
    }

    private static MotionEvent.PointerProperties[] getToolTypeFingerProperties() {
        MotionEvent.PointerProperties[] pointerPropertiesArray =
                new MotionEvent.PointerProperties[1];
        MotionEvent.PointerProperties trackpadProperties = new MotionEvent.PointerProperties();
        trackpadProperties.id = 7;
        trackpadProperties.toolType = MotionEvent.TOOL_TYPE_FINGER;
        pointerPropertiesArray[0] = trackpadProperties;
        return pointerPropertiesArray;
    }

    private static MotionEvent.PointerCoords[] getPointerCoords() {
        MotionEvent.PointerCoords[] pointerCoordsArray = new MotionEvent.PointerCoords[1];
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = 14;
        coords.y = 21;
        pointerCoordsArray[0] = coords;
        return pointerCoordsArray;
    }

    private static int getTrackpadSource() {
        return InputDevice.SOURCE_MOUSE;
    }
}
