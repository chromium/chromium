// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/** Convenience static methods to access {@link ContentFeatureMap}. */
public class ContentFeatureList {
    private ContentFeatureList() {}

    // TODO(crbug.com/40268884): Use generated constants in ContentFeatures and other generated
    // Features files, then remove the constants below.

    // Alphabetical:
    public static final String ACCESSIBILITY_INCLUDE_LONG_CLICK_ACTION =
            "AccessibilityIncludeLongClickAction";

    public static final String ACCESSIBILITY_PAGE_ZOOM = "AccessibilityPageZoom";
    // Field trial param associated with the Page Zoom feature.
    public static final String ACCESSIBILITY_PAGE_ZOOM_PARAM = "AdjustForOSLevel";

    public static final String ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS =
            "AccessibilityPageZoomEnhancements";

    public static final String ACCESSIBILITY_PAGE_ZOOM_V2 = "AccessibilityPageZoomV2";

    public static final String ACCESSIBILITY_UNIFIED_SNAPSHOTS = "AccessibilityUnifiedSnapshots";
    public static final String ACCESSIBILITY_MANAGE_BROADCAST_RECEIVER_ON_BACKGROUND =
            "AccessibilityManageBroadcastReceiverOnBackground";

    public static final String ANDROID_OPEN_PDF_INLINE = "AndroidOpenPdfInline";

    public static final String HIDE_PASTE_POPUP_ON_GSB = "HidePastePopupOnGSB";

    public static final String MOUSE_AND_TRACKPAD_DROPDOWN_MENU = "MouseAndTrackpadDropdownMenu";

    public static final String INPUT_ON_VIZ = "InputOnViz";

    public static final String OPTIMIZE_IMM_HIDE_CALLS = "OptimizeImmHideCalls";

    public static final String ONE_TIME_PERMISSION = "OneTimePermission";

    public static final String PROCESS_SHARING_WITH_STRICT_SITE_INSTANCES =
            "ProcessSharingWithStrictSiteInstances";

    public static final String CONTINUE_GESTURE_ON_LOSING_FOCUS = "ContinueGestureOnLosingFocus";

    public static final String SMART_ZOOM = "SmartZoom";

    public static final String WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND =
            "WebBluetoothNewPermissionsBackend";

    public static final String WEB_NFC = "WebNFC";

    public static final String WEB_IDENTITY_DIGITAL_CREDENTIALS = "WebIdentityDigitalCredentials";
}
