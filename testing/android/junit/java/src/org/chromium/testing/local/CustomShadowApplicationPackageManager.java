// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import android.content.ComponentName;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowApplicationPackageManager;

/**
 * Uses {@link PackageManager#getPackageInfo()} to retrieve ActivityInfo. This enables registering
 * activities via {@link ShadowPackageManager#addPackage()}.
 */
@Implements(className = "android.app.ApplicationPackageManager")
public class CustomShadowApplicationPackageManager extends ShadowApplicationPackageManager {
    @Implementation
    @Override
    public ActivityInfo getActivityInfo(ComponentName component, int flags)
            throws PackageManager.NameNotFoundException {
        PackageInfo packageInfo = getPackageInfo(component.getPackageName(), flags);
        if (packageInfo.activities != null) {
            for (ActivityInfo activityInfo : packageInfo.activities) {
                if (component.getClassName().equals(activityInfo.targetActivity)) {
                    return activityInfo;
                }
            }
        }
        return super.getActivityInfo(component, flags);
    }
}
