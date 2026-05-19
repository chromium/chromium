// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import com.google.common.collect.ImmutableMap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.util.HashMap;
import java.util.Map;

/**
 * Simple version of NavigationEntry.java used for testing. Contains fields needed for testing from
 * NavigationEntry.java and header information from content/public/browser/navigation_entry.h.
 */
@JNINamespace("content")
public class NavigationEntrySimple {
    private final String mUrl;
    private final ImmutableMap<String, String> mExtraHeaders;

    @CalledByNative
    public NavigationEntrySimple(String url, String extraHeaders) {
        mUrl = url;
        mExtraHeaders = ImmutableMap.copyOf(parseExtraHeadersString(extraHeaders));
    }

    public String getUrl() {
        return mUrl;
    }

    public Map<String, String> getExtraHeaders() {
        return mExtraHeaders;
    }

    private static Map<String, String> parseExtraHeadersString(String headersString) {
        Map<String, String> headersMap = new HashMap<>();

        if (headersString == null || headersString.isEmpty()) {
            return headersMap;
        }

        // Split the string using the original "\n" delimiter based on LoadUrlParams
        String[] lines = headersString.split("\n");

        for (String line : lines) {
            // Skip empty lines (e.g., if there was a trailing newline)
            if (line.isEmpty()) {
                continue;
            }

            // Find the first colon to separate the key and the value.
            // We use indexOf instead of split(":") because the header value
            // itself might contain colons (e.g., "https://...").
            int colonIndex = line.indexOf(':');

            if (colonIndex > 0) {
                String key = line.substring(0, colonIndex);
                // The original method appends the value immediately after the colon
                String value = line.substring(colonIndex + 1).trim();

                headersMap.put(key, value);
            }
        }

        return headersMap;
    }
}
