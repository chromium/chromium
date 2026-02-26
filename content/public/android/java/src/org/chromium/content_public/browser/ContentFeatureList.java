// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.os.Build;

import org.chromium.base.MutableBooleanParamWithSafeDefault;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.base.MutableIntParamWithSafeDefault;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content.common.ContentInternalFeatures;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.accessibility.AccessibilityFeatures;

/** Convenience static methods to access {@link ContentFeatureMap}. */
@NullMarked
public class ContentFeatureList {
    private ContentFeatureList() {}

    // TODO(crbug.com/40268884): Use generated constants in ContentFeatures and other generated
    // Features files, then remove the constants below.

    // Alphabetical:
    public static final String ACCESSIBILITY_ATOMIC_LIVE_REGIONS = "AccessibilityAtomicLiveRegions";

    public static final String ACCESSIBILITY_DEPRECATE_TYPE_ANNOUNCE =
            "AccessibilityDeprecateTypeAnnounce";

    public static final String ACCESSIBILITY_EXTENDED_SELECTION = "AccessibilityExtendedSelection";

    public static final String ACCESSIBILITY_IME_GET_FORMATTED_TEXT =
            "AccessibilityImeGetFormattedText";

    public static final String ACCESSIBILITY_IMPROVE_LIVE_REGION_ANNOUNCE =
            "AccessibilityImproveLiveRegionAnnounce";

    public static final String ACCESSIBILITY_PAGE_ZOOM_V2 = "AccessibilityPageZoomV2";

    public static final String ACCESSIBILITY_POPULATE_SUPPLEMENTAL_DESCRIPTION_API =
            "AccessibilityPopulateSupplementalDescriptionApi";

    public static final String ACCESSIBILITY_SEQUENTIAL_FOCUS = "AccessibilitySequentialFocus";

    public static final String ACCESSIBILITY_SET_SELECTABLE_ON_ALL_NODES_WITH_TEXT =
            "AccessibilitySetSelectableOnAllNodesWithText";

    public static final String ACCESSIBILITY_UNIFIED_SNAPSHOTS = "AccessibilityUnifiedSnapshots";
    public static final String ACCESSIBILITY_MANAGE_BROADCAST_RECEIVER_ON_BACKGROUND =
            "AccessibilityManageBroadcastReceiverOnBackground";

    public static final String ALLOW_DELAYED_AUDIO_FOCUS_GAIN_ANDROID =
            "AllowDelayedAudioFocusGainAndroid";

    public static final String ANDROID_ENABLE_BACKGROUND_MEDIA_CAPTURING =
            "AndroidEnableBackgroundMediaCapturing";

    public static final String ANDROID_CAPTURE_KEY_EVENTS = "AndroidCaptureKeyEvents";
    public static final String ANDROID_CARET_BROWSING = "AndroidCaretBrowsing";

    public static final String ANDROID_DEV_TOOLS_FRONTEND = "AndroidDevToolsFrontend";

    public static final String ANDROID_MEDIA_INSERTION = "AndroidMediaInsertion";

    public static final String ANDROID_PK_AUTOCORRECT_UNDERLINE = "AndroidPkAutocorrectUnderline";

    public static final String ANDROID_SPELLCHECK_FULL_API_BLINK = "AndroidSpellcheckFullApiBlink";

    public static final String ANDROID_SPELLING_UNDERLINE_IN_COMPOSITION_MODE =
            "AndroidSpellingUnderlineInCompositionMode";

    public static final String HIDE_PASTE_POPUP_ON_GSB = "HidePastePopupOnGSB";

    public static final String INPUT_ON_VIZ = "InputOnViz";

    public static final String ONE_TIME_PERMISSION = "OneTimePermission";

    public static final String CONTINUE_GESTURE_ON_LOSING_FOCUS = "ContinueGestureOnLosingFocus";

    public static final String SMART_ZOOM = "SmartZoom";

    public static final String WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND =
            "WebBluetoothNewPermissionsBackend";

    public static final String WEB_IDENTITY_DIGITAL_CREDENTIALS = "WebIdentityDigitalCredentials";

    public static final String WEB_IDENTITY_DIGITAL_CREDENTIALS_CREATION =
            "WebIdentityDigitalCredentialsCreation";

    public static final String DIPS_TTL = "DIPSTtl";

