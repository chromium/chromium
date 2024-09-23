// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import androidx.annotation.VisibleForTesting;

import java.nio.charset.StandardCharsets;

/**
 * Network Traffic Annotations document the purpose of a particular network request, and its impact
 * on privacy.
 *
 * This documentation is typically meant for system administrators in an enterprise setting. It
 * should be easy for them to read and understand, and answer the following questions:
 *
 *   1. When and why does Chrome make this network request?
 *   2. Does this network request send any sensitive data?
 *   3. Where does the request go? (e.g. a Google server, a website the user is viewing...)
 *   4. How can I disable it if I don't like it?
 */
public class NetworkTrafficAnnotationTag {
    /**
     * For network requests that aren't documented yet. These should be
     * accompanied with a TODO with a bug/owner to write their documentation.
     */
    public static final NetworkTrafficAnnotationTag NO_TRAFFIC_ANNOTATION_YET =
            createComplete("undefined", "Nothing here yet.");

    /**
     * For network requests that don't need an annotation, because they're in an
     * allowlisted file (see tools/traffic_annotation/safe_list.txt).
     */
    public static final NetworkTrafficAnnotationTag MISSING_TRAFFIC_ANNOTATION =
            createComplete("undefined", "Function called without traffic annotation.");

    /** For network requests made in tests, don't bother writing documentation. */
    public static final NetworkTrafficAnnotationTag TRAFFIC_ANNOTATION_FOR_TESTS =
            createComplete("test", "Traffic annotation for unit, browser and other tests");

    /**
     * Create a self-contained tag describing a network request made by Chromium. This is the most
     * common factory method.
     *
     * The C++ equivalent is DefineNetworkTrafficAnnotation().
     *
     * @param uniqueId a String that uniquely identifies this annotations across all of Chromium
     *         source code.
     * @param proto a text-encoded NetworkTrafficAnnotation protobuf (see
     *         chrome/browser/privacy/traffic_annotation.proto).
     */
    public static NetworkTrafficAnnotationTag createComplete(String uniqueId, String proto) {
        return new NetworkTrafficAnnotationTag(uniqueId);
    }

    // TODO(crbug.com/40190832): Add Partial, Completing, Branched-Completing, and
    // Mutable(?) factory methods.

    /**
     * At runtime, an annotation tag is just a hashCode. Most of the validation is done on CQ, so
     * there's no point keeping track of everything at runtime.
     *
     * <p>This field is referenced from C++, so don't change it without updating
     * net/traffic_annotation/network_traffic_annotation.h.
     */
    // TODO(crbug.com/40190832): Unlike the C++ version though, the string will still get compiled
    // into the APK, and get loaded into memory when the constructor is called... Is there a way to
    // tell Java, "No, I don't actually need this string at runtime"? We should investigate.
    private final int mHashCode;

    /**
     * @return the hash code of uniqueId, which uniquely identifies this annotation.
     */
    public int getHashCode() {
        return mHashCode;
    }

    /**
     * Constructor for NetworkTrafficAnnotationTag. Consumers of this API should use
     * CreateComplete() instead.
     *
     * @param uniqueId a String that uniquely identifies this annotation across all of Chromium
     *         source code.
     */
    private NetworkTrafficAnnotationTag(String uniqueId) {
        mHashCode = iterativeHash(uniqueId);
    }

    /**
     * Returns the hashcode of a string, as per the recursive_hash() function used in C++ code.
     *
     * This is NOT the same as Java's built-in hashCode() function, because we really want to
     * produce the same hashcode that auditor.py produces.
     *
     * @param s the String to calculate the hash on.
     */
    @VisibleForTesting
    static int iterativeHash(String s) {
        // Multiplying by 31 would cause an overflow if using `int', so use `long' instead.
        long acc = 0;
        // Encode the string as UTF-8.
        byte[] bytes = s.getBytes(StandardCharsets.UTF_8);
        for (byte b : bytes) {
            acc = (acc * 31 + b) % 138003713;
        }
        // The final result always fits in an `int' thanks to the modulo.
        return (int) acc;
    }
}
