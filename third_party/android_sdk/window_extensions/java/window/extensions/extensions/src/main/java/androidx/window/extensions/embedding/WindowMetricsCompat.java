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

package androidx.window.extensions.embedding;

import android.os.Build;
import android.view.WindowMetrics;

import androidx.annotation.RequiresApi;

import org.jspecify.annotations.NonNull;

/** A helper class to access {@link WindowMetrics#toString()} with compatibility. */
class WindowMetricsCompat {
    private WindowMetricsCompat() {}

    static @NonNull String toString(@NonNull WindowMetrics windowMetrics) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            // WindowMetrics#toString is implemented in U.
            return windowMetrics.toString();
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return Api30Impl.toString(windowMetrics);
        }
        // Should be safe since ActivityEmbedding is not enabled before R.
        throw new UnsupportedOperationException("WindowMetrics didn't exist in R.");
    }

    @RequiresApi(30)
    private static final class Api30Impl {
        static @NonNull String toString(@NonNull WindowMetrics windowMetrics) {
            return WindowMetrics.class.getSimpleName()
                    + ":{"
                    + "bounds="
                    + windowMetrics.getBounds()
                    + ", windowInsets="
                    + windowMetrics.getWindowInsets()
                    + "}";
        }
    }
}
