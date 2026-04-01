/*
 * Copyright 2024 The Android Open Source Project
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

package androidx.window.extensions.layout;

import androidx.window.extensions.RequiresVendorApiLevel;

import org.jspecify.annotations.NonNull;

import java.util.ArrayList;
import java.util.List;

/**
 * A class to represent all the possible features that may interact with or appear in a window, that
 * an application might want to respond to.
 */
public final class SupportedWindowFeatures {

    private final List<DisplayFoldFeature> mDisplayFoldFeatureList;

    private SupportedWindowFeatures(@NonNull List<DisplayFoldFeature> displayFoldFeatureList) {
        mDisplayFoldFeatureList = new ArrayList<>(displayFoldFeatureList);
    }

    /** Returns the possible {@link DisplayFoldFeature}s that can be reported to an application. */
    @RequiresVendorApiLevel(level = 6)
    public @NonNull List<DisplayFoldFeature> getDisplayFoldFeatures() {
        return new ArrayList<>(mDisplayFoldFeatureList);
    }

    /** A class to create a {@link SupportedWindowFeatures} instance. */
    public static final class Builder {

        private final List<DisplayFoldFeature> mDisplayFoldFeatures;

        /** Creates a new instance of {@link Builder} */
        @RequiresVendorApiLevel(level = 6)
        public Builder(@NonNull List<DisplayFoldFeature> displayFoldFeatures) {
            mDisplayFoldFeatures = new ArrayList<>(displayFoldFeatures);
        }

        /** Creates an instance of {@link SupportedWindowFeatures} with the features set. */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull SupportedWindowFeatures build() {
            return new SupportedWindowFeatures(mDisplayFoldFeatures);
        }
    }
}
