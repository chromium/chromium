// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test.util;

import org.chromium.base.ThreadUtils;

import java.util.concurrent.FutureTask;

/** A utility class useful for testing NetworkChangeNotifier. */
public class NetworkChangeNotifierTestUtil {
    /** Flushes UI thread task queue. */
    public static void flushUiThreadTaskQueue() throws Exception {
        FutureTask<Void> task =
                new FutureTask<Void>(
                        new Runnable() {
                            @Override
                            public void run() {}
                        },
                        null);
        ThreadUtils.postOnUiThread(task);
        task.get();
    }
}
