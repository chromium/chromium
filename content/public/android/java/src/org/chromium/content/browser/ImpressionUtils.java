// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.UnguessableToken;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.Impression;

/**
 * Interface which provides native access to an Impression.
 */
@JNINamespace("content")
public class ImpressionUtils {
    private ImpressionUtils() {}

    @CalledByNative
    private static Impression create(UnguessableToken attributionSrcToken,
            UnguessableToken initiatorFrameToken, int initiatorProcessID,
            long attributionRuntimeFeatures) {
        return new Impression(attributionSrcToken, initiatorFrameToken, initiatorProcessID,
                attributionRuntimeFeatures);
    }

    @CalledByNative
    private static UnguessableToken getAttributionSrcToken(Impression impression) {
        return impression.getAttributionSrcToken();
    }

    @CalledByNative
    private static UnguessableToken getInitiatorFrameToken(Impression impression) {
        return impression.getInitiatorFrameToken();
    }

    @CalledByNative
    private static int getInitiatorProcessID(Impression impression) {
        return impression.getInitiatorProcessID();
    }

    @CalledByNative
    private static long getAttributionRuntimeFeatures(Impression impression) {
        return impression.getAttributionRuntimeFeatures();
    }
}
