// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * This interface represents an animation job.
 * The client of AnimationJob is supposed to:
 * <ul>
 *  <li>Create AnimationJob instances at initialization stage and keep them alive for the whole
 *    life cycle.</li>
 *  <li>Call methods like startAnimation() to start animation. This method is not defined in the
 *    interface since start methods may vary.</li>
 *  <li>In its repaint/animation cycle, call processAnimation() to execute the animation job, and
 *    exit the animation cycle when all animations are finished.</li>
 *  <li>Call abortAnimation() when the animation need to be interrupted.</li>
 * </ul>
 */
interface AnimationJob {
    /**
     * Processes the animation when it is still active/not finished
     * @return true if the animation is not finished yet
     */
    boolean processAnimation();

    /**
     * Abort current animation. The animation will be marked as finished
     */
    void abortAnimation();
}