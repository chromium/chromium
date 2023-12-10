// Copyright 2021 The Chromium Authors
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
    private final int mWordStartAdjust;
    private final int mWordEndAdjust;

    /**
     * @return The start offset difference between the extended selection and the initial selection
     *         (caret).
     */
    public int getExtendedStartAdjust() {
        return mExtendedStartAdjust;
    }

    /**
     * @return The end offset difference between the extended selection and the initial selection
     *         (caret).
     */
    public int getExtendedEndAdjust() {
        return mExtendedEndAdjust;
    }

    /**
     * @return The start offset difference between the word selection (regardless of the extended
     *         selection granularity) and the initial selection (caret).
     */
    public int getWordStartAdjust() {
        return mWordStartAdjust;
    }

    /**
     * @return The end offset difference between the word selection (regardless of the extended
     *         selection granularity) and the initial selection (caret).
     */
    public int getWordEndAdjust() {
        return mWordEndAdjust;
    }

    /**
     * Create {@link SelectAroundCaretResult} instance.
     * @param extendedStartAdjust The start offset difference between the extended selection and the
     *         initial selection (caret).
     * @param extendedEndAdjust The end offset difference between the extended selection and the
     *         initial selection (caret).
     * @param wordStartAdjust The start offset difference between the word selection (regardless of
     *         the extended selection granularity) and the initial selection (caret).
     * @param wordEndAdjust The end offset difference between the word selection (regardless of the
     *         extended selection granularity) and the initial selection (caret).
     */
    public SelectAroundCaretResult(
            int extendedStartAdjust,
            int extendedEndAdjust,
            int wordStartAdjust,
            int wordEndAdjust) {
        mExtendedStartAdjust = extendedStartAdjust;
        mExtendedEndAdjust = extendedEndAdjust;
        mWordStartAdjust = wordStartAdjust;
        mWordEndAdjust = wordEndAdjust;
    }
}
