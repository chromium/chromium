// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/**
 * Convenience static methods to access {@link ContentFeatureMap}.
 */
public class ContentFeatureList {
    private ContentFeatureList() {}

    // TODO(crbug.com/1447098): Use generated constants in ContentFeatures and other generated
    // Features files, then remove the constants below.

    // Alphabetical:
    public static final String ACCESSIBILITY_PAGE_ZOOM = "AccessibilityPageZoom";
    // Field trial param associated with the Page Zoom feature.
    public static final String ACCESSIBILITY_PAGE_ZOOM_PARAM = "AdjustForOSLevel";

    public static final String ACCESSIBILITY_PERFORMANCE_FILTERING =
            "AccessibilityPerformanceFiltering";

    public static final String AUTO_DISABLE_ACCESSIBILITY_V2 = "AutoDisableAccessibilityV2";

    public static final String BACKGROUND_MEDIA_RENDERER_HAS_MODERATE_BINDING =
            "BackgroundMediaRendererHasModerateBinding";

    public static final String MOUSE_AND_TRACKPAD_DROPDOWN_MENU = "MouseAndTrackpadDropdownMenu";

    public static final String ON_DEMAND_ACCESSIBILITY_EVENTS = "OnDemandAccessibilityEvents";

    public static final String OPTIMIZE_IMM_HIDE_CALLS = "OptimizeImmHideCalls";

    public static final String PROCESS_SHARING_WITH_STRICT_SITE_INSTANCES =
            "ProcessSharingWithStrictSiteInstances";

    public static final String REQUEST_DESKTOP_SITE_ADDITIONS = "RequestDesktopSiteAdditions";

    public static final String REQUEST_DESKTOP_SITE_WINDOW_SETTING =
            "RequestDesktopSiteWindowSetting";

    public static final String SMART_ZOOM = "SmartZoom";

    public static final String WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND =
            "WebBluetoothNewPermissionsBackend";

    public static final String WEB_NFC = "WebNFC";

    public static final String WEB_IDENTITY_MDOCS = "WebIdentityMDocs";
}
