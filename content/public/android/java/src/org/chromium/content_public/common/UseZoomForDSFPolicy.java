// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.common;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * This is a utility class to wrap use_zoom_for_dsf_policy.cc.
 */
@JNINamespace("content")
public final class UseZoomForDSFPolicy {
    public static boolean isUseZoomForDSFEnabled() {
        return UseZoomForDSFPolicyJni.get().isUseZoomForDSFEnabled();
    }

    // Do not instantiate this class.
    private UseZoomForDSFPolicy() {}

    @NativeMethods
    interface Natives {
        boolean isUseZoomForDSFEnabled();
    }
}
