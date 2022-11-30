package android.support.test.internal.runner.listener;

import static android.support.test.internal.util.Checks.checkNotNull;

import android.app.Instrumentation;
import android.support.test.runner.MonitoringInstrumentation;
import org.junit.runner.Description;
import org.junit.runner.notification.RunListener;

/**
 * Ensures that no activities are running when a test method starts and that no activities are still
 * running when it ends.
 */
public class ActivityFinisherRunListener extends RunListener {
    private static final String TAG = "ActivityFinisherRunListener";
    private final Instrumentation mInstrumentation;
    private final MonitoringInstrumentation.ActivityFinisher mActivityFinisher;

    public ActivityFinisherRunListener(Instrumentation instrumentation,
                                       MonitoringInstrumentation.ActivityFinisher finisher) {
        mInstrumentation = checkNotNull(instrumentation);
        mActivityFinisher = checkNotNull(finisher);
    }

    @Override
    public void testStarted(Description description) throws Exception {
        mInstrumentation.runOnMainSync(mActivityFinisher);
    }


    @Override
    public void testFinished(Description description) throws Exception {
        mInstrumentation.runOnMainSync(mActivityFinisher);
    }
}
