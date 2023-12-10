// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.FileDescriptorInfo;
import org.chromium.base.process_launcher.IChildProcessService;
import org.chromium.content.browser.ChildProcessLauncherHelperImpl;
import org.chromium.content.browser.LauncherThread;

import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.Semaphore;

/** An assortment of static methods used in tests that deal with launching child processes. */
public final class ChildProcessLauncherTestUtils {
    // Do not instanciate, use static methods instead.
    private ChildProcessLauncherTestUtils() {}

    public static void runOnLauncherThreadBlocking(final Runnable runnable) {
        if (LauncherThread.runningOnLauncherThread()) {
            runnable.run();
            return;
        }
        final Semaphore done = new Semaphore(0);
        LauncherThread.post(
                new Runnable() {
                    @Override
                    public void run() {
                        runnable.run();
                        done.release();
                    }
                });
        done.acquireUninterruptibly();
    }

    public static <T> T runOnLauncherAndGetResult(Callable<T> callable) {
        if (LauncherThread.runningOnLauncherThread()) {
            try {
                return callable.call();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
        try {
            FutureTask<T> task = new FutureTask<T>(callable);
            LauncherThread.post(task);
            return task.get();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public static ChildProcessLauncherHelperImpl startForTesting(
            final boolean sandboxed,
            final String[] commandLine,
            final FileDescriptorInfo[] filesToBeMapped,
            final boolean doSetupConnection) {
        return runOnLauncherAndGetResult(
                new Callable<ChildProcessLauncherHelperImpl>() {
                    @Override
                    public ChildProcessLauncherHelperImpl call() {
                        return ChildProcessLauncherHelperImpl.createAndStartForTesting(
                                commandLine,
                                filesToBeMapped,
                                sandboxed,
                                /* reducePriorityOnBackground= */ false,
                                /* canUseWarmUpConnection= */ true,
                                /* binderCallback= */ null,
                                doSetupConnection);
                    }
                });
    }

    public static ChildProcessConnection getConnection(
            final ChildProcessLauncherHelperImpl childProcessLauncher) {
        return runOnLauncherAndGetResult(
                new Callable<ChildProcessConnection>() {
                    @Override
                    public ChildProcessConnection call() {
                        return ((ChildProcessLauncherHelperImpl) childProcessLauncher)
                                .getChildProcessConnection();
                    }
                });
    }

    // Retrieves the PID of the passed in connection on the launcher thread as to not assert.
    public static int getConnectionPid(final ChildProcessConnection connection) {
        return runOnLauncherAndGetResult(
                new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return connection.getPid();
                    }
                });
    }

    // Retrieves the service number of the passed in connection from its service name, or -1 if the
    // service number could not be determined.
    public static int getConnectionServiceNumber(final ChildProcessConnection connection) {
        String serviceName = getConnectionServiceName(connection);
        // The service name ends up with the service number.
        StringBuilder numberString = new StringBuilder();
        for (int i = serviceName.length() - 1; i >= 0; i--) {
            char c = serviceName.charAt(i);
            if (!Character.isDigit(c)) {
                break;
            }
            numberString.append(c);
        }
        try {
            return Integer.decode(numberString.toString());
        } catch (NumberFormatException nfe) {
            return -1;
        }
    }

    // Retrieves the service number of the passed in connection on the launcher thread as to not
    // assert.
    public static String getConnectionServiceName(final ChildProcessConnection connection) {
        return runOnLauncherAndGetResult(
                new Callable<String>() {
                    @Override
                    public String call() {
                        return connection.getServiceName().getClassName();
                    }
                });
    }

    // Retrieves the service of the passed in connection on the launcher thread as to not assert.
    public static IChildProcessService getConnectionService(
            final ChildProcessConnection connection) {
        return runOnLauncherAndGetResult(
                new Callable<IChildProcessService>() {
                    @Override
                    public IChildProcessService call() {
                        return connection.getService();
                    }
                });
    }
}
