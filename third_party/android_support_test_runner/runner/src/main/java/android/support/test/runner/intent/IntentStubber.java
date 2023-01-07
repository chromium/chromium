package android.support.test.runner.intent;


import android.app.Instrumentation.ActivityResult;
import android.content.Intent;

/**
 * Interface to intercept activity launch for a given {@link android.content.Intent} and stub
 * {@link ActivityResult} its response.
 * <p>
 * Retrieve instances of the stubber through {@link IntentStubberRegistry}
 * <p>
 * Stubbing intents requires support from Instrumentation, therefore do not expect an instance
 * to be present under any arbitrary instrumentation.
 */
public interface IntentStubber {

    /**
     * Returns the first matching stubbed result for the given activity if stubbed result was set
     * by test author. The method searches the list of existing matcher/response pairs in reverse
     * order of which they were entered; i.e. the last stubbing has the highest priority. If no
     * stubbed result matching the given intent is found, {@code null} is returned.
     * <p>
     * Must be called on main thread.
     */
    public ActivityResult getActivityResultForIntent(Intent intent);

}
