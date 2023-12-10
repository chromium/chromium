// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.os.Handler;

import org.chromium.build.annotations.UsedByReflection;

/**
 * An interface to provide smart clip data when requested.
 *
 * NOTE: Some platforms may call these functions to extract smart clip data.
 * Please make sure implementation of them is somewhere in the view
 * hierarchy.
 */
@UsedByReflection("ExternalOemSupport")
public interface SmartClipProvider {
    /**
     * Initiate extraction of text, HTML, and other information for clipping puposes (smart clip)
     * from the rectangle area defined by starting positions (x and y), and width and height.
     */
    @UsedByReflection("ExternalOemSupport")
    void extractSmartClipData(int x, int y, int width, int height);

    /** Register a handler to handle smart clip data once extraction is done. */
    @UsedByReflection("ExternalOemSupport")
    void setSmartClipResultHandler(final Handler resultHandler);
}
