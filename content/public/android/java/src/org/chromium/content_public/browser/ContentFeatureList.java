// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.base.FeatureList;
import org.chromium.content.browser.ContentFeatureListImpl;

/**
 * Static public methods for ContentFeatureList.
 */
public class ContentFeatureList {
    private ContentFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        Boolean testValue = FeatureList.getTestValueForFeature(featureName);
        if (testValue != null) return testValue;
        return ContentFeatureListImpl.isEnabled(featureName);
    }

    /**
     * Returns a field trial param as an int for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The integer value to use if the param is not available.
     * @return The parameter value as an int. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value does not represent an int.
     */
    public static int getFieldTrialParamByFeatureAsInt(
            String featureName, String paramName, int defaultValue) {
        String testValue = FeatureList.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Integer.valueOf(testValue);
        if (FeatureList.hasTestFeatures()) return defaultValue;
        assert FeatureList.isInitialized();
        return ContentFeatureListImpl.getFieldTrialParamByFeatureAsInt(
                featureName, paramName, defaultValue);
    }

    /**
     * Returns a field trial param as a boolean for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as a boolean.
     * @param defaultValue The boolean value to use if the param is not available.
     * @return The parameter value as a boolean. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value is neither "true" nor "false".
     */
    public static boolean getFieldTrialParamByFeatureAsBoolean(
            String featureName, String paramName, boolean defaultValue) {
        String testValue = FeatureList.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Boolean.valueOf(testValue);
        if (FeatureList.hasTestFeatures()) return defaultValue;
        assert FeatureList.isInitialized();
        return ContentFeatureListImpl.getFieldTrialParamByFeatureAsBoolean(
                featureName, paramName, defaultValue);
    }

    // Alphabetical:
    public static final String ACCESSIBILITY_PAGE_ZOOM = "AccessibilityPageZoom";

    public static final String ACCESSIBILITY_PERFORMANCE_FILTERING =
            "AccessibilityPerformanceFiltering";

    public static final String AUTO_DISABLE_ACCESSIBILITY_V2 = "AutoDisableAccessibilityV2";

    public static final String BACKGROUND_MEDIA_RENDERER_HAS_MODERATE_BINDING =
            "BackgroundMediaRendererHasModerateBinding";

    public static final String ON_DEMAND_ACCESSIBILITY_EVENTS = "OnDemandAccessibilityEvents";

    public static final String OPTIMIZE_IMM_HIDE_CALLS = "OptimizeImmHideCalls";

    public static final String PROCESS_SHARING_WITH_STRICT_SITE_INSTANCES =
            "ProcessSharingWithStrictSiteInstances";

    public static final String REQUEST_DESKTOP_SITE_ADDITIONS = "RequestDesktopSiteAdditions";

    public static final String REQUEST_DESKTOP_SITE_EXCEPTIONS = "RequestDesktopSiteExceptions";

    public static final String WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND =
            "WebBluetoothNewPermissionsBackend";

    public static final String WEB_NFC = "WebNFC";

    public static final String WEB_IDENTITY_MDOCS = "WebIdentityMDocs";
}
