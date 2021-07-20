package android.support.test.runner.intent;


import static android.support.test.internal.util.Checks.checkNotNull;
import static android.support.test.internal.util.Checks.checkState;

import android.os.Looper;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Exposes an implementation of {@link IntentStubber}.
 */
public final class IntentStubberRegistry {

    private static IntentStubber mInstance;

    private static AtomicBoolean mIsLoaded = new AtomicBoolean();

    /**
     * Loads an {@link IntentStubber} into this registry. There can only be one active stubber at a
     * time.
     * <p>
     * Calling this method multiple times in the same instrumentation will result in an
     * exception.
     * <p>
     * This method can be called from any thread.
     */
    public static void load(IntentStubber intentStubber) {
        checkNotNull(intentStubber, "IntentStubber cannot be null!");
        checkState(!mIsLoaded.getAndSet(true),
                "Intent stubber already registered! Multiple stubbers are not"
                        + "allowedAre you running under an ");
        mInstance = intentStubber;
    }

    /**
     * @return if an {@link IntentStubber} has been loaded.
     */
    public static boolean isLoaded() {
        return mIsLoaded.get();
    }

    /**
     * Returns the loaded Intent Stubber mInstance.
     *
     * @throws IllegalStateException if this method is not called on the main thread.
     * @throws IllegalStateException if no Intent Stubber has been loaded.
     */
    public static IntentStubber getInstance() {
        checkMain();
        checkState(null != mInstance, "No intent monitor registered! Are you running under an "
                + "Instrumentation which registers intent monitors?");
        return mInstance;
    }

    private static void checkMain() {
        checkState(Looper.myLooper() == Looper.getMainLooper(), "Must be called on main thread.");
    }

    private IntentStubberRegistry() {
    }

    /**
     * Clears the current instance of Intent Stubber.
     */
    public static synchronized void reset() {
        mInstance = null;
        mIsLoaded.set(false);
    }
}
