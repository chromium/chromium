package android.support.test.internal.runner;

import android.app.Instrumentation;
import android.os.Bundle;
import android.os.Debug;
import android.support.test.internal.runner.listener.InstrumentationRunListener;
import android.support.test.internal.util.Checks;
import android.util.Log;
import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.List;
import org.junit.runner.Description;
import org.junit.runner.JUnitCore;
import org.junit.runner.Result;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunListener;

/**
 * Class that given a TestRequest containing tests to run, wires up the test listeners and
 * actually executes the test using upstream JUnit
 */
public final class TestExecutor {
    private static final String LOG_TAG = "TestExecutor";

    private final List<RunListener> mListeners;
    private final boolean mWaitForDebugger;
    private final Instrumentation mInstr;

    private TestExecutor(Builder builder) {
        mListeners = Checks.checkNotNull(builder.mListeners);
        mWaitForDebugger = builder.mWaitForDebugger;
        mInstr = builder.mInstr;
    }

    /**
     * Execute the tests
     */
    public Bundle execute(TestRequest testRequest) {
        Bundle resultBundle = new Bundle();

        if (mWaitForDebugger) {
            Log.i(LOG_TAG, "Waiting for debugger to connect...");
            Debug.waitForDebugger();
            Log.i(LOG_TAG, "Debugger connected.");
        }

        Result junitResults = new Result();
        try {
            JUnitCore testRunner = new JUnitCore();
            setUpListeners(testRunner);
            junitResults = testRunner.run(testRequest.getRequest());
            junitResults.getFailures().addAll(testRequest.getFailures());
        } catch (Throwable t) {
            final String msg = "Fatal exception when running tests";
            Log.e(LOG_TAG, msg, t);
            junitResults.getFailures().add(new Failure(Description.createSuiteDescription(msg), t));
        } finally {
            ByteArrayOutputStream summaryStream = new ByteArrayOutputStream();
            // create the stream used to output summary data to the user
            PrintStream summaryWriter = new PrintStream(summaryStream);
            reportRunEnded(mListeners, summaryWriter, resultBundle, junitResults);
            summaryWriter.close();
            resultBundle.putString(Instrumentation.REPORT_KEY_STREAMRESULT,
                    String.format("\n%s", summaryStream.toString()));
        }
        return resultBundle;
    }

    /**
     * Initialize listeners and add them to the JUnitCore runner
     */
    private void setUpListeners(JUnitCore testRunner) {
        for (RunListener listener : mListeners) {
            Log.d(LOG_TAG, "Adding listener " + listener.getClass().getName());
            testRunner.addListener(listener);
            if (listener instanceof InstrumentationRunListener) {
                ((InstrumentationRunListener)listener).setInstrumentation(mInstr);
            }
        }
    }

    private void reportRunEnded(List<RunListener> listeners, PrintStream summaryWriter,
                                Bundle resultBundle, Result jUnitResults) {
        for (RunListener listener : listeners) {
            if (listener instanceof InstrumentationRunListener) {
                ((InstrumentationRunListener)listener).instrumentationRunFinished(summaryWriter,
                        resultBundle, jUnitResults);
            }
        }
    }

    public static class Builder {
        private final List<RunListener> mListeners = new ArrayList<RunListener>();
        private boolean mWaitForDebugger = false;
        private final Instrumentation mInstr;

        public Builder(Instrumentation instr) {
            mInstr = instr;
        }

        public Builder setWaitForDebugger(boolean waitForDebugger) {
            mWaitForDebugger = waitForDebugger;
            return this;
        }

        /**
         * Adds a custom RunListener
         * @param listener
         * @return
         */
        public Builder addRunListener(RunListener listener) {
            mListeners.add(listener);
            return this;
        }

        public TestExecutor build() {
            return new TestExecutor(this);
        }
    }
}
