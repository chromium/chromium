// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated using
//     mojo/tools/generate_java_callback_interfaces.py

package org.chromium.mojo.bindings;

/** Contains a generic interface for callbacks. */
public interface Callbacks {

    /** A generic callback. */
    interface Callback0 {
        /** Call the callback. */
        public void call();
    }

    /**
     * A generic 1-argument callback.
     *
     * @param <T1> the type of argument 1.
     */
    interface Callback1<T1> {
        /** Call the callback. */
        public void call(T1 arg1);
    }

    /**
     * A generic 2-argument callback.
     *
     * @param <T1> the type of argument 1.
     * @param <T2> the type of argument 2.
     */
    interface Callback2<T1, T2> {
        /** Call the callback. */
        public void call(T1 arg1, T2 arg2);
    }

    /**
     * A generic 3-argument callback.
     *
     * @param <T1> the type of argument 1.
     * @param <T2> the type of argument 2.
     * @param <T3> the type of argument 3.
     */
    interface Callback3<T1, T2, T3> {
        /** Call the callback. */
        public void call(T1 arg1, T2 arg2, T3 arg3);
    }

    /**
     * A generic 4-argument callback.
     *
     * @param <T1> the type of argument 1.
     * @param <T2> the type of argument 2.
     * @param <T3> the type of argument 3.
     * @param <T4> the type of argument 4.
     */
    interface Callback4<T1, T2, T3, T4> {
        /** Call the callback. */
        public void call(T1 arg1, T2 arg2, T3 arg3, T4 arg4);
    }

    /**
     * A generic 5-argument callback.
     *
     * @param <T1> the type of argument 1.
     * @param <T2> the type of argument 2.
     * @param <T3> the type of argument 3.
     * @param <T4> the type of argument 4.
     * @param <T5> the type of argument 5.
     */
    interface Callback5<T1, T2, T3, T4, T5> {
        /** Call the callback. */
        public void call(T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5);
    }

    /**
     * A generic 6-argument callback.
     *
     * @param <T1> the type of argument 1.
     * @param <T2> the type of argument 2.
     * @param <T3> the type of argument 3.
     * @param <T4> the type of argument 4.
     * @param <T5> the type of argument 5.
     * @param <T6> the type of argument 6.
     */
    interface Callback6<T1, T2, T3, T4, T5, T6> {
        /** Call the callback. */
        public void call(T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6);
    }

    /**
     * A generic 7-argument callback.
     *
     * @param <T1> the type of argument 1.
     * @param <T2> the type of argument 2.
     * @param <T3> the type of argument 3.
     * @param <T4> the type of argument 4.
     * @param <T5> the type of argument 5.
     * @param <T6> the type of argument 6.
     * @param <T7> the type of argument 7.
     */
    interface Callback7<T1, T2, T3, T4, T5, T6, T7> {
        /** Call the callback. */
        public void call(T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6, T7 arg7);
    }

    /**
     * A generic 11-argument callback.
     *
     * @param <T1> the type of argument 1.
     * @param <T2> the type of argument 2.
     * @param <T3> the type of argument 3.
     * @param <T4> the type of argument 4.
     * @param <T5> the type of argument 5.
     * @param <T6> the type of argument 6.
     * @param <T7> the type of argument 7.
     * @param <T8> the type of argument 8.
     * @param <T9> the type of argument 9.
     * @param <T10> the type of argument 10.
     * @param <T11> the type of argument 11.
     */
    interface Callback11<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11> {
        /** Call the callback. */
        public void call(
                T1 arg1,
                T2 arg2,
                T3 arg3,
                T4 arg4,
                T5 arg5,
                T6 arg6,
                T7 arg7,
                T8 arg8,
                T9 arg9,
                T10 arg10,
                T11 arg11);
    }

    /**
     * A generic 13-argument callback.
     *
     * @param <T1> the type of argument 1.
     * @param <T2> the type of argument 2.
     * @param <T3> the type of argument 3.
     * @param <T4> the type of argument 4.
     * @param <T5> the type of argument 5.
     * @param <T6> the type of argument 6.
     * @param <T7> the type of argument 7.
     * @param <T8> the type of argument 8.
     * @param <T9> the type of argument 9.
     * @param <T10> the type of argument 10.
     * @param <T11> the type of argument 11.
     * @param <T12> the type of argument 12.
     * @param <T13> the type of argument 13.
     */
    interface Callback13<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13> {
        /** Call the callback. */
        public void call(
                T1 arg1,
                T2 arg2,
                T3 arg3,
                T4 arg4,
                T5 arg5,
                T6 arg6,
                T7 arg7,
                T8 arg8,
                T9 arg9,
                T10 arg10,
                T11 arg11,
                T12 arg12,
                T13 arg13);
    }

    /**
     * A generic 22-argument callback.
     *
     * @param <T1> the type of argument 1.
     * @param <T2> the type of argument 2.
     * @param <T3> the type of argument 3.
     * @param <T4> the type of argument 4.
     * @param <T5> the type of argument 5.
     * @param <T6> the type of argument 6.
     * @param <T7> the type of argument 7.
     * @param <T8> the type of argument 8.
     * @param <T9> the type of argument 9.
     * @param <T10> the type of argument 10.
     * @param <T11> the type of argument 11.
     * @param <T12> the type of argument 12.
     * @param <T13> the type of argument 13.
     * @param <T14> the type of argument 14.
     * @param <T15> the type of argument 15.
     * @param <T16> the type of argument 16.
     * @param <T17> the type of argument 17.
     * @param <T18> the type of argument 18.
     * @param <T19> the type of argument 19.
     * @param <T20> the type of argument 20.
     * @param <T21> the type of argument 21.
     * @param <T22> the type of argument 22.
     */
    interface Callback22<
            T1,
            T2,
            T3,
            T4,
            T5,
            T6,
            T7,
            T8,
            T9,
            T10,
            T11,
            T12,
            T13,
            T14,
            T15,
            T16,
            T17,
            T18,
            T19,
            T20,
            T21,
            T22> {
        /** Call the callback. */
        public void call(
                T1 arg1,
                T2 arg2,
                T3 arg3,
                T4 arg4,
                T5 arg5,
                T6 arg6,
                T7 arg7,
                T8 arg8,
                T9 arg9,
                T10 arg10,
                T11 arg11,
                T12 arg12,
                T13 arg13,
                T14 arg14,
                T15 arg15,
                T16 arg16,
                T17 arg17,
                T18 arg18,
                T19 arg19,
                T20 arg20,
                T21 arg21,
                T22 arg22);
    }
}
