// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

/**
 * An {@link ExceptionHandler} is notified of any {@link RuntimeException} happening in the
 * bindings or any of the callbacks.
 */
public interface ExceptionHandler {
    /**
     * Receives a notification that an unhandled {@link RuntimeException} has been thrown in an
     * {@link Interface} implementation or one of the {@link Callbacks} internal classes.
     *
     * Normal implementations should either throw the exception or return whether the connection
     * should be kept alive or terminated.
     */
    public boolean handleException(RuntimeException e);

    /**
     * The default ExceptionHandler, which simply throws the exception upon receiving it. It can
     * also delegate the handling of the exceptions to another instance of ExceptionHandler.
     */
    public static class DefaultExceptionHandler implements ExceptionHandler {
        private ExceptionHandler mDelegate;

        @Override
        public boolean handleException(RuntimeException e) {
            if (mDelegate != null) {
                return mDelegate.handleException(e);
            }
            throw e;
        }

        private DefaultExceptionHandler() {}

        /** Static class that implements the initialization-on-demand holder idiom. */
        private static class LazyHolder {
            static final DefaultExceptionHandler INSTANCE = new DefaultExceptionHandler();
        }

        /** Gets the singleton instance for the DefaultExceptionHandler. */
        public static DefaultExceptionHandler getInstance() {
            return LazyHolder.INSTANCE;
        }

        /** Sets a delegate ExceptionHandler, in case throwing an exception is not desirable. */
        public void setDelegate(ExceptionHandler exceptionHandler) {
            mDelegate = exceptionHandler;
        }
    }
}
