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

package android.support.test;

import android.app.Instrumentation;
import android.content.Context;
import android.os.Bundle;
import java.util.concurrent.atomic.AtomicReference;

/**
 * An exposed registry instance that holds a reference to the instrumentation running in the
 * process and it's arguments. Also provides an easy way for callers to get a hold of
 * instrumentation, application context and instrumentation arguments Bundle.
 */
public final class InstrumentationRegistry {

    private static final AtomicReference<Instrumentation> sInstrumentationRef =
            new AtomicReference<Instrumentation>(null);
    private static final AtomicReference<Bundle> sArguments = new AtomicReference<Bundle>(null);

    /**
     * Returns the instrumentation currently running. Use this to get an {@link Instrumentation}
     * into your test.
     *
     * @throws IllegalStateException if instrumentation hasn't been registered
     */
    public static Instrumentation getInstrumentation() {
        Instrumentation instance = sInstrumentationRef.get();
        if (null == instance) {
            throw new IllegalStateException("No instrumentation registered! "
                    + "Must run under a registering instrumentation.");
        }
        return instance;
    }

    /**
     * Returns a copy of instrumentation arguments Bundle. Use this to get a {@link Bundle}
     * containing the command line arguments passed to {@link Instrumentation} into your test.
     * <p>
     * This Bundle is not guaranteed to be present under all instrumentations.
     * </p>
     *
     * @return Bundle the arguments for this instrumentation.
     * @throws IllegalStateException if no argument Bundle has been registered.
     */
    public static Bundle getArguments() {
        Bundle instance = sArguments.get();
        if (null == instance) {
            throw new IllegalStateException("No instrumentation arguments registered! "
                    + "Are you running under an Instrumentation which registers arguments?");
        }
        return new Bundle(instance);
    }

    /**
     * Return the Context of this instrumentation's package. Use this to get a
     * {@link Context} representing {@link Instrumentation#getContext()} into your test.
     */
    public static Context getContext() {
        return getInstrumentation().getContext();
    }

    /**
     * Return a Context for the target application being instrumented. Use this to get a
     * {@link Context} representing {@link Instrumentation#getTargetContext()} into your test.
     */
    public static Context getTargetContext() {
        return getInstrumentation().getTargetContext();
    }

    /**
     * Records/exposes the instrumentation currently running and stores a copy of the
     * instrumentation arguments Bundle in the registry.
     * <p>
     * This is a global registry - so be aware of the impact of calling this method!
     * </p>
     *
     * @param instrumentation the instrumentation currently running.
     * @param arguments the arguments for this application. Null deregisters any existing arguments.
     */
    public static void registerInstance(Instrumentation instrumentation, Bundle arguments) {
        sInstrumentationRef.set(instrumentation);
        sArguments.set(new Bundle(arguments));
    }

    private InstrumentationRegistry() {
    }

}
