// Copyright 2018 The Chromium Authors
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
public class UiThreadTaskTraits {
    private UiThreadTaskTraits() {}

    // These are convenience constants for UI thread tasks at different priority levels.
    public static final @TaskTraits int DEFAULT = TaskTraits.UI_DEFAULT;
    public static final @TaskTraits int BEST_EFFORT = TaskTraits.UI_BEST_EFFORT;
    public static final @TaskTraits int USER_VISIBLE = TaskTraits.UI_USER_VISIBLE;
    public static final @TaskTraits int USER_BLOCKING = TaskTraits.UI_USER_BLOCKING;
}
