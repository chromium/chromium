// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.UnguessableToken;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.AdditionalNavigationParams;

/** Interface which provides native access to an AdditionalNavigationParams instance. */
@JNINamespace("content")
@NullMarked
public class AdditionalNavigationParamsUtils {
    private AdditionalNavigationParamsUtils() {}

    @CalledByNative
    private static AdditionalNavigationParams create(
            @JniType("base::UnguessableToken") UnguessableToken initiatorFrameToken,
            int initiatorProcessId,
            @JniType("std::optional<base::UnguessableToken>") @Nullable
                    UnguessableToken attributionSrcToken) {
        return new AdditionalNavigationParams(
                initiatorFrameToken, initiatorProcessId, attributionSrcToken);
    }

    @CalledByNative
    private static @JniType("std::optional<base::UnguessableToken>") UnguessableToken
            getInitiatorFrameToken(AdditionalNavigationParams params) {
        return params.getInitiatorFrameToken();
    }

    @CalledByNative
    private static int getInitiatorProcessId(AdditionalNavigationParams params) {
        return params.getInitiatorProcessId();
    }

    @CalledByNative
    private static @JniType("std::optional<base::UnguessableToken>") @Nullable UnguessableToken
            getAttributionSrcToken(AdditionalNavigationParams params) {
        return params.getAttributionSrcToken();
    }
}
