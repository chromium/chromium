// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Class to access HttpUtil library from Java.
 *
 * <p>The corresponding native code is in net/android/android_http_util.cc.
 */
@JNINamespace("net")
@NullMarked
public final class HttpUtil {
    /**
     * Returns true iff: - |headerName| is validly formed (see HttpUtil::IsValidHeaderName). -
     * |headerName| is not a forbidden header (see HttpUtil::IsSafeHeader). - |headerValue| is
     * validly formed (see HttpUtil::IsValidHeaderValue).
     */
    public static boolean isAllowedHeader(String headerName, @Nullable String headerValue) {
        return HttpUtilJni.get().isAllowedHeader(headerName, headerValue);
    }

    @NativeMethods
    interface Natives {
        boolean isAllowedHeader(String headerName, @Nullable String headerValue);
    }
}
