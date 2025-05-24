// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.process_launcher.ChildProcessService;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.app.ChildProcessServiceFactory;

/**
 * Service implementation which calls through to a ChildProcessService that uses the content
 * specific delegate.
 * The [Sandboxed|Privileged]ProcessService0, 1.. etc classes are the subclasses for sandboxed/non
 * sandboxed child processes.
 * The embedding application must declare these service instances in the application section
 * of its AndroidManifest.xml, first with some meta-data describing the services:
 *     <meta-data android:name="org.chromium.content.browser.NUM_[SANDBOXED|PRIVILEGED]_SERVICES"
 *           android:value="N"/>
 *     <meta-data android:name="org.chromium.content.browser.[SANDBOXED|PRIVILEGED]_SERVICES_NAME"
 *           android:value="org.chromium.content.app.[Sandboxed|Privileged]ProcessService"/>
 * and then N entries of the form:
 *     <service android:name="org.chromium.content.app.[Sandboxed|Privileged]ProcessServiceX"
 *              android:process=":[sandboxed|privileged]_processX" />
 */
@NullMarked
public class ContentChildProcessService extends Service {
    private @Nullable ChildProcessService mService;

    @Override
    public void onCreate() {
        super.onCreate();
        mService = ChildProcessServiceFactory.create(this, getApplicationContext());
        mService.onCreate();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        assumeNonNull(mService);
        mService.onDestroy();
        mService = null;
    }

    @Override
    public IBinder onBind(Intent intent) {
        assumeNonNull(mService);
        return mService.onBind(intent);
    }
}
