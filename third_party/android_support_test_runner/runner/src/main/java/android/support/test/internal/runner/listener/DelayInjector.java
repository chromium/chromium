/*
 * Copyright (C) 2012 The Android Open Source Project
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
package android.support.test.internal.runner.listener;

import android.util.Log;
import org.junit.runner.Description;
import org.junit.runner.notification.RunListener;

/**
 * A <a href="http://junit.org/javadoc/latest/org/junit/runner/notification/RunListener.html">
 * <code>RunListener</code></a> that injects a given delay between tests.
 */
public class DelayInjector extends RunListener {

    private final int mDelayMsec;

    /**
     * @param delayMsec
     */
    public DelayInjector(int delayMsec) {
        mDelayMsec = delayMsec;
    }

    @Override
    public void testRunStarted(Description description) throws Exception {
        // delay before first test
        delay();
    }

    @Override
    public void testFinished(Description description) throws Exception {
        // delay after every test
        delay();
    }

    private void delay() {
        try {
            Thread.sleep(mDelayMsec);
        } catch (InterruptedException e) {
            Log.e("DelayInjector", "interrupted", e);
        }
    }
}
