// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.res.Configuration;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link WindowEventObserverManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WindowEventObserverManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContentsImpl mWebContents;
    @Mock private WindowAndroid mWindowAndroid1;
    @Mock private WindowAndroid mWindowAndroid2;
    @Mock private DisplayAndroid mDisplay1;
    @Mock private DisplayAndroid mDisplay2;
    @Mock private WindowEventObserver mObserver;

    private WindowEventObserverManager mManager;

    @Before
    public void setUp() {
        // Mock WebContents user data host behavior using a simple map
        Map<Class<?>, Object> userDataMap = new HashMap<>();

        when(mWebContents.getOrSetUserData(any(), any()))
                .thenAnswer(
                        invocation -> {
                            Class<?> clazz = invocation.getArgument(0);
                            UserDataFactory<?> factory = invocation.getArgument(1);
                            Object data = userDataMap.get(clazz);
                            if (data == null && factory != null) {
                                data = factory.create(mWebContents);
                                userDataMap.put(clazz, data);
                            }
                            return data;
                        });

        // Setup mock window and display relationships
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid1);
        when(mWindowAndroid1.getDisplay()).thenReturn(mDisplay1);
        when(mWindowAndroid2.getDisplay()).thenReturn(mDisplay2);

        // Mock display properties to avoid null pointers during init
        when(mDisplay1.getRotation()).thenReturn(1);
        when(mDisplay1.getDipScale()).thenReturn(1.0f);
        when(mDisplay2.getRotation()).thenReturn(90);
        when(mDisplay2.getDipScale()).thenReturn(2.0f);

        mManager = WindowEventObserverManager.from(mWebContents);
        mManager.addObserver(mObserver);
    }

    @Test
    public void testAttachToWindow_observesDisplay() {
        mManager.onAttachedToWindow();

        // Verify it observes the display of the current window
        verify(mDisplay1).addObserver(mManager);
        verify(mObserver).onRotationChanged(1);
        verify(mObserver).onDIPScaleChanged(1.0f);
    }

    @Test
    public void testWindowAndroidChanged_switchesDisplayObserver() {
        mManager.onAttachedToWindow();
        verify(mDisplay1).addObserver(mManager);

        // Change WindowAndroid
        mManager.onWindowAndroidChanged(mWindowAndroid2);

        // Verify it removes observer from old display and adds to new display
        verify(mDisplay1).removeObserver(mManager);
        verify(mDisplay2).addObserver(mManager);

        // Verify new display properties are propagated
        verify(mObserver).onRotationChanged(90);
        verify(mObserver).onDIPScaleChanged(2.0f);
    }

    @Test
    public void testConfigurationChanged_updatesDisplayObserver() {
        mManager.onAttachedToWindow();
        verify(mDisplay1).addObserver(mManager);

        // Simulate display change on configuration change (e.g. unfold)
        // by making the window return a different display.
        when(mWindowAndroid1.getDisplay()).thenReturn(mDisplay2);

        mManager.onConfigurationChanged(new Configuration());

        // Verify it updates observer to the new display
        verify(mDisplay1).removeObserver(mManager);
        verify(mDisplay2).addObserver(mManager);

        // Verify new display properties are propagated
        verify(mObserver).onRotationChanged(90);
        verify(mObserver).onDIPScaleChanged(2.0f);
    }

    @Test
    public void testDetachFromWindow_removesObserver() {
        mManager.onAttachedToWindow();
        verify(mDisplay1).addObserver(mManager);

        mManager.onDetachedFromWindow();

        verify(mDisplay1).removeObserver(mManager);
    }
}
