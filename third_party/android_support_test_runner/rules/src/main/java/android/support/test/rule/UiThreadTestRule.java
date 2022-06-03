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

package android.support.test.rule;

import static android.support.test.InstrumentationRegistry.getInstrumentation;

import android.os.Looper;
import android.support.test.annotation.Beta;
import android.support.test.annotation.UiThreadTest;
import android.support.test.internal.statement.UiThreadStatement;
import android.util.Log;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * This rule allows the test method annotated with {@link UiThreadTest} to execute on the
 * application's main thread (or UI thread).
 * <p/>
 * Note, methods annotated with
 * <a href="http://junit.sourceforge.net/javadoc/org/junit/Before.html"><code>Before</code></a> and
 * <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html"><code>After</code></a> will
 * also be executed on the UI thread.
 *
 * @see android.support.test.annotation.UiThreadTest
 */
@Beta
public class UiThreadTestRule implements TestRule {
    private static final String LOG_TAG = "UiThreadTestRule";

    @Override
    public Statement apply(final Statement base, Description description) {
        return new UiThreadStatement(base, shouldRunOnUiThread(description));
    }

    protected boolean shouldRunOnUiThread(Description description) {
        return description.getAnnotation(UiThreadTest.class) != null;
    }

    /**
     * Helper for running portions of a test on the UI thread.
     * <p/>
     * Note, in most cases it is simpler to annotate the test method with
     * {@link UiThreadTest}, which will run the entire test method including methods annotated with
     * <a href="http://junit.sourceforge.net/javadoc/org/junit/Before.html"><code>Before</code></a>
     * and <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html">
     * <code>After</code></a> on the UI thread.
     * <p/>
     * Use this method if you need to switch in and out of the UI thread to perform your test.
     *
     * @param runnable runnable containing test code in the {@link Runnable#run()} method
     *
     * @see android.support.test.annotation.UiThreadTest
     */
    public void runOnUiThread(final Runnable runnable) throws Throwable {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            Log.w(LOG_TAG, "Already on the UI thread, this method should not be called from the " +
                    "main application thread");
            runnable.run();
        } else {
            FutureTask<Void> task = new FutureTask<>(runnable, null);
            getInstrumentation().runOnMainSync(task);
            try {
                task.get();
            } catch (ExecutionException e) {
                // Expose the original exception
                throw e.getCause();
            }
        }
    }
}
