// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

/** Constants to be used by child processes. */
public interface ContentChildProcessConstants {
    // Below are the names for the items placed in the bind or start command intent.
    // Note that because that intent maybe reused if a service is restarted, none should be process
    // specific.

    // Below are the names for the items placed in the Bundle passed in the
    // IChildProcessService.setupConnection call, once the connection has been established.

    // Key for the number of CPU cores.
    public static final String EXTRA_CPU_COUNT = "com.google.android.apps.chrome.extra.cpu_count";

    // Key for the CPU features mask.
    public static final String EXTRA_CPU_FEATURES =
            "com.google.android.apps.chrome.extra.cpu_features";
}
