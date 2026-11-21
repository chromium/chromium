// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.Dialog;
import android.os.Looper;

import org.jni_zero.JniRawPtr;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowDialog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.ime.TextInputType;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link DateTimeChooserAndroid}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DateTimeChooserAndroidTest {
    // Defined in third_party/blink/renderer/platform/text/date_components.h.
    private static final double DATE_DIALOG_DEFAULT_MIN = -62135596800000.0;
    private static final double DATE_DIALOG_DEFAULT_MAX = 8640000000000000.0;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContentsImpl mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WindowAndroid mNewWindowAndroid;
    @Mock private DateTimeChooserAndroid.Natives mNatives;

    private final JniRawPtr<DateTimeChooserAndroid.NativeDateTimeChooserAndroid>
            mNativeDateTimeChooser = JniRawPtr.createForTesting(1L);

    @Before
    public void setUp() {
        DateTimeChooserAndroidJni.setInstanceForTesting(mNatives);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(activity));
        Map<Class<?>, Object> userData = new HashMap<>();
        when(mWebContents.getOrSetUserData(any(), any()))
                .thenAnswer(
                        invocation -> {
                            Class<?> key = invocation.getArgument(0);
                            Object data = userData.get(key);
                            if (data == null) {
                                UserDataFactory factory = invocation.getArgument(1);
                                data = factory.create(mWebContents);
                                userData.put(key, data);
                            }
                            return data;
                        });
    }

    @Test
    public void testCancelsAndDismissesWhenWindowChanges() {
        DateTimeChooserAndroid chooser =
                DateTimeChooserAndroid.createDateTimeChooser(
                        mWebContents,
                        mWindowAndroid,
                        mNativeDateTimeChooser,
                        TextInputType.DATE,
                        /* dialogValue= */ 0.0,
                        DATE_DIALOG_DEFAULT_MIN,
                        DATE_DIALOG_DEFAULT_MAX,
                        /* step= */ 1.0,
                        /* suggestions= */ null);
        assertNotNull(chooser);
        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue(dialog.isShowing());

        // Mirror native DateTimeChooserAndroid::CancelDialog(), which calls dismissAndDestroy().
        doAnswer(
                        invocation -> {
                            chooser.dismissAndDestroy();
                            return null;
                        })
                .when(mNatives)
                .cancelDialog(any());

        // The WebContents moves to another window, e.g. for a new Activity.
        WindowEventObserverManager.from(mWebContents).onWindowAndroidChanged(mNewWindowAndroid);
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertFalse(dialog.isShowing());
        verify(mNatives, times(1)).cancelDialog(any());

        // A further window change after cleanup must not cancel again.
        WindowEventObserverManager.from(mWebContents).onWindowAndroidChanged(mWindowAndroid);
        verify(mNatives, times(1)).cancelDialog(any());
    }
}
