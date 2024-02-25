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

package androidx.window.extensions.core.util.function;

/**
 * Represents a function that accepts an argument and produces no result.
 * It is used internally to avoid using Java 8 functional interface that leads to desugaring and
 * Proguard shrinking.
 *
 * @param <T>: the type of the input of the function
 *
 * @see java.util.function.Consumer
 */
@FunctionalInterface
public interface Consumer<T> {
    /**
     * Performs the operation on the given argument
     *
     * @param t the input argument
     */
    void accept(T t);
}
