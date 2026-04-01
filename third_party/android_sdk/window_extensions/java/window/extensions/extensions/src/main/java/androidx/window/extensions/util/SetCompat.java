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

package androidx.window.extensions.util;

import android.os.Build;
import android.util.ArraySet;

import androidx.annotation.RequiresApi;
import androidx.annotation.RestrictTo;

import org.jspecify.annotations.NonNull;

import java.util.HashSet;
import java.util.Set;

/**
 * A {@link Set} wrapper for compatibility. It {@link ArraySet} if it's available, and uses other
 * compatible {@link Set} class, otherwise.
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
public final class SetCompat {

    private SetCompat() {}

    /**
     * Creates a {@link Set}.
     *
     * @param <T> the type of the {@link Set}.
     */
    public static <T> @NonNull Set<T> create() {
        if (Build.VERSION.SDK_INT < 23) {
            return new HashSet<>();
        } else {
            return Api23Impl.create();
        }
    }

    @RequiresApi(23)
    private static class Api23Impl {

        static <T> @NonNull Set<T> create() {
            return new ArraySet<>();
        }
    }
}
