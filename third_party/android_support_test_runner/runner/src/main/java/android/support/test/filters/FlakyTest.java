/*
 * Copyright (C) 2014 The Android Open Source Project
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
package android.support.test.filters;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Designates a test as being flaky (non-deterministic).
 * <p/>
 * Can then be used to filter tests on execution using -e annotation or -e notAnnotation as
 * desired.
 */
@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.METHOD, ElementType.TYPE})
public @interface FlakyTest {
    /**
     * An optional bug number associated with the test. -1 Means that no bug number
     * is associated with the flaky annotation.
     *
     * @return int
     */
    int bugId() default -1;

    /**
     * Details, such as the reason of why the test is flaky.
     *
     * @return String
     */
    String detail() default "";
}