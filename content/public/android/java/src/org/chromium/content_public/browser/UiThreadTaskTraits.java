// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.base.task.TaskTraits;

/**
 * Traits for tasks that need to run on the Browser UI thread. Keep in sync with
 * content::BrowserTaskTraitsExtension.
 *
 * NB if you wish to post to the thread pool then use {@link TaskTraits} instead of {@link
 * UiThreadTaskTraits}.
 */
public class UiThreadTaskTraits extends TaskTraits {
    // Corresponds to content::BrowserTaskTraitsExtension.
    static final byte EXTENSION_ID = 1;

    private static final byte UI_THREAD_ID = 0; // Corresponds to content::BrowserThread::ID.

    // Keep in sync with content::BrowserTaskTraitsExtension::Serialize.
    private static final byte THREAD_INDEX = 0;
    private static final byte NESTING_INDEX = 1;

    private static final byte[] sDefaultExtensionData = getDefaultExtesionData();

    public UiThreadTaskTraits() {
        setExtensionId(EXTENSION_ID);
        setExtensionData(sDefaultExtensionData);
    }

    private static byte[] getDefaultExtesionData() {
        byte extensionData[] = new byte[TaskTraits.EXTENSION_STORAGE_SIZE];

        // Note we don't specify the UI thread directly here because it's ID 0 and the array is
        // initialized to zero.

        // TODO(crbug.com/876272) Remove this if possible.
        extensionData[NESTING_INDEX] = 1; // Allow the task to run in a nested RunLoop.
        return extensionData;
    }
}
