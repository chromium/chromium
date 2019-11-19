// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

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
        return ContentFeatureListImpl.isEnabled(featureName);
    }

    // Alphabetical:
    public static final String BACKGROUND_MEDIA_RENDERER_HAS_MODERATE_BINDING =
            "BackgroundMediaRendererHasModerateBinding";

    public static final String SERVICE_GROUP_IMPORTANCE = "ServiceGroupImportance";

    public static final String WEB_NFC = "WebNFC";
}
