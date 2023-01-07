// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.os.Handler;
import android.os.Looper;

import java.util.HashSet;

/**
 * A thread-safe event queue which provides both {@link #add} and {@link #remove} functions with
 * O(log(n)) time complexity, and a {@link #raise} function in the derived class
 * {@link Event.Raisable} to execute all queued callbacks.
 *
 * @param <ParamT> The parameter used in {@link ParameterRunnable} callback.
 */
public class Event<ParamT> {
    /** A runnable with parameter. */
    public static interface ParameterRunnable<ParamT> {
        void run(ParamT p);
    }

    /** A callback with parameter. */
    public static interface ParameterCallback<ReturnT, ParamT> {
        ReturnT run(ParamT p);
    }

    /**
     * An event provider version of {@link Event} implementation, provides {@link #raise} function
     * to execute appended {@link ParameterRunnable}, and {@link #clear} function to clear all
     * appended callbacks.
     */
    public static class Raisable<ParamT> extends Event<ParamT> {
        /** Clears all appended callbacks */
        public void clear() {
            synchronized (mSet) {
                mSet.clear();
            }
        }

        /**
         * Executes all queued {@link ParameterRunnable} with |parameter|, returns an integer of
         * total callbacks executed. Note, if an 'add' function call is executing concurrently
         * with the 'raise' function call, the newly added object may not be executed.
         */
        public int raise(ParamT parameter) {
            Object[] array;
            synchronized (mSet) {
                array = mSet.toArray();
            }
            int count = 0;
            for (Object obj : array) {
                execute(obj, parameter);
                count++;
            }
            return count;
        }

        /** Executes |obj| as ParameterRunnable<ParamT> with |parameter| as ParamT. */
        @SuppressWarnings("unchecked")
        private void execute(Object obj, ParamT parameter) {
            ParameterRunnable<ParamT> runnable = (ParameterRunnable<ParamT>) obj;
            runnable.run(parameter);
        }
    }

    /**
     * A {@link Raisable} which always executes the newly added {@link ParameterRunnable} with the
     * parameter sent to the last {@link #raise} function call. If the event has not been raised,
     * this class has consistent behavior as {@link Raisable}. <br>
     * This class is useful for some one-time events, such as RenderStub.onClientSizeChanged(). A
     * later attached runnable will never be able to get the event. <br>
     * The {@link ParameterRunnable} will be executed in the thread in which {@link #add}
     * function is called if the event has been raised before. If there is not a {@link Looper} on
     * current thread, the runnable will be executed immediately. Otherwise, a task will be posted
     * to current looper. Note that it may be executed twice on different threads with the exactly
     * same parameter.
     */
    public static final class PromisedRaisable<ParamT> extends Raisable<ParamT> {
        private boolean mRaised;
        private ParamT mLastParameter;

        @Override
        public Object add(final ParameterRunnable<ParamT> runnable) {
            Object result = super.add(runnable);
            if (result != null) {
                synchronized (mSet) {
                    if (mRaised) {
                        if (Looper.myLooper() == null) {
                            runnable.run(mLastParameter);
                        } else {
                            // We should always use the latest parameter, otherwise the order of
                            // parameters current runnable gets may not be correct.
                            final PromisedRaisable<ParamT> me = this;
                            Handler h = new Handler(Looper.myLooper());
                            h.post(new Runnable() {
                                @Override
                                public void run() {
                                    runnable.run(me.mLastParameter);
                                }
                            });
                        }
                    }
                }
            }
            return result;
        }

        @Override
        public int raise(ParamT parameter) {
            synchronized (mSet) {
                mRaised = true;
                mLastParameter = parameter;
            }
            return super.raise(parameter);
        }
    }

    /**
     * A self removable {@link ParameterRunner}, uses a boolean {@link ParameterCallback} to decide
     * whether removes self from {@link Event} or not.
     */
    private static final class SelfRemovableParameterRunnable<ParamT>
            implements ParameterRunnable<ParamT> {
        private final ParameterCallback<Boolean, ParamT> mCallback;
        private final Event<ParamT> mOwner;

        // This lock is used to make sure mEvent is correctly set before remove in run function.
        // i.e. mOwner.add and assignment of mEvent need to be atomic.
        private final Object mLock;
        private final Object mEvent;

        private SelfRemovableParameterRunnable(Event<ParamT> owner,
                                               ParameterCallback<Boolean, ParamT> callback) {
            Preconditions.notNull(callback);
            mCallback = callback;
            mOwner = owner;
            mLock = new Object();
            synchronized (mLock) {
                mEvent = mOwner.add(this);
            }
            Preconditions.notNull(mEvent);
        }

        @Override
        public final void run(ParamT p) {
            synchronized (mLock) {
                if (mOwner.contains(mEvent)) {
                    if (!mCallback.run(p)) {
                        // Event.Raisable.clear may be called in a different thread.
                        mOwner.remove(mEvent);
                    }
                }
            }
        }
    }

    protected final HashSet<ParameterRunnable<ParamT>> mSet;

    public Event() {
        mSet = new HashSet<>();
    }

    /**
     * Adds a {@link ParameterRunnable} object into current instance, returns an object which can
     * be used to {@link remove} the runnable from this instance. If |runnable| is null or it has
     * been added to this instance already, this function returns null.
     */
    public Object add(ParameterRunnable<ParamT> runnable) {
        if (runnable == null) {
            return null;
        }
        synchronized (mSet) {
            if (mSet.add(runnable)) {
                return runnable;
            }
        }
        return null;
    }

    /**
     * Adds a self removable {@link ParameterRunnable} object into current instance; the runnable
     * will remove itself from {@link Event} instance when callback returns false.
     */
    public void addSelfRemovable(ParameterCallback<Boolean, ParamT> callback) {
        Preconditions.notNull(callback);

        // SelfRemovableParameterRunner is self-contained, i.e. consumers do not need to have a
        // reference of this instance, but all the logic is in new function.
        new SelfRemovableParameterRunnable<ParamT>(this, callback);
    }

    /**
     * Removes an object that was previously returned by {@link add}. Returns false if the object
     * is not in the event queue, or not returned by {@link add} function.
     */
    public boolean remove(Object obj) {
        synchronized (mSet) {
            return mSet.remove(obj);
        }
    }

    /**
     * Returns whether current instance contains the object that was previously returned by
     * {@link add}.
     */
    public boolean contains(Object obj) {
        synchronized (mSet) {
            return mSet.contains(obj);
        }
    }

    /**
     * Returns the total count of runnables attached to current instance.
     */
    public int size() {
        synchronized (mSet) {
            return mSet.size();
        }
    }

    /**
     * Returns true if there is no runnable attached to current instance.
     */
    public boolean isEmpty() {
        synchronized (mSet) {
            return mSet.isEmpty();
        }
    }
}
