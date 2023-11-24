// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Class to access the GURL library from java. */
@JNINamespace("net")
public final class GURLUtils {

    /**
     * Get the origin of an url: Ex getOrigin("http://www.example.com:8080/index.html?bar=foo")
     * would return "http://www.example.com:8080/". It will return an empty string for an
     * invalid url.
     *
     * @return The origin of the url
     */
    public static String getOrigin(String url) {
        return GURLUtilsJni.get().getOrigin(url);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        String getOrigin(String url);
    }
}
