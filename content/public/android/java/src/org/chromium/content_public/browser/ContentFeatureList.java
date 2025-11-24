// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.base.MutableBooleanParamWithSafeDefault;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.base.MutableIntParamWithSafeDefault;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.cached_flags.CachedFlag;
import org.chromium.content.common.ContentInternalFeatures;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.accessibility.AccessibilityFeatures;

import java.util.List;

/** Convenience static methods to access {@link ContentFeatureMap}. */
@NullMarked
public class ContentFeatureList {
    private ContentFeatureList() {}

    // TODO(crbug.com/40268884): Use generated constants in ContentFeatures and other generated
    // Features files, then remove the constants below.

    // Alphabetical:
    public static final String ACCESSIBILITY_DEPRECATE_TYPE_ANNOUNCE =
            "AccessibilityDeprecateTypeAnnounce";

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

    public static final String ANDROID_CAPTURE_KEY_EVENTS = "AndroidCaptureKeyEvents";
    public static final String ANDROID_CARET_BROWSING = "AndroidCaretBrowsing";

    public static final String ANDROID_DEV_TOOLS_FRONTEND = "AndroidDevToolsFrontend";

    public static final String ANDROID_MEDIA_INSERTION = "AndroidMediaInsertion";

    public static final String ANDROID_OPEN_PDF_INLINE = "AndroidOpenPdfInline";

    public static final String HIDE_PASTE_POPUP_ON_GSB = "HidePastePopupOnGSB";

    public static final String JAVALESS_RENDERERS = "JavalessRenderers";

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
                    false);

    public static final MutableFlagWithSafeDefault sAndroidCaretBrowsing =
            new MutableFlagWithSafeDefault(
                    ContentFeatureMap.getInstance(), ContentFeatures.ANDROID_CARET_BROWSING, false);

    public static final MutableFlagWithSafeDefault sStrictHighRankProcessLRU =
            new MutableFlagWithSafeDefault(
                    ContentFeatureMap.getInstance(),
                    ContentInternalFeatures.STRICT_HIGH_RANK_PROCESS_LRU,
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

    // Use a CachedFlag as this is often checked before native is loaded, and must stay consistent
    // once decided upon.
    public static final CachedFlag sJavalessRenderers =
            new CachedFlag(ContentFeatureMap.getInstance(), JAVALESS_RENDERERS, false, false);

    public static final MutableFlagWithSafeDefault sAndroidDesktopZoomScaling =
            new MutableFlagWithSafeDefault(
                    ContentFeatureMap.getInstance(),
                    ContentFeatures.ANDROID_DESKTOP_ZOOM_SCALING,
                    false);

    public static final MutableIntParamWithSafeDefault sAndroidDesktopZoomScalingFactor =
            sAndroidDesktopZoomScaling.newIntParam("desktop-zoom-scaling-factor", 100);

    public static final MutableIntParamWithSafeDefault sAndroidMonitorZoomScalingFactor =
            sAndroidDesktopZoomScaling.newIntParam("monitor-zoom-scaling-factor", 100);

    public static final List<CachedFlag> sCachedFlags = List.of(sJavalessRenderers);
}
