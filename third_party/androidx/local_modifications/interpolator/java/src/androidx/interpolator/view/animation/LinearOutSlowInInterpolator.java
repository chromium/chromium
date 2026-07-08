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
 * Interpolator corresponding to {@link android.R.interpolator#linear_out_slow_in}.
 *
 * Uses a lookup table for the Bezier curve from (0,0) to (1,1) with control points:
 * P0 (0, 0)
 * P1 (0, 0)
 * P2 (0.2, 1.0)
 * P3 (1.0, 1.0)
 */
public class LinearOutSlowInInterpolator implements Interpolator {

    /**
     * Lookup table values sampled with x at regular intervals between 0 and 1 for a total of
     * 201 points.
     */
    private static final float[] VALUES = new float[] {
            0.0000f, 0.0222f, 0.0424f, 0.0613f, 0.0793f, 0.0966f, 0.1132f,
            0.1293f, 0.1449f, 0.1600f, 0.1747f, 0.1890f, 0.2029f, 0.2165f,
            0.2298f, 0.2428f, 0.2555f, 0.2680f, 0.2802f, 0.2921f, 0.3038f,
            0.3153f, 0.3266f, 0.3377f, 0.3486f, 0.3592f, 0.3697f, 0.3801f,
            0.3902f, 0.4002f, 0.4100f, 0.4196f, 0.4291f, 0.4385f, 0.4477f,
            0.4567f, 0.4656f, 0.4744f, 0.4831f, 0.4916f, 0.5000f, 0.5083f,
            0.5164f, 0.5245f, 0.5324f, 0.5402f, 0.5479f, 0.5555f, 0.5629f,
            0.5703f, 0.5776f, 0.5847f, 0.5918f, 0.5988f, 0.6057f, 0.6124f,
            0.6191f, 0.6257f, 0.6322f, 0.6387f, 0.6450f, 0.6512f, 0.6574f,
            0.6635f, 0.6695f, 0.6754f, 0.6812f, 0.6870f, 0.6927f, 0.6983f,
            0.7038f, 0.7093f, 0.7147f, 0.7200f, 0.7252f, 0.7304f, 0.7355f,
            0.7406f, 0.7455f, 0.7504f, 0.7553f, 0.7600f, 0.7647f, 0.7694f,
            0.7740f, 0.7785f, 0.7829f, 0.7873f, 0.7917f, 0.7959f, 0.8002f,
            0.8043f, 0.8084f, 0.8125f, 0.8165f, 0.8204f, 0.8243f, 0.8281f,
            0.8319f, 0.8356f, 0.8392f, 0.8429f, 0.8464f, 0.8499f, 0.8534f,
            0.8568f, 0.8601f, 0.8634f, 0.8667f, 0.8699f, 0.8731f, 0.8762f,
            0.8792f, 0.8823f, 0.8852f, 0.8882f, 0.8910f, 0.8939f, 0.8967f,
            0.8994f, 0.9021f, 0.9048f, 0.9074f, 0.9100f, 0.9125f, 0.9150f,
            0.9174f, 0.9198f, 0.9222f, 0.9245f, 0.9268f, 0.9290f, 0.9312f,
            0.9334f, 0.9355f, 0.9376f, 0.9396f, 0.9416f, 0.9436f, 0.9455f,
            0.9474f, 0.9492f, 0.9510f, 0.9528f, 0.9545f, 0.9562f, 0.9579f,
            0.9595f, 0.9611f, 0.9627f, 0.9642f, 0.9657f, 0.9672f, 0.9686f,
            0.9700f, 0.9713f, 0.9726f, 0.9739f, 0.9752f, 0.9764f, 0.9776f,
            0.9787f, 0.9798f, 0.9809f, 0.9820f, 0.9830f, 0.9840f, 0.9849f,
            0.9859f, 0.9868f, 0.9876f, 0.9885f, 0.9893f, 0.9900f, 0.9908f,
            0.9915f, 0.9922f, 0.9928f, 0.9934f, 0.9940f, 0.9946f, 0.9951f,
            0.9956f, 0.9961f, 0.9966f, 0.9970f, 0.9974f, 0.9977f, 0.9981f,
            0.9984f, 0.9987f, 0.9989f, 0.9992f, 0.9994f, 0.9995f, 0.9997f,
            0.9998f, 0.9999f, 0.9999f, 1.0000f, 1.0000f
    };
    private static final float STEP = 1f / (VALUES.length - 1);

    @Override
    public float getInterpolation(float input) {
        return LookupTableInterpolator.interpolate(VALUES, STEP, input);
    }

}
