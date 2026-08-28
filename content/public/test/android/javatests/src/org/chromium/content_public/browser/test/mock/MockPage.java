// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.mock;

import androidx.annotation.AnyThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.PageState;
import org.chromium.url.GURL;

/** Mock class for {@link Page}. */
@NullMarked
public class MockPage implements Page {
    private boolean mIsPrerendering;
    private GURL mUrl = GURL.emptyGURL();
    private volatile PageState mMostRecentPageState;

    public MockPage() {
        takePageSnapshot();
    }

    @Override
    public void setPageDeletionListener(PageDeletionListener listener) {}

    @Override
    public boolean isPrerendering() {
        return mIsPrerendering;
    }

    @Override
    public void setIsPrerendering(boolean isPrerendering) {
        mIsPrerendering = isPrerendering;
    }

    @Override
    public GURL getUrl() {
        return mUrl;
    }

    @Override
    public void setUrl(GURL url) {
        mUrl = url;
        takePageSnapshot();
    }

    @Override
    @AnyThread
    public PageState getMostRecentPageState() {
        return mMostRecentPageState;
    }

    /** Take a snapshot of the current state of Page */
    private void takePageSnapshot() {
        mMostRecentPageState = new PageState(mUrl);
    }
}
