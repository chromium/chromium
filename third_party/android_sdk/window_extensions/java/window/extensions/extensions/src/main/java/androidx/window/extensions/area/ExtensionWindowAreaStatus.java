/*
 * Copyright 2023 The Android Open Source Project
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

package androidx.window.extensions.area;

import android.util.DisplayMetrics;

import androidx.annotation.NonNull;

/**
 * Interface to provide information around the current status of a window area feature.
 *
 * Since {@link androidx.window.extensions.WindowExtensions#VENDOR_API_LEVEL_3}
 * @see WindowAreaComponent#addRearDisplayPresentationStatusListener
 */
public interface ExtensionWindowAreaStatus {

    /**
     * Returns the {@link androidx.window.extensions.area.WindowAreaComponent.WindowAreaStatus}
     * value that relates to the current status of a feature.
     */
    @WindowAreaComponent.WindowAreaStatus
    int getWindowAreaStatus();

    /**
     * Returns the {@link DisplayMetrics} that corresponds to the window area that a feature
     * interacts with. This is converted to size class information provided to developers.
     */
    @NonNull
    DisplayMetrics getWindowAreaDisplayMetrics();
}
