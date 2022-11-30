package android.support.test.internal.runner.hidden;

import android.app.Activity;
import android.app.Fragment;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;

/**
 * Exposes select hidden android apis to the compiler and to enable recording and stubbing of
 * intents that pass through the execStartActivity methods.
 * These methods are stripped from the android.jar sdk compile time jar, however are called at
 * runtime (and exist in the android.jar on the device).
 *
 * This class will actually never be included in our .aar!
 */
public class ExposedInstrumentationApi extends Instrumentation {

    public  ActivityResult execStartActivity(
            Context who, IBinder contextThread, IBinder token, Activity target,
            Intent intent, int requestCode) {
        throw new RuntimeException("Stub!");
    }

    public ActivityResult execStartActivity(
            Context who, IBinder contextThread, IBinder token, Activity target,
            Intent intent, int requestCode, Bundle options) {
        throw new RuntimeException("Stub!");
    }

    public void execStartActivities(Context who, IBinder contextThread,
            IBinder token, Activity target, Intent[] intents, Bundle options) {
        throw new RuntimeException("Stub!");
    }

    public ActivityResult execStartActivity(
            Context who, IBinder contextThread, IBinder token, Fragment target,
            Intent intent, int requestCode, Bundle options) {
        throw new RuntimeException("Stub!");
    }
}
