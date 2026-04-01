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

package androidx.window.extensions;

import androidx.window.extensions.area.WindowAreaComponent;
import androidx.window.extensions.embedding.ActivityEmbeddingComponent;
import androidx.window.extensions.layout.WindowLayoutComponent;

import org.jspecify.annotations.Nullable;

/**
 * A class to provide instances of different WindowManager Jetpack extension components. An OEM must
 * implement all the availability methods to state which WindowManager Jetpack extension can be
 * used. If a component is not available then the check must return {@code false}. Trying to get a
 * component that is not available will throw an {@link UnsupportedOperationException}. All
 * components must support the API level returned in {@link WindowExtensions#getVendorApiLevel()}.
 */
public interface WindowExtensions {

    /**
     * Returns the API level of the vendor library on the device. If the returned version is not
     * supported by the WindowManager library, then some functions may not be available or replaced
     * with stub implementations.
     *
     * <p>The expected use case is for the WindowManager library to determine which APIs are
     * available and wrap the API so that app developers do not need to deal with the complexity.
     *
     * @return the API level supported by the library.
     */
    default int getVendorApiLevel() {
        throw new RuntimeException("Not implemented. Must override in a subclass.");
    }

    /**
     * Returns the OEM implementation of {@link WindowLayoutComponent} if it is supported on the
     * device, {@code null} otherwise. The implementation must match the API level reported in
     * {@link WindowExtensions}.
     *
     * @return the OEM implementation of {@link WindowLayoutComponent}
     */
    @Nullable WindowLayoutComponent getWindowLayoutComponent();

    /**
     * Returns the OEM implementation of {@link ActivityEmbeddingComponent} if it is supported on
     * the device, {@code null} otherwise. The implementation must match the API level reported in
     * {@link WindowExtensions}.
     *
     * @return the OEM implementation of {@link ActivityEmbeddingComponent}
     */
    default @Nullable ActivityEmbeddingComponent getActivityEmbeddingComponent() {
        return null;
    }

    /**
     * Returns the OEM implementation of {@link WindowAreaComponent} if it is supported on the
     * device, {@code null} otherwise. The implementation must match the API level reported in
     * {@link WindowExtensions}.
     *
     * @return the OEM implementation of {@link WindowAreaComponent}
     */
    default @Nullable WindowAreaComponent getWindowAreaComponent() {
        return null;
    }
}
