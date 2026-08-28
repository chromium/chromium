// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.AnyThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/**
 * {@link PageState} is an incomplete copy of {@link Page} at a certain point in time, which occurs
 * after every time after a value is set.
 */
@AnyThread
@NullMarked
public class PageState {

    private final GURL mUrl;

    public PageState(GURL url) {
        mUrl = url;
    }

    public GURL getUrl() {
        return mUrl;
    }
}
