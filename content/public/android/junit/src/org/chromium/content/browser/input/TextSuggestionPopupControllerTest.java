// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.Map;

/** Unit tests for {@link TextSuggestionPopupController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TextSuggestionPopupControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private TextSuggestionPopupController mController;

    @Mock private WebContentsImpl mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ViewAndroidDelegate mViewDelegate;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();

        when(mWebContents.getContext()).thenReturn(mContext);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(mViewDelegate);

        when(mWebContents.getOrSetUserData(any(), any()))
                .thenAnswer(
                        invocation -> {
                            UserDataFactory factory = invocation.getArgument(1);
                            return factory.create(mWebContents);
                        });

        mController = new TextSuggestionPopupController(mWebContents);
    }

    @Test
    public void testDestroyClearsNativePtrsAndHosts() {
        long nativePtr1 = 111L;
        long nativePtr2 = 222L;

        TextSuggestionHost host1 = new TextSuggestionHost(nativePtr1, () -> {});
        TextSuggestionHost host2 = new TextSuggestionHost(nativePtr2, () -> {});

        // Inject hosts into TextSuggestionPopupController.
        Map<Long, TextSuggestionHost> map = mController.getTextSuggestionHostsForTesting();
        map.put(nativePtr1, host1);
        map.put(nativePtr2, host2);

        mController.setLastShownHostForTesting(host1);

        // Verify pre-conditions.
        assertEquals(nativePtr1, host1.getNativePtrForTesting());
        assertEquals(nativePtr2, host2.getNativePtrForTesting());
        assertEquals(2, map.size());
        assertEquals(host1, mController.getLastShownHostForTesting());

        // Call destroy.
        mController.destroy();

        // Verify that native pointers in both hosts are cleared.
        assertEquals(0, host1.getNativePtrForTesting());
        assertEquals(0, host2.getNativePtrForTesting());

        // Verify that map and field are cleared.
        assertTrue(map.isEmpty());
        assertNull(mController.getLastShownHostForTesting());
    }
}
