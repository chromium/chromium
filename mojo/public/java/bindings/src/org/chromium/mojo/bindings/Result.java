// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.NoSuchElementException;

/**
 * A result container for mojo methods using the {@code result<T,E>} return type. The container will
 * either contain a success value of type <T>, or an error of type <E>. Users must check {@link
 * isSuccess} before attempting to retrieve the value.
 *
 * <p>TODO(crbug.com/40841428): Document this API in the bindings README.md.
 *
 * @param <T> The success type.
 * @param <E> The error type.
 */
@NullMarked
public class Result<T, E> {
    @Nullable private final T mSuccess;
    @Nullable private final E mError;

    private Result(@Nullable T success, @Nullable E error) {
        assert (success == null && error != null) || (success != null && error == null);
        this.mSuccess = success;
        this.mError = error;
    }

    /** Constructs a result with a success value. */
    public static <T, E> Result<T, E> of(T success) {
        assert success != null;
        return new Result(success, null);
    }

    /** Constructs a result with an error. */
    public static <T, E> Result<T, E> ofError(E error) {
        assert error != null;
        return new Result(null, error);
    }

    /** Whether or not the result contains a success value. */
    public boolean isSuccess() {
        return mSuccess != null;
    }

    /**
     * Retrieves the success value. If the result does not contain a success value, a {@link
     * NoSuchElementException} is thrown.
     *
     * @return T
     * @throws NoSuchElementException if the result is not a success.
     */
    public T get() {
        if (mSuccess == null) {
            throw new NoSuchElementException("result is not a success");
        }
        return mSuccess;
    }

    /**
     * Retrieves the error. If the result does not contain an error a {@link NoSuchElementException}
     * is thrown.
     *
     * @return E
     * @throws NoSuchElementException if the result is not an error.
     */
    public E getError() {
        if (mError == null) {
            throw new NoSuchElementException("result is not a failure");
        }
        return mError;
    }
}
