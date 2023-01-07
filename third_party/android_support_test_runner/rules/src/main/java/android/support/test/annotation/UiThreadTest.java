/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.support.test.annotation;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * This annotation should be used along with {@link android.support.test.rule.UiThreadTestRule}
 * or with any rule that inherits from it. When the annotation is present, the test method is
 * executed on the application's UI thread (or main thread).
 * <p/>
 * Note, due to current JUnit limitation, methods annotated with
 * <a href="http://junit.sourceforge.net/javadoc/org/junit/Before.html"><code>Before</code></a> and
 * <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html"><code>After</code></a> will
 * also be executed on the UI Thread. Consider using
 * {@link android.support.test.rule.UiThreadTestRule#runOnUiThread(Runnable)} if this is an
 * issue.
 *
 * @see android.support.test.rule.UiThreadTestRule#runOnUiThread(Runnable)
 */
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface UiThreadTest {
}