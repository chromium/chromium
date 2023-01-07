package android.support.test.internal.runner.lifecycle;

import static android.support.test.internal.util.Checks.checkNotNull;

import android.app.Application;
import android.support.test.runner.lifecycle.ApplicationLifecycleCallback;
import android.support.test.runner.lifecycle.ApplicationLifecycleMonitor;
import android.support.test.runner.lifecycle.ApplicationStage;
import android.util.Log;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Implementation of a ApplicationLifecycleMonitor
 */
public class ApplicationLifecycleMonitorImpl implements ApplicationLifecycleMonitor {

    private static final String TAG = "ApplicationLifecycleMonitorImpl";

    // Accessed from any thread.
    private List<WeakReference<ApplicationLifecycleCallback>> mCallbacks =
            new ArrayList<WeakReference<ApplicationLifecycleCallback>>();

    @Override
    public void addLifecycleCallback(ApplicationLifecycleCallback callback) {
        // there will never be too many callbacks, so iterating over a list will probably
        // be faster then the constant time costs of setting up and maintaining a map.
        checkNotNull(callback);

        synchronized (mCallbacks) {
            boolean needsAdd = true;
            Iterator<WeakReference<ApplicationLifecycleCallback>> refIter = mCallbacks.iterator();
            while (refIter.hasNext()) {
                ApplicationLifecycleCallback storedCallback = refIter.next().get();
                if (null == storedCallback) {
                    refIter.remove();
                } else if (storedCallback == callback) {
                    needsAdd = false;
                }
            }
            if (needsAdd) {
                mCallbacks.add(new WeakReference<ApplicationLifecycleCallback>(callback));
            }
        }
    }

    @Override
    public void removeLifecycleCallback(ApplicationLifecycleCallback callback) {
        checkNotNull(callback);

        synchronized (mCallbacks) {
            Iterator<WeakReference<ApplicationLifecycleCallback>> refIter = mCallbacks.iterator();
            while (refIter.hasNext()) {
                ApplicationLifecycleCallback storedCallback = refIter.next().get();
                if (null == storedCallback) {
                    refIter.remove();
                } else if (storedCallback == callback) {
                    refIter.remove();
                }
            }
        }
    }

    public void signalLifecycleChange(Application app, ApplicationStage stage) {
        synchronized (mCallbacks) {
            Iterator<WeakReference<ApplicationLifecycleCallback>> refIter = mCallbacks.iterator();
            while (refIter.hasNext()) {
                ApplicationLifecycleCallback callback = refIter.next().get();
                if (null == callback) {
                    refIter.remove();
                } else {
                    try {
                        Log.d(TAG, "running callback: " + callback);
                        callback.onApplicationLifecycleChanged(app, stage);
                        Log.d(TAG, "callback completes: " + callback);
                    } catch (RuntimeException re) {
                        Log.e(TAG, String.format(
                                        "Callback threw exception! (callback: %s stage: %s)",
                                        callback,
                                        stage),
                                re);
                    }
                }
            }
        }
    }
}
