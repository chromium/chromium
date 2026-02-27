// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.cached_flags.BooleanCachedFeatureParam;
import org.chromium.components.cached_flags.CachedFeatureParam;
import org.chromium.components.cached_flags.CachedFlag;
import org.chromium.content_public.common.ContentSwitches;

import java.util.List;
import java.util.Locale;
import java.util.function.BiConsumer;

/** Data class for the JavalessRenderers feature. */
@NullMarked
public class JavalessRenderersFeatureList {
    public static final String JAVALESS_RENDERER_EXPERIMENT_ON = "JavalessRendererExperimentOn";

    public static final CachedFlag sJavalessRendererExperimentOn =
            new CachedFlag(
                    ContentFeatureMap.getInstance(),
                    JAVALESS_RENDERER_EXPERIMENT_ON,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);

    public static final BooleanCachedFeatureParam sJavalessRendererEnable =
            new BooleanCachedFeatureParam(
                    ContentFeatureMap.getInstance(),
                    JAVALESS_RENDERER_EXPERIMENT_ON,
                    "JavalessRendererEnable",
                    /* defaultValue= */ false);

    public static final List<CachedFlag> sCachedFlags = List.of(sJavalessRendererExperimentOn);

    public static final List<CachedFeatureParam<?>> sParamsCached =
            List.of(sJavalessRendererEnable);

    private static @Nullable Boolean sEnabled;
    private static @MonotonicNonNull String sGroup;
    private static @Nullable BiConsumer<String, String> sCallback;

    // Should be run only once, and will only run once both callback and group are set, then unsets
    // callback to ensure it never runs again.
    private static void maybeRunCallback() {
        if (sCallback != null && sGroup != null) {
            BiConsumer<String, String> callback = sCallback;
            sCallback = null;
            ThreadUtils.runOnUiThread(() -> callback.accept("JavalessRenderersSynthetic", sGroup));
        }
    }

    public static void setRegisterSyntheticFieldTrialCallback(BiConsumer<String, String> callback) {
        synchronized (JavalessRenderersFeatureList.class) {
            sCallback = callback;
            maybeRunCallback();
        }
    }

    public static boolean isEnabled() {
        synchronized (JavalessRenderersFeatureList.class) {
            if (sEnabled == null) {
                decideEnabledState();
                maybeRunCallback();
            }
            return sEnabled;
        }
    }

    @EnsuresNonNull("sEnabled")
    private static void decideEnabledState() {
        CommandLine commandLine = CommandLine.getInstance();
        if (commandLine.hasSwitch(ContentSwitches.JAVALESS_RENDERERS)) {
            String value = commandLine.getSwitchValue(ContentSwitches.JAVALESS_RENDERERS);
            if (!TextUtils.isEmpty(value)) {
                if (value.toLowerCase(Locale.ENGLISH).startsWith("enable")) {
                    sEnabled = true;
                    return;
                } else if (value.toLowerCase(Locale.ENGLISH).startsWith("disable")) {
                    sEnabled = false;
                    return;
                }
            }
            assert false : "--javaless-renderers switch requires value of 'enabled' or 'disabled'";
            sEnabled = false;
            return;
        }
        if (!sJavalessRendererExperimentOn.isEnabled()) {
            sEnabled = false;
        } else {
            // We only give a sGroup if we are legitimately controlled by the trial.
            boolean enabled = sJavalessRendererEnable.getValue();
            sGroup = enabled ? "Enabled" : "Disabled";
            sEnabled = enabled;
        }
    }
}
