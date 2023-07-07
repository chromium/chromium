// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.annotation.Nullable;

import org.chromium.base.UnguessableToken;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.AdditionalNavigationParams;

/**
 * Interface which provides native access to an AdditionalNavigationParams instance.
 */
@JNINamespace("content")
public class AdditionalNavigationParamsUtils {
    private AdditionalNavigationParamsUtils() {}

    @CalledByNative
    private static AdditionalNavigationParams create(UnguessableToken initiatorFrameToken,
            int initiatorProcessId, @Nullable UnguessableToken attributionSrcToken,
            long attributionRuntimeFeatures) {
        return new AdditionalNavigationParams(initiatorFrameToken, initiatorProcessId,
                attributionSrcToken, attributionRuntimeFeatures);
    }

    @CalledByNative
    private static UnguessableToken getInitiatorFrameToken(AdditionalNavigationParams params) {
        return params.getInitiatorFrameToken();
    }

    @CalledByNative
    private static int getInitiatorProcessId(AdditionalNavigationParams params) {
        return params.getInitiatorProcessId();
    }

    @CalledByNative
    private static UnguessableToken getAttributionSrcToken(AdditionalNavigationParams params) {
        return params.getAttributionSrcToken();
    }

    @CalledByNative
    private static long getAttributionRuntimeFeatures(AdditionalNavigationParams params) {
        return params.getAttributionRuntimeFeatures();
    }
}