    private static final MutableFlagWithSafeDefault sAccessibilityCheckJavaNodeCacheFreshness =
            new MutableFlagWithSafeDefault(
                    ContentFeatureMap.getInstance(),
                    ContentFeatures.ACCESSIBILITY_CHECK_JAVA_NODE_CACHE_FRESHNESS,
                    false);

    /**
     * Checks "AccessibilityCheckJavaNodeCacheFreshness" feature flag, including that current
     * environment is at least required Android SDK 33 (Tiramisu).
     */
    public static boolean enabledAccessibilityCheckJavaNodeCacheFreshness() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                && sAccessibilityCheckJavaNodeCacheFreshness.isEnabled();
    }

    public static final MutableFlagWithSafeDefault sAccessibilityDeprecateJavaNodeCache =
            new MutableFlagWithSafeDefault(
                    ContentFeatureMap.getInstance(),
                    ContentFeatures.ACCESSIBILITY_DEPRECATE_JAVA_NODE_CACHE,
                    false);

    public static final MutableBooleanParamWithSafeDefault
            sAccessibilityDeprecateJavaNodeCacheOptimizeScroll =
                    sAccessibilityDeprecateJavaNodeCache.newBooleanParam("optimize_scroll", false);

    public static final MutableBooleanParamWithSafeDefault
            sAccessibilityDeprecateJavaNodeCacheDisableCache =
                    sAccessibilityDeprecateJavaNodeCache.newBooleanParam("disable_cache", false);

    public static final MutableFlagWithSafeDefault sAccessibilityMagnificationFollowsFocus =
            new MutableFlagWithSafeDefault(
                    ContentFeatureMap.getInstance(),
                    AccessibilityFeatures.ACCESSIBILITY_MAGNIFICATION_FOLLOWS_FOCUS,
                    true);

    public static final MutableFlagWithSafeDefault sAndroidCaretBrowsing =
            new MutableFlagWithSafeDefault(
                    ContentFeatureMap.getInstance(), ContentFeatures.ANDROID_CARET_BROWSING, false);

    public static final MutableFlagWithSafeDefault sStrictHighRankProcessLRU =
            new MutableFlagWithSafeDefault(
                    ContentFeatureMap.getInstance(),
                    ContentInternalFeatures.STRICT_HIGH_RANK_PROCESS_LRU,
                    true);

    public static final MutableFlagWithSafeDefault sRemoveCachedProcessFromBindingManager =
            new MutableFlagWithSafeDefault(
                    ContentFeatureMap.getInstance(),
                    ContentInternalFeatures.REMOVE_CACHED_PROCESS_FROM_BINDING_MANAGER,
                    false);

    public static final MutableFlagWithSafeDefault sSpareRendererProcessPriority =
            new MutableFlagWithSafeDefault(
                    ContentFeatureMap.getInstance(),
                    ContentFeatures.SPARE_RENDERER_PROCESS_PRIORITY,
                    false);

    // Add a non-perceptible binding on Android to decrease the chance of the spare process getting
    // killed before it is taken.
    public static final MutableBooleanParamWithSafeDefault sSpareRendererAddNotPerceptibleBinding =
            sSpareRendererProcessPriority.newBooleanParam("not-perceptible-binding", false);

    // Make the spare renderer of the lowest priority so as not to kill other processes during OOM.
    public static final MutableBooleanParamWithSafeDefault sSpareRendererLowestRanking =
            sSpareRendererProcessPriority.newBooleanParam("lowest-ranking", false);

    // Skip the timeout when removing the VISIBLE and STRONG binding for the spare renderer.
    public static final MutableBooleanParamWithSafeDefault sSpareRendererRemoveBindingNoTimeout =
            sSpareRendererProcessPriority.newBooleanParam("remove-binding-no-timeout", false);

    public static final MutableFlagWithSafeDefault sAndroidDesktopZoomScaling =
            new MutableFlagWithSafeDefault(
                    ContentFeatureMap.getInstance(),
                    ContentFeatures.ANDROID_DESKTOP_ZOOM_SCALING,
                    false);

    public static final MutableIntParamWithSafeDefault sAndroidDesktopZoomScalingFactor =
            sAndroidDesktopZoomScaling.newIntParam("desktop-zoom-scaling-factor", 100);

    public static final MutableIntParamWithSafeDefault sAndroidMonitorZoomScalingFactor =
            sAndroidDesktopZoomScaling.newIntParam("monitor-zoom-scaling-factor", 100);
}
