// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/**
 * A list of parameters about a selection attempt. This data is generated from
 * third_party/blink/public/mojom/input/input_handler.mojom's SelectAroundCaretResult.
 */
public class SelectAroundCaretResult {
    private final int mExtendedStartAdjust;
    private final int mExtendedEndAdjust;

    /**
     * @return The start offset difference between the extended selection (if
     * not using Word granularity) and the previous selection (caret).
     */
    public int getExtendedStartAdjust() {
        return mExtendedStartAdjust;
    }

    /**
     * @return The end offset difference between the extended selection (if not
     * using Word granularity) and the previous selection (caret).
     */
    public int getExtendedEndAdjust() {
        return mExtendedEndAdjust;
    }

    /**
     * Create {@link SelectAroundCaretResult} instance.
     * @param extendedStartAdjust The start offset difference between the extended selection and the
     *         previous selection (caret).
     * @param extendedEndAdjust The end offset difference between the extended selection and the
     *         previous selection (caret).
     */
    public SelectAroundCaretResult(int extendedStartAdjust, int extendedEndAdjust) {
        mExtendedStartAdjust = extendedStartAdjust;
        mExtendedEndAdjust = extendedEndAdjust;
    }
}