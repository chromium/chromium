package android.support.test.internal.runner.hidden;

import android.app.Instrumentation;

/**
 * This is the ExposedInstrumentationApi class which gets packaged into the aar. The
 * execStartActivity methods will be available through {@link Instrumentation} at runtime and exist
 * in the android.jar on the device.
 */
public abstract class ExposedInstrumentationApi extends Instrumentation {}
