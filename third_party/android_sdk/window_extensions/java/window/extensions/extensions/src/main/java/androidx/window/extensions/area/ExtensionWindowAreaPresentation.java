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

import android.content.Context;
import android.view.View;
import android.view.Window;

import androidx.window.extensions.RequiresVendorApiLevel;

import org.jspecify.annotations.NonNull;

/**
 * An interface representing a container in an extension window area in which app content can be
 * shown.
 *
 * @see WindowAreaComponent#getRearDisplayPresentation()
 */
public interface ExtensionWindowAreaPresentation {

    /**
     * Returns the {@link Context} for the window that is being used to display the additional
     * content provided from the application.
     */
    @RequiresVendorApiLevel(level = 3)
    @NonNull Context getPresentationContext();

    /** Sets the {@link View} that the application wants to display in the extension window area. */
    @RequiresVendorApiLevel(level = 3)
    void setPresentationView(@NonNull View view);

    /** Returns the {@link Window} for the rear display presentation area. */
    @RequiresVendorApiLevel(level = 4)
    default @NonNull Window getWindow() {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }
}
