// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.content.browser.selection.LGEmailActionModeWorkaroundImpl;

/**
 * This is a workaround for LG Email app: https://crbug.com/651706
 * LG Email app runs UI-thread APIs from InputConnection methods. This is not allowable with
 * the change ImeThread introduces, and LG Email app is bundled and cannot be updated without
 * a system update. However, LG Email team is committed to fixing this in the near future.
 * This is a version code limited workaround to avoid crashes in the app.
 */
public final class LGEmailActionModeWorkaround {
    private LGEmailActionModeWorkaround() {}

    public static boolean isSafeVersion(int versionCode) {
        return LGEmailActionModeWorkaroundImpl.isSafeVersion(versionCode);
    }
}
