/*
 * Copyright 2021 The Android Open Source Project
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

import android.graphics.Rect;

import androidx.annotation.NonNull;

/**
 * Description of a physical feature on the display.
 */
public interface DisplayFeature {

    /**
     * The bounding rectangle of the feature within the application window
     * in the window coordinate space.
     *
     * @return bounds of display feature.
     */
    @NonNull
    Rect getBounds();
}
