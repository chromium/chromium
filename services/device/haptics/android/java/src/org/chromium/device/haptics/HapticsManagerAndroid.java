// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.haptics;

import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.os.Build;
import android.os.VibrationAttributes;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.device.mojom.HapticEffect;

/**
 * Android implementation details for device::HapticsManagerAndroid. Connects the Web Haptics API to
 * Android's Vibrator / VibrationEffect framework.
 */
@JNINamespace("device")
@NullMarked
public class HapticsManagerAndroid {
    private static final String TAG = "HapticsManager";

    // Primitive scales and inter-beat delays sourced from AOSP's HapticFeedbackVibrationProvider
    // (GESTURE_THRESHOLD_DEACTIVATE) and the Android Google App (AGSA) SearchAppHaptics
    // implementation for cross-platform tactile parity:
    // - FULL_STRENGTH_SCALE: 0.7f scale for TICK, EDGE, and SUCCESS compositions (ALIGN uses
    //   unscaled PRIMITIVE_TICK to match AGSA and preserve contrast against HINT).
    // - HINT_TICK_SCALE: 0.4f scale on PRIMITIVE_TICK for a subtle light tap.
    // - TICK_FALLBACK_SCALE: 0.6f scale when falling back from PRIMITIVE_LOW_TICK to
    //   PRIMITIVE_TICK to prevent tactile fatigue during rapid stepping.
    // - SUCCESS_INTER_BEAT_DELAY_MS: 52ms gap between two PRIMITIVE_CLICK beats.
    // - ERROR_BEAT_*_SCALE / ERROR_INTER_BEAT_DELAY_MS: 3-beat decaying PRIMITIVE_CLICK pattern
    //   (0.40f, 0.39f, 0.31f separated by 100ms).
    private static final float FULL_STRENGTH_SCALE = 0.7f;
    private static final float HINT_TICK_SCALE = 0.4f;
    private static final float TICK_FALLBACK_SCALE = 0.6f;
    private static final int SUCCESS_INTER_BEAT_DELAY_MS = 52;
    private static final float ERROR_BEAT_1_SCALE = 0.40f;
    private static final float ERROR_BEAT_2_SCALE = 0.39f;
    private static final float ERROR_BEAT_3_SCALE = 0.31f;
    private static final int ERROR_INTER_BEAT_DELAY_MS = 100;

    private final @Nullable Vibrator mSystemVibrator;
    private final boolean mHasVibratePermission;

    public HapticsManagerAndroid() {
        this(
                getSystemVibrator(ContextUtils.getApplicationContext()),
                checkVibratePermission(ContextUtils.getApplicationContext()));
    }

    @VisibleForTesting
    HapticsManagerAndroid(@Nullable Vibrator systemVibrator, boolean hasVibratePermission) {
        mSystemVibrator = systemVibrator;
        mHasVibratePermission = hasVibratePermission;
    }

