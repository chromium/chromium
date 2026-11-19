// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.ContextWrapper;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link WebContentsAccessibilityImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ContentFeatures.ACCESSIBILITY_MANAGE_BROADCAST_RECEIVER_ON_BACKGROUND,
    ContentFeatures.ACCESSIBILITY_REQUEST_SCOPED_CONTENT_CHANGED_EVENTS,
})
public class WebContentsAccessibilityImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AccessibilityDelegate mDelegate;

    private Context mInitialContext;
    private WebContentsAccessibilityImpl mWebContentsAccessibility;

    @Before
    public void setUp() {
        mInitialContext = new ContextWrapper(ApplicationProvider.getApplicationContext());
        when(mDelegate.getContainerView()).thenReturn(new View(mInitialContext));
        mWebContentsAccessibility = new WebContentsAccessibilityImpl(mDelegate);
    }

    @After
    public void tearDown() {
        mWebContentsAccessibility.destroy();
    }

    @Test
    public void testOnWindowAndroidChangedUpdatesAndClearsContext() {
        assertSame(mInitialContext, mWebContentsAccessibility.mContext);

        mWebContentsAccessibility.onWindowAndroidChanged(null);
        assertSame(ContextUtils.getApplicationContext(), mWebContentsAccessibility.mContext);

        Context updatedContext = new ContextWrapper(ApplicationProvider.getApplicationContext());
        WindowAndroid windowWithContext = mock(WindowAndroid.class);
        when(windowWithContext.getContext()).thenReturn(new WeakReference<>(updatedContext));
        mWebContentsAccessibility.onWindowAndroidChanged(windowWithContext);
        assertSame(updatedContext, mWebContentsAccessibility.mContext);

        WindowAndroid windowWithoutContext = mock(WindowAndroid.class);
        when(windowWithoutContext.getContext()).thenReturn(new WeakReference<>(null));
        mWebContentsAccessibility.onWindowAndroidChanged(windowWithoutContext);
        assertSame(ContextUtils.getApplicationContext(), mWebContentsAccessibility.mContext);
    }
}
