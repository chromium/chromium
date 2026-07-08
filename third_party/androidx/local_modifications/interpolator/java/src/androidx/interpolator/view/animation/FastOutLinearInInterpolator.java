/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package androidx.interpolator.view.animation;

import android.view.animation.Interpolator;

/**
 * Interpolator corresponding to {@link android.R.interpolator#fast_out_linear_in}.
 *
 * Uses a lookup table for the Bezier curve from (0,0) to (1,1) with control points:
 * P0 (0, 0)
 * P1 (0.4, 0)
 * P2 (1.0, 1.0)
 * P3 (1.0, 1.0)
 */
public class FastOutLinearInInterpolator implements Interpolator {

    /**
     * Lookup table values sampled with x at regular intervals between 0 and 1 for a total of
     * 201 points.
     */
    private static final float[] VALUES = new float[] {
            0.0000f, 0.0001f, 0.0002f, 0.0005f, 0.0008f, 0.0013f, 0.0018f,
            0.0024f, 0.0032f, 0.0040f, 0.0049f, 0.0059f, 0.0069f, 0.0081f,
            0.0093f, 0.0106f, 0.0120f, 0.0135f, 0.0151f, 0.0167f, 0.0184f,
            0.0201f, 0.0220f, 0.0239f, 0.0259f, 0.0279f, 0.0300f, 0.0322f,
            0.0345f, 0.0368f, 0.0391f, 0.0416f, 0.0441f, 0.0466f, 0.0492f,
            0.0519f, 0.0547f, 0.0574f, 0.0603f, 0.0632f, 0.0662f, 0.0692f,
            0.0722f, 0.0754f, 0.0785f, 0.0817f, 0.0850f, 0.0884f, 0.0917f,
            0.0952f, 0.0986f, 0.1021f, 0.1057f, 0.1093f, 0.1130f, 0.1167f,
            0.1205f, 0.1243f, 0.1281f, 0.1320f, 0.1359f, 0.1399f, 0.1439f,
            0.1480f, 0.1521f, 0.1562f, 0.1604f, 0.1647f, 0.1689f, 0.1732f,
            0.1776f, 0.1820f, 0.1864f, 0.1909f, 0.1954f, 0.1999f, 0.2045f,
            0.2091f, 0.2138f, 0.2184f, 0.2232f, 0.2279f, 0.2327f, 0.2376f,
            0.2424f, 0.2473f, 0.2523f, 0.2572f, 0.2622f, 0.2673f, 0.2723f,
            0.2774f, 0.2826f, 0.2877f, 0.2929f, 0.2982f, 0.3034f, 0.3087f,
            0.3141f, 0.3194f, 0.3248f, 0.3302f, 0.3357f, 0.3412f, 0.3467f,
            0.3522f, 0.3578f, 0.3634f, 0.3690f, 0.3747f, 0.3804f, 0.3861f,
            0.3918f, 0.3976f, 0.4034f, 0.4092f, 0.4151f, 0.4210f, 0.4269f,
            0.4329f, 0.4388f, 0.4448f, 0.4508f, 0.4569f, 0.4630f, 0.4691f,
            0.4752f, 0.4814f, 0.4876f, 0.4938f, 0.5000f, 0.5063f, 0.5126f,
            0.5189f, 0.5252f, 0.5316f, 0.5380f, 0.5444f, 0.5508f, 0.5573f,
            0.5638f, 0.5703f, 0.5768f, 0.5834f, 0.5900f, 0.5966f, 0.6033f,
            0.6099f, 0.6166f, 0.6233f, 0.6301f, 0.6369f, 0.6436f, 0.6505f,
            0.6573f, 0.6642f, 0.6710f, 0.6780f, 0.6849f, 0.6919f, 0.6988f,
            0.7059f, 0.7129f, 0.7199f, 0.7270f, 0.7341f, 0.7413f, 0.7484f,
            0.7556f, 0.7628f, 0.7700f, 0.7773f, 0.7846f, 0.7919f, 0.7992f,
            0.8066f, 0.8140f, 0.8214f, 0.8288f, 0.8363f, 0.8437f, 0.8513f,
            0.8588f, 0.8664f, 0.8740f, 0.8816f, 0.8892f, 0.8969f, 0.9046f,
            0.9124f, 0.9201f, 0.9280f, 0.9358f, 0.9437f, 0.9516f, 0.9595f,
            0.9675f, 0.9755f, 0.9836f, 0.9918f, 1.0000f
    };
    private static final float STEP = 1f / (VALUES.length - 1);

    @Override
    public float getInterpolation(float input) {
        return LookupTableInterpolator.interpolate(VALUES, STEP, input);
    }
}
