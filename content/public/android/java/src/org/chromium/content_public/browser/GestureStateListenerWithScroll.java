// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/**
 * Adds notification of changes in scroll offset/extent to GestureStateListener. Tracking changes to
 * scroll offset/extent impacts performance, which is why it's separated in its own interface.
 */
public abstract class GestureStateListenerWithScroll extends GestureStateListener {
    /**
     * Called when the scroll offsets or extents may have changed.
     */
    public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {}
}
