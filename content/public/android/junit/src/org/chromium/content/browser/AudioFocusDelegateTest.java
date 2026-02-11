// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.media.AudioFocusRequest;
import android.media.AudioManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.content.browser.AudioFocusDelegate.AudioFocusRequestResult;
import org.chromium.content_public.browser.ContentFeatureList;

import java.lang.reflect.Method;

/** Unit tests for {@link AudioFocusDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ContentFeatureList.ALLOW_DELAYED_AUDIO_FOCUS_GAIN_ANDROID)
public class AudioFocusDelegateTest {
    private static final String AUDIO_FOCUS_REQUEST_RESULT_HISTOGRAM =
            "Media.Android.AudioFocusRequestResult";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private AudioManager mAudioManager;

    private AudioFocusDelegate mDelegate;
    private final long mNativeAudioFocusDelegateAndroid = 1234L;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(mContext);
        doReturn(mAudioManager).when(mContext).getSystemService(Context.AUDIO_SERVICE);
    }

    private AudioFocusDelegate createDelegate() {
        try {
            Method method = AudioFocusDelegate.class.getDeclaredMethod("create", long.class);
            method.setAccessible(true);
            return (AudioFocusDelegate) method.invoke(null, mNativeAudioFocusDelegateAndroid);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private boolean requestAudioFocus(boolean transientFocus) {
        try {
            Method method =
                    AudioFocusDelegate.class.getDeclaredMethod("requestAudioFocus", boolean.class);
            method.setAccessible(true);
            return (boolean) method.invoke(mDelegate, transientFocus);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @Test
    public void testRequestAudioFocus_Granted() {
        mDelegate = createDelegate();
        doReturn(AudioManager.AUDIOFOCUS_REQUEST_GRANTED)
                .when(mAudioManager)
                .requestAudioFocus(any(AudioFocusRequest.class));

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AUDIO_FOCUS_REQUEST_RESULT_HISTOGRAM, AudioFocusRequestResult.GRANTED);

        boolean result = requestAudioFocus(false);

        assertTrue(result);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRequestAudioFocus_Failed() {
        mDelegate = createDelegate();
        doReturn(AudioManager.AUDIOFOCUS_REQUEST_FAILED)
                .when(mAudioManager)
                .requestAudioFocus(any(AudioFocusRequest.class));

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AUDIO_FOCUS_REQUEST_RESULT_HISTOGRAM, AudioFocusRequestResult.FAILED);

        boolean result = requestAudioFocus(false);

        assertFalse(result);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRequestAudioFocus_Delayed() {
        mDelegate = createDelegate();
        doReturn(AudioManager.AUDIOFOCUS_REQUEST_DELAYED)
                .when(mAudioManager)
                .requestAudioFocus(any(AudioFocusRequest.class));

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AUDIO_FOCUS_REQUEST_RESULT_HISTOGRAM, AudioFocusRequestResult.DELAYED);

        boolean result = requestAudioFocus(false);

        assertTrue(result);
        histogramWatcher.assertExpected();
    }
}