    private static @Nullable Vibrator getSystemVibrator(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            VibratorManager vibratorManager = context.getSystemService(VibratorManager.class);
            return vibratorManager != null ? vibratorManager.getDefaultVibrator() : null;
        }
        return context.getSystemService(Vibrator.class);
    }

    private static boolean checkVibratePermission(Context context) {
        boolean granted =
                context.checkSelfPermission(android.Manifest.permission.VIBRATE)
                        == PackageManager.PERMISSION_GRANTED;
        if (!granted) {
            Log.w(TAG, "Missing android.permission.VIBRATE.");
        }
        return granted;
    }

    @CalledByNative
    static HapticsManagerAndroid create() {
        return new HapticsManagerAndroid();
    }

    @CalledByNative
    public void playHaptics(int effect, double intensity) {
        ThreadUtils.assertOnUiThread();
        // Below Android R (API level 30), the SDK lacks VibrationEffect.Composition and does not
        // support intensity values.
        // TODO(crbug.com/584500233): Evaluate whether Android Q (API level 29) should provide a
        // fallback that does not support intensity scaling.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R
                || !mHasVibratePermission
                || Double.isNaN(intensity)
                || Double.isInfinite(intensity)
                || intensity <= 0.0) {
            return;
        }

        if (mSystemVibrator == null || !mSystemVibrator.hasVibrator()) {
            return;
        }

        float clampedIntensity = (float) Math.min(intensity, 1.0);
        VibrationEffect vibrationEffect =
                createVibrationEffect(mSystemVibrator, effect, clampedIntensity);
        if (vibrationEffect == null) {
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            VibrationAttributes attrs =
                    new VibrationAttributes.Builder()
                            .setUsage(VibrationAttributes.USAGE_TOUCH)
                            .build();
            mSystemVibrator.vibrate(vibrationEffect, attrs);
        } else {
            AudioAttributes audioAttrs =
                    new AudioAttributes.Builder()
                            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                            .setUsage(AudioAttributes.USAGE_ASSISTANCE_SONIFICATION)
                            .build();
            mSystemVibrator.vibrate(vibrationEffect, audioAttrs);
        }
    }

    private @Nullable VibrationEffect createVibrationEffect(
            Vibrator vibrator, int effect, float intensity) {
        // Return early below Android R (API level 30) because the SDK does not accept an intensity
        // parameter.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return null;
        }

        switch (effect) {
            case HapticEffect.HINT:
                // Light tap / subtle affordance (GESTURE_THRESHOLD_DEACTIVATE:
                // PRIMITIVE_TICK @ 0.4f).
                if (vibrator.areAllPrimitivesSupported(
                        VibrationEffect.Composition.PRIMITIVE_TICK)) {
                    return VibrationEffect.startComposition()
                            .addPrimitive(
                                    VibrationEffect.Composition.PRIMITIVE_TICK,
                                    HINT_TICK_SCALE * intensity)
                            .compose();
                }
                return null;

            case HapticEffect.TICK:
                // Primary: Frequent micro-tick (SEGMENT_FREQUENT_TICK)
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                        && vibrator.areAllPrimitivesSupported(
                                VibrationEffect.Composition.PRIMITIVE_LOW_TICK)) {
                    return VibrationEffect.startComposition()
                            .addPrimitive(
                                    VibrationEffect.Composition.PRIMITIVE_LOW_TICK,
                                    FULL_STRENGTH_SCALE * intensity)
                            .compose();
                }
                // Fallback: Softened standard tick to prevent tactile fatigue
                if (vibrator.areAllPrimitivesSupported(
                        VibrationEffect.Composition.PRIMITIVE_TICK)) {
                    return VibrationEffect.startComposition()
                            .addPrimitive(
                                    VibrationEffect.Composition.PRIMITIVE_TICK,
                                    TICK_FALLBACK_SCALE * intensity)
                            .compose();
                }
                return null;

            case HapticEffect.ALIGN:
                // Primary: Crisp detent / snap confirmation (SEGMENT_TICK)
                if (vibrator.areAllPrimitivesSupported(
                        VibrationEffect.Composition.PRIMITIVE_TICK)) {
                    return VibrationEffect.startComposition()
                            .addPrimitive(VibrationEffect.Composition.PRIMITIVE_TICK, intensity)
                            .compose();
                }
                return null;

            case HapticEffect.EDGE:
                // Primary: Heavy boundary impact / collision (LONG_PRESS)
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                        && vibrator.areAllPrimitivesSupported(
                                VibrationEffect.Composition.PRIMITIVE_THUD)) {
                    return VibrationEffect.startComposition()
                            .addPrimitive(
                                    VibrationEffect.Composition.PRIMITIVE_THUD,
                                    FULL_STRENGTH_SCALE * intensity)
                            .compose();
                }
                // Fallback: Crisp click impact on API 30 or devices without PRIMITIVE_THUD
                if (vibrator.areAllPrimitivesSupported(
                        VibrationEffect.Composition.PRIMITIVE_CLICK)) {
                    return VibrationEffect.startComposition()
                            .addPrimitive(
                                    VibrationEffect.Composition.PRIMITIVE_CLICK,
                                    FULL_STRENGTH_SCALE * intensity)
                            .compose();
                }
                return null;

            case HapticEffect.SUCCESS:
                // Primary: Two crisp click beats confirmation (AGSA SearchAppHaptics)
                if (vibrator.areAllPrimitivesSupported(
                        VibrationEffect.Composition.PRIMITIVE_CLICK)) {
                    return VibrationEffect.startComposition()
                            .addPrimitive(
                                    VibrationEffect.Composition.PRIMITIVE_CLICK,
                                    FULL_STRENGTH_SCALE * intensity,
                                    0)
                            .addPrimitive(
                                    VibrationEffect.Composition.PRIMITIVE_CLICK,
                                    FULL_STRENGTH_SCALE * intensity,
                                    SUCCESS_INTER_BEAT_DELAY_MS)
                            .compose();
                }
                return null;

            case HapticEffect.ERROR:
                // Primary: Three-beat decaying click rejection (AGSA SearchAppHaptics)
                if (vibrator.areAllPrimitivesSupported(
                        VibrationEffect.Composition.PRIMITIVE_CLICK)) {
                    return VibrationEffect.startComposition()
                            .addPrimitive(
                                    VibrationEffect.Composition.PRIMITIVE_CLICK,
                                    ERROR_BEAT_1_SCALE * intensity,
                                    0)
                            .addPrimitive(
                                    VibrationEffect.Composition.PRIMITIVE_CLICK,
                                    ERROR_BEAT_2_SCALE * intensity,
                                    ERROR_INTER_BEAT_DELAY_MS)
                            .addPrimitive(
                                    VibrationEffect.Composition.PRIMITIVE_CLICK,
                                    ERROR_BEAT_3_SCALE * intensity,
                                    ERROR_INTER_BEAT_DELAY_MS)
                            .compose();
                }
                return null;

            default:
                return null;
        }
    }
}
