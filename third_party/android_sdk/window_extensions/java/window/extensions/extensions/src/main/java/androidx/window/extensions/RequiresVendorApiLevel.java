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

package androidx.window.extensions;

import androidx.annotation.IntRange;
import androidx.annotation.RestrictTo;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** The annotation used to document the minimum supported vendor API level of the denoted target. */
@Retention(value = RetentionPolicy.RUNTIME)
@Target({ElementType.TYPE, ElementType.CONSTRUCTOR, ElementType.METHOD, ElementType.FIELD})
@RestrictTo(RestrictTo.Scope.LIBRARY)
public @interface RequiresVendorApiLevel {

    /** The minimum required vendor API level of the denoted target. */
    @IntRange(from = 1)
    int level();

    /**
     * The vendor API level that the denoted target started to deprecated, which defaults to {@link
     * #VENDOR_API_LEVEL_NOT_SET}.
     */
    int deprecatedSince() default VENDOR_API_LEVEL_NOT_SET;

    /** Indicates the denoted target is not deprecated. */
    int VENDOR_API_LEVEL_NOT_SET = -1;
}
