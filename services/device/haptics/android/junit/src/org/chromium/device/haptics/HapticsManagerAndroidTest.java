// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.haptics;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.media.AudioAttributes;
import android.os.Build;
import android.os.VibrationAttributes;
import android.os.VibrationEffect;
import android.os.Vibrator;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.device.mojom.HapticEffect;

/** Unit tests for {@link HapticsManagerAndroid}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.TIRAMISU)
public class HapticsManagerAndroidTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Vibrator mSystemVibrator;

    private HapticsManagerAndroid mHapticsManager;

    @Before
    public void setUp() {
        when(mSystemVibrator.hasVibrator()).thenReturn(true);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            when(mSystemVibrator.areAllPrimitivesSupported(anyInt())).thenReturn(true);
        }

        mHapticsManager =
                new HapticsManagerAndroid(mSystemVibrator, /* hasVibratePermission= */ true);
    }

    @Test
    public void testNoOpWhenMissingVibratePermission() {
        HapticsManagerAndroid managerWithoutPermission =
                new HapticsManagerAndroid(mSystemVibrator, /* hasVibratePermission= */ false);

        managerWithoutPermission.playHaptics(HapticEffect.TICK, 0.8);

        verify(mSystemVibrator, never())
                .vibrate(any(VibrationEffect.class), any(VibrationAttributes.class));
    }

    @Test
    public void testNoOpWhenSystemHasNoVibrator() {
        when(mSystemVibrator.hasVibrator()).thenReturn(false);

        mHapticsManager.playHaptics(HapticEffect.TICK, 0.8);

        verify(mSystemVibrator, never())
                .vibrate(any(VibrationEffect.class), any(VibrationAttributes.class));
    }

    @Test
    public void testNoOpOnUnrecognizedEffect() {
        mHapticsManager.playHaptics(-1, 0.8);
        mHapticsManager.playHaptics(999, 0.8);

        verify(mSystemVibrator, never())
                .vibrate(any(VibrationEffect.class), any(VibrationAttributes.class));
    }

    @Test
    public void testPrimaryCompositionEffectsOnApi33() {
        float intensity = 0.75f;

        mHapticsManager.playHaptics(HapticEffect.HINT, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_TICK,
                                                0.4f * intensity)
                                        .compose()),
                        any(VibrationAttributes.class));

        mHapticsManager.playHaptics(HapticEffect.TICK, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_LOW_TICK,
                                                0.7f * intensity)
                                        .compose()),
                        any(VibrationAttributes.class));

        mHapticsManager.playHaptics(HapticEffect.ALIGN, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_TICK,
                                                intensity)
                                        .compose()),
                        any(VibrationAttributes.class));

        mHapticsManager.playHaptics(HapticEffect.EDGE, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_THUD,
                                                0.7f * intensity)
                                        .compose()),
                        any(VibrationAttributes.class));

        mHapticsManager.playHaptics(HapticEffect.SUCCESS, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.7f * intensity,
                                                0)
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.7f * intensity,
                                                52)
                                        .compose()),
                        any(VibrationAttributes.class));

        mHapticsManager.playHaptics(HapticEffect.ERROR, intensity);
        ArgumentCaptor<VibrationAttributes> attrsCaptor =
                ArgumentCaptor.forClass(VibrationAttributes.class);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.40f * intensity,
                                                0)
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.39f * intensity,
                                                100)
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.31f * intensity,
                                                100)
                                        .compose()),
                        attrsCaptor.capture());
        assertEquals(VibrationAttributes.USAGE_TOUCH, attrsCaptor.getValue().getUsage());
    }

    @Test
    public void testFallbackCompositionPrimitivesWhenPrimaryUnsupported() {
        float intensity = 0.8f;

        // TICK falls back to PRIMITIVE_TICK scaled by 0.6f when LOW_TICK is unsupported.
        when(mSystemVibrator.areAllPrimitivesSupported(
                        VibrationEffect.Composition.PRIMITIVE_LOW_TICK))
                .thenReturn(false);
        when(mSystemVibrator.areAllPrimitivesSupported(VibrationEffect.Composition.PRIMITIVE_TICK))
                .thenReturn(true);
        mHapticsManager.playHaptics(HapticEffect.TICK, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_TICK,
                                                0.6f * intensity)
                                        .compose()),
                        any(VibrationAttributes.class));

        // EDGE falls back to PRIMITIVE_CLICK scaled by 0.7f when PRIMITIVE_THUD is unsupported.
        when(mSystemVibrator.areAllPrimitivesSupported(VibrationEffect.Composition.PRIMITIVE_THUD))
                .thenReturn(false);
        when(mSystemVibrator.areAllPrimitivesSupported(VibrationEffect.Composition.PRIMITIVE_CLICK))
                .thenReturn(true);
        mHapticsManager.playHaptics(HapticEffect.EDGE, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.7f * intensity)
                                        .compose()),
                        any(VibrationAttributes.class));

        // When primitives are unsupported, all effects silently no-op.
        when(mSystemVibrator.areAllPrimitivesSupported(VibrationEffect.Composition.PRIMITIVE_CLICK))
                .thenReturn(false);
        when(mSystemVibrator.areAllPrimitivesSupported(VibrationEffect.Composition.PRIMITIVE_TICK))
                .thenReturn(false);
        clearInvocations(mSystemVibrator);
        mHapticsManager.playHaptics(HapticEffect.HINT, intensity);
        mHapticsManager.playHaptics(HapticEffect.TICK, intensity);
        mHapticsManager.playHaptics(HapticEffect.ALIGN, intensity);
        mHapticsManager.playHaptics(HapticEffect.EDGE, intensity);
        mHapticsManager.playHaptics(HapticEffect.SUCCESS, intensity);
        mHapticsManager.playHaptics(HapticEffect.ERROR, intensity);
        verify(mSystemVibrator, never())
                .vibrate(any(VibrationEffect.class), any(VibrationAttributes.class));
        verify(mSystemVibrator, never())
                .vibrate(any(VibrationEffect.class), any(AudioAttributes.class));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testNoOpOnApi29() {
        mHapticsManager.playHaptics(HapticEffect.HINT, 0.8);
        mHapticsManager.playHaptics(HapticEffect.TICK, 0.8);
        mHapticsManager.playHaptics(HapticEffect.ALIGN, 0.8);
        mHapticsManager.playHaptics(HapticEffect.EDGE, 0.8);
        mHapticsManager.playHaptics(HapticEffect.SUCCESS, 0.8);
        mHapticsManager.playHaptics(HapticEffect.ERROR, 0.8);

        verify(mSystemVibrator, never())
                .vibrate(any(VibrationEffect.class), any(AudioAttributes.class));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testCompositionEffectsOnApi30() {
        float intensity = 0.8f;

        mHapticsManager.playHaptics(HapticEffect.HINT, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_TICK,
                                                0.4f * intensity)
                                        .compose()),
                        any(AudioAttributes.class));

        mHapticsManager.playHaptics(HapticEffect.TICK, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_TICK,
                                                0.6f * intensity)
                                        .compose()),
                        any(AudioAttributes.class));

        mHapticsManager.playHaptics(HapticEffect.ALIGN, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_TICK,
                                                intensity)
                                        .compose()),
                        any(AudioAttributes.class));

        ArgumentCaptor<AudioAttributes> audioAttrsCaptor =
                ArgumentCaptor.forClass(AudioAttributes.class);
        mHapticsManager.playHaptics(HapticEffect.EDGE, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.7f * intensity)
                                        .compose()),
                        audioAttrsCaptor.capture());
        assertEquals(
                AudioAttributes.USAGE_ASSISTANCE_SONIFICATION,
                audioAttrsCaptor.getValue().getUsage());

        mHapticsManager.playHaptics(HapticEffect.SUCCESS, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.7f * intensity,
                                                0)
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.7f * intensity,
                                                52)
                                        .compose()),
                        any(AudioAttributes.class));

        mHapticsManager.playHaptics(HapticEffect.ERROR, intensity);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.40f * intensity,
                                                0)
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.39f * intensity,
                                                100)
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_CLICK,
                                                0.31f * intensity,
                                                100)
                                        .compose()),
                        any(AudioAttributes.class));
    }

    @Test
    public void testIntensityClamping() {
        mHapticsManager.playHaptics(HapticEffect.ALIGN, 1.5);
        verify(mSystemVibrator)
                .vibrate(
                        eq(
                                VibrationEffect.startComposition()
                                        .addPrimitive(
                                                VibrationEffect.Composition.PRIMITIVE_TICK, 1.0f)
                                        .compose()),
                        any(VibrationAttributes.class));
        clearInvocations(mSystemVibrator);

        // Zero, negative, and non-finite intensities are ignored.
        mHapticsManager.playHaptics(HapticEffect.SUCCESS, 0.0);
        mHapticsManager.playHaptics(HapticEffect.SUCCESS, -0.5);
        mHapticsManager.playHaptics(HapticEffect.SUCCESS, Double.NaN);
        mHapticsManager.playHaptics(HapticEffect.SUCCESS, Double.POSITIVE_INFINITY);
        mHapticsManager.playHaptics(HapticEffect.SUCCESS, Double.NEGATIVE_INFINITY);
        verify(mSystemVibrator, never())
                .vibrate(any(VibrationEffect.class), any(VibrationAttributes.class));
    }
}
